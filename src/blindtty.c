/* blindtty.c - Modern blindtty implementation
 * Run command in a detached terminal using modern C and sp.h
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "sp.h"

// Signal handler for child termination
static void sigchld_handler(int sig) {
    (void)sig;
    _exit(0);
}

// Signal handler for interrupt
static void sigint_handler(int sig) {
    (void)sig;
    _exit(1);
}

// Usage information
static void usage(const char* progname) {
    SP_LOG("Usage: {} [OPTIONS] CMD [ARG]...", SP_FMT_CSTR(progname));
    SP_LOG("Run a command in a detached terminal for later attachment with retty");
    SP_LOG("");
    SP_LOG("Options:");
    SP_LOG("  -h, --help     Display this help and exit");
    SP_LOG("  -v, --version  Display version information");
    SP_LOG("  -q, --quiet    Quiet mode (suppress output)");
    SP_LOG("");
    SP_LOG("Examples:");
    SP_LOG("  {} bash                    # Start bash in detached terminal", SP_FMT_CSTR(progname));
    SP_LOG("  setsid {} vim file.txt     # Start vim with setsid", SP_FMT_CSTR(progname));
    SP_LOG("");
    SP_LOG("Note: Using 'setsid' is recommended to fully detach from terminal");
}

// Version information
static void version(void) {
    SP_LOG("blindtty version 2.0 (modern C with sp.h)");
    SP_LOG("Part of the retty terminal attachment suite");
}

int main(int argc, char *argv[]) {
    int ptm = -1;
    pid_t pid = -1;
    int quiet = 0;
    int opt;

    // Parse command line options
    // struct option long_options[] = {
    //     {"help", no_argument, 0, 'h'},
    //     {"version", no_argument, 0, 'v'},
    //     {"quiet", no_argument, 0, 'q'},
    //     {0, 0, 0, 0}
    // };

    while ((opt = getopt(argc, argv, "hvq")) != -1) {
        switch (opt) {
            case 'h':
                usage(argv[0]);
                return 0;
            case 'v':
                version();
                return 0;
            case 'q':
                quiet = 1;
                break;
            default:
                usage(argv[0]);
                return 1;
        }
    }

    // Check for command
    if (optind >= argc) {
        if (!quiet) {
            SP_LOG("Error: No command specified");
        }
        usage(argv[0]);
        return 1;
    }

    // Setup signal handlers
    signal(SIGCHLD, sigchld_handler);
    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    // Create pseudo-terminal and fork
    pid = forkpty(&ptm, NULL, NULL, NULL);
    if (pid < 0) {
        if (!quiet) {
            SP_LOG("Error: Failed to fork pseudo-terminal");
        }
        return 1;
    }

    if (pid == 0) {
        // Child process
        // Shift arguments for execvp
        char **child_argv = &argv[optind];

        // Execute the command
        execvp(child_argv[0], child_argv);

        // If we get here, execvp failed
        if (!quiet) {
            SP_LOG("Error: Failed to execute command: {}", SP_FMT_CSTR(child_argv[0]));
        }
        _exit(127); // Standard "command not found" exit code
    }

    // Parent process
    if (!quiet) {
        SP_LOG("Process {} started with PID {}", SP_FMT_CSTR(argv[optind]), SP_FMT_S32(pid));
        SP_LOG("Use 'retty {}' to attach to this process", SP_FMT_S32(pid));
    }

    // Make file descriptor non-blocking for better handling
    int flags = fcntl(ptm, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(ptm, F_SETFL, flags | O_NONBLOCK);
    }

    // Main loop - read from pseudo-terminal to keep it alive
    while (1) {
        char buf[4096];
        ssize_t bytes_read;

        // Try to read from pseudo-terminal
        bytes_read = read(ptm, buf, sizeof(buf));

        if (bytes_read > 0) {
            // Data available - could log it or discard it
            // In quiet mode, we just discard it
            if (!quiet) {
                // Optional: log first few bytes to show it's working
                static int logged = 0;
                if (!logged && bytes_read > 0) {
                    logged = 1;
                    SP_LOG("Pseudo-terminal is active (received {} bytes)", SP_FMT_S32(bytes_read));
                }
            }
        } else if (bytes_read == 0) {
            // EOF - child process closed pseudo-terminal
            if (!quiet) {
                SP_LOG("Child process closed pseudo-terminal");
            }
            break;
        } else if (bytes_read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // No data available, sleep a bit
                usleep(100000); // 100ms

                // Check if child is still alive
                int status;
                pid_t result = waitpid(pid, &status, WNOHANG);
                if (result > 0) {
                    // Child exited
                    if (!quiet) {
                        if (WIFEXITED(status)) {
                            SP_LOG("Child process exited with status {}", SP_FMT_CSTR(WEXITSTATUS(status)));
                        } else if (WIFSIGNALED(status)) {
                            SP_LOG("Child process terminated by signal {}", SP_FMT_CSTR(WTERMSIG(status)));
                        }
                    }
                    break;
                } else if (result < 0) {
                    // Error
                    if (!quiet) {
                        SP_LOG("Error checking child process status");
                    }
                    break;
                }
                // Child still running, continue
                continue;
            } else {
                // Read error
                if (!quiet) {
                    SP_LOG("Error reading from pseudo-terminal");
                }
                break;
            }
        }
    }

    // Cleanup
    if (ptm >= 0) {
        close(ptm);
    }

    if (!quiet) {
        SP_LOG("blindtty exiting");
    }

    return 0;
}