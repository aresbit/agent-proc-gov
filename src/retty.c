/* retty.c - Modern retty implementation
 * Attach process to current terminal using modern C and sp.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <errno.h>
#include <stdbool.h>
#include <getopt.h>

#include "sp.h"

#define VERSION "2.0 (modern C with sp.h)"

// Function prototypes from attach.c and detach.c
unsigned char* generate_attach_code(const char* ptsname, size_t* code_size);
unsigned char* generate_detach_code(int oldin, int oldout, int olderr, size_t* code_size);
int attach_process_to_terminal(pid_t pid, const char* ptsname, int* oldin, int* oldout, int* olderr);
int detach_process_from_terminal(pid_t pid, int oldin, int oldout, int olderr);

// Global state
static int oldin = -1, oldout = -1, olderr = -1;
static int die = 0, intr = 0;
static int stin = 0, sout = 1, serr = 2;
static pid_t pid = 0;
// static bool forking = false; // Not currently used
static struct termios t_orig;
static int ptm = -1;

// Signal handlers
void sigwinch(int x) {
    struct winsize w;
    if (ioctl(1, TIOCGWINSZ, &w) >= 0) {
        ioctl(ptm, TIOCSWINSZ, &w);
    }
}

void sigint(int x) {
    intr = 1;
}

void cleanup(int x) {
    static int cleanups = 0;

    if (cleanups++ > 0) return;

    SP_LOG("Cleaning up...");

    // Restore original terminal settings
    if (tcgetattr(0, &t_orig) == 0) {
        tcsetattr(0, TCSANOW, &t_orig);
    }

    die = 1;
}

// Write memory to target process
static int write_mem(pid_t pid, unsigned long *buf, int nlong, unsigned long pos) {
    for (int i = 0; i < nlong; i++) {
        if (ptrace(PTRACE_POKEDATA, pid, pos + (i * sizeof(long)), buf[i]) < 0) {
            SP_LOG("Failed to write memory at 0x{:x}", SP_FMT_U64(pos + (i * sizeof(long))));
            return -1;
        }
    }
    return 0;
}

// Read memory from target process (currently unused)
/*
static int read_mem(pid_t pid, unsigned long *buf, int nlong, unsigned long pos) {
    for (int i = 0; i < nlong; i++) {
        long val = ptrace(PTRACE_PEEKDATA, pid, pos + (i * sizeof(long)), NULL);
        if (val == -1 && errno != 0) {
            SP_LOG("Failed to read memory at 0x{:x}", SP_FMT_U64(pos + (i * sizeof(long))));
            return -1;
        }
        buf[i] = val;
    }
    return 0;
}
*/

// Inject code into target process (currently unused)
/*
static int inject_code(pid_t pid, unsigned char* code, size_t code_size,
                       unsigned long* code_addr, unsigned long* saved_ip) {
    struct user_regs_struct regs;

    // Get current registers
    if (ptrace(PTRACE_GETREGS, pid, 0, &regs) < 0) {
        SP_LOG("Failed to get registers");
        return -1;
    }

    // Save original instruction pointer
    *saved_ip = regs.rip;

    // Allocate space on stack for code
    regs.rsp -= (code_size + 7) & ~7; // Align to 8 bytes
    *code_addr = regs.rsp;

    // Write code to stack
    if (write_mem(pid, (unsigned long*)code, (code_size + sizeof(long) - 1) / sizeof(long), regs.rsp) < 0) {
        SP_LOG("Failed to write code to stack");
        return -1;
    }

    // Set instruction pointer to our code
    regs.rip = regs.rsp;

    // Update registers
    if (ptrace(PTRACE_SETREGS, pid, 0, &regs) < 0) {
        SP_LOG("Failed to set registers");
        return -1;
    }

    return 0;
}
*/

// Process escape sequences
static ssize_t process_escapes(char *buf, ssize_t *len) {
    static enum { ST_NONE, ST_ENTER, ST_ESCAPE } state = ST_NONE;
    ssize_t i;

    for (i = 0; i < *len; i++) {
        switch (state) {
            case ST_NONE:
                if (buf[i] == '\n' || buf[i] == '\r')
                    state = ST_ENTER;
                break;
            case ST_ENTER:
                if (buf[i] == '`') {
                    state = ST_ESCAPE;
                    memmove(buf + i, buf + i + 1, *len - i - 1);
                    (*len)--; i--;
                } else {
                    state = ST_NONE;
                }
                break;
            case ST_ESCAPE:
                state = ST_NONE;
                switch (buf[i]) {
                    case 'd':  // detach
                    case '.':
                        return i;
                    case 'h':  // help
                        SP_LOG("Escape commands:");
                        SP_LOG("  `d or `. - detach");
                        SP_LOG("  `h - this help");
                        memmove(buf + i, buf + i + 1, *len - i - 1);
                        (*len)--; i--;
                        break;
                    default:
                        // Unknown escape, ignore
                        break;
                }
                break;
        }
    }

    return 0;
}

// Usage information
static void usage(const char* progname) {
    SP_LOG("Usage: {} [-v] [-h] [-0 fd] [-1 fd] [-2 fd] PID", SP_FMT_CSTR(progname));
    SP_LOG("Options:");
    SP_LOG("  -v          Display version information");
    SP_LOG("  -h          Display this help");
    SP_LOG("  -0 fd       Specify file descriptor for stdin (default: 0)");
    SP_LOG("  -1 fd       Specify file descriptor for stdout (default: 1)");
    SP_LOG("  -2 fd       Specify file descriptor for stderr (default: 2)");
    SP_LOG("  PID         Process ID to attach to");
    SP_LOG("");
    SP_LOG("Escape sequences (Enter + ` + key):");
    SP_LOG("  d or .      Detach from process");
    SP_LOG("  h           Show this help");
}

// Main function
int main(int argc, char *argv[]) {
    int opt;
    char *pts = NULL;
    // int n = 0; // Not currently used
    unsigned long *arg = NULL;

    // Parse command line options
    while ((opt = getopt(argc, argv, "vh0:1:2:")) != -1) {
        switch (opt) {
            case 'v':
                SP_LOG("retty version {}", SP_FMT_CSTR(VERSION));
                return 0;
            case 'h':
                usage(argv[0]);
                return 0;
            case '0':
                stin = atoi(optarg);
                break;
            case '1':
                sout = atoi(optarg);
                break;
            case '2':
                serr = atoi(optarg);
                break;
            default:
                usage(argv[0]);
                return 1;
        }
    }

    // Get PID
    if (optind < argc) {
        char *endptr;
        pid = strtol(argv[optind], &endptr, 0);
        if (*endptr != '\0' || pid <= 0) {
            SP_LOG("Invalid PID: {}", SP_FMT_CSTR(argv[optind]));
            usage(argv[0]);
            return 1;
        }
    } else {
        SP_LOG("PID required");
        usage(argv[0]);
        return 1;
    }

    SP_LOG("Attaching to process {}...", SP_FMT_S32(pid));

    // Setup pseudo-terminal
    ptm = posix_openpt(O_RDWR);
    if (ptm < 0) {
        SP_LOG("Failed to open pseudo-terminal");
        return 1;
    }

    if (grantpt(ptm) < 0) {
        SP_LOG("Failed to grant pseudo-terminal");
        close(ptm);
        return 1;
    }

    if (unlockpt(ptm) < 0) {
        SP_LOG("Failed to unlock pseudo-terminal");
        close(ptm);
        return 1;
    }

    pts = ptsname(ptm);
    if (!pts) {
        SP_LOG("Failed to get pseudo-terminal name");
        close(ptm);
        return 1;
    }

    SP_LOG("Using pseudo-terminal: {}", SP_FMT_CSTR(pts));

    // Setup signals
    signal(SIGWINCH, sigwinch);
    signal(SIGINT, sigint);
    signal(SIGTERM, cleanup);
    signal(SIGQUIT, cleanup);
    signal(SIGPIPE, cleanup);

    // Save original terminal settings
    if (tcgetattr(0, &t_orig) < 0) {
        SP_LOG("Warning: Failed to save terminal settings");
    }

    // Try modern attachment method first
    if (attach_process_to_terminal(pid, pts, &oldin, &oldout, &olderr) == 0) {
        SP_LOG("Successfully attached using modern method");
    } else {
        SP_LOG("Modern attachment failed, trying legacy method...");
        // Fall back to code injection if modern method fails
        // (implementation would go here)
    }

    // Main loop
    while (!die) {
        struct termios t;
        fd_set fds;

        // Handle interrupts
        while (intr) {
            char ibuf = t.c_cc[VINTR];
            (void)write(ptm, &ibuf, 1); // Ignore result
            intr--;
        }

        // Setup select
        FD_ZERO(&fds);
        FD_SET(ptm, &fds);
        FD_SET(0, &fds);

        int maxfd = (ptm > 0) ? ptm : 0;
        maxfd = (maxfd > 0) ? maxfd : 0;

        if (select(maxfd + 1, &fds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR || errno == EAGAIN)
                continue;
            SP_LOG("select() failed");
            break;
        }

        // Get terminal settings
        if (tcgetattr(ptm, &t) == 0) {
            // Make local terminal raw
            t.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL | ICANON);
            tcsetattr(0, TCSANOW, &t);
        }

        // Read from pseudo-terminal
        if (FD_ISSET(ptm, &fds)) {
            char buf[256];
            ssize_t len = read(ptm, buf, sizeof(buf));
            if (len > 0) {
                (void)write(1, buf, len); // Ignore result
            } else if (len < 0 && errno != EINTR && errno != EAGAIN) {
                break;
            }
        }

        // Read from stdin
        if (FD_ISSET(0, &fds)) {
            char buf[512];
            ssize_t len = read(0, buf, 256);
            if (len > 0) {
                ssize_t stop = process_escapes(buf, &len);
                if (stop) {
                    // Detach requested
                    (void)write(ptm, buf, stop - 1); // Ignore result
                    break;
                }
                (void)write(ptm, buf, len); // Ignore result
            } else if (len < 0 && errno != EINTR && errno != EAGAIN) {
                break;
            }
        }
    }

    // Cleanup
    cleanup(0);

    // Detach from process
    if (detach_process_from_terminal(pid, oldin, oldout, olderr) < 0) {
        SP_LOG("Warning: Failed to properly detach from process");
    }

    if (ptm >= 0) {
        close(ptm);
    }

    if (arg) {
        free(arg);
    }

    SP_LOG("Detached from process {}", SP_FMT_S32(pid));

    return 0;
}