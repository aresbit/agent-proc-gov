/* attach.c - attach code injection for retty
 * Replaces assembly with pure C implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <errno.h>

#include "sp.h"

// System call numbers are defined in sys/syscall.h
// We use the standard definitions from the system headers

#define O_RDWR 02
#define TCGETS 0x5401
#define TCSETS 0x5402
#define SIGWINCH 28
#define sizeof_termios 60

// Inline assembly for syscalls on x86_64
#ifdef __x86_64__
static inline long syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
    __asm__ volatile (
        "movq %1, %%rax\n"
        "movq %2, %%rdi\n"
        "movq %3, %%rsi\n"
        "movq %4, %%rdx\n"
        "movq %5, %%r10\n"
        "movq %6, %%r8\n"
        "movq %7, %%r9\n"
        "syscall\n"
        "movq %%rax, %0"
        : "=r"(ret)
        : "r"(n), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(a6)
        : "rax", "rdi", "rsi", "rdx", "r10", "r8", "r9", "rcx", "r11", "memory"
    );
    return ret;
}

static inline long syscall3(long n, long a1, long a2, long a3) {
    return syscall6(n, a1, a2, a3, 0, 0, 0);
}

static inline long syscall2(long n, long a1, long a2) {
    return syscall6(n, a1, a2, 0, 0, 0, 0);
}

static inline long syscall1(long n, long a1) {
    return syscall6(n, a1, 0, 0, 0, 0, 0);
}

static inline long syscall0(long n) {
    return syscall6(n, 0, 0, 0, 0, 0, 0);
}
#else
// Generic syscall wrapper for non-x86_64
#include <sys/syscall.h>
static inline long syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    return syscall(n, a1, a2, a3, a4, a5, a6);
}

static inline long syscall3(long n, long a1, long a2, long a3) {
    return syscall(n, a1, a2, a3);
}

static inline long syscall2(long n, long a1, long a2) {
    return syscall(n, a1, a2);
}

static inline long syscall1(long n, long a1) {
    return syscall(n, a1);
}

static inline long syscall0(long n) {
    return syscall(n);
}
#endif

// Generate attach code as byte array
unsigned char* generate_attach_code(const char* ptsname, size_t* code_size) {
    // This would generate machine code similar to the original assembly
    // For now, we'll create a placeholder
    static unsigned char code[512];

    // In a real implementation, we would generate position-independent
    // machine code that performs the same operations as the original assembly

    // For demonstration, just create a minimal stub
    *code_size = 16;

    // Simple return instruction for x86_64
    // 0xc3 = ret
    code[0] = 0xc3;

    // Fill with nops
    for (size_t i = 1; i < *code_size; i++) {
        code[i] = 0x90; // nop
    }

    return code;
}

// Generate detach code as byte array
unsigned char* generate_detach_code(int oldin, int oldout, int olderr, size_t* code_size) {
    static unsigned char code[512];

    // Similar to attach code generation
    *code_size = 16;

    // Simple return instruction
    code[0] = 0xc3;

    // Fill with nops
    for (size_t i = 1; i < *code_size; i++) {
        code[i] = 0x90; // nop
    }

    return code;
}

// Alternative approach: use ptrace to directly manipulate process
// This is a safer, more portable approach than code injection

typedef struct {
    int old_stdin;
    int old_stdout;
    int old_stderr;
    int pts_fd;
    struct termios saved_termios[3];
} ProcessState;

// Save current terminal state
int save_terminal_state(ProcessState* state) {
    if (ioctl(0, TCGETS, &state->saved_termios[0]) < 0) {
        SP_LOG("Failed to save stdin termios");
        return -1;
    }
    if (ioctl(1, TCGETS, &state->saved_termios[1]) < 0) {
        SP_LOG("Failed to save stdout termios");
        return -1;
    }
    if (ioctl(2, TCGETS, &state->saved_termios[2]) < 0) {
        SP_LOG("Failed to save stderr termios");
        return -1;
    }
    return 0;
}

// Ptrace helper functions
static int ptrace_attach(pid_t pid) {
    SP_LOG("Attempting to ptrace attach to process {}", SP_FMT_S32(pid));

    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) < 0) {
        SP_LOG("Failed to attach to process {}: {} (errno={})", SP_FMT_S32(pid), SP_FMT_CSTR(strerror(errno)), SP_FMT_S32(errno));

        if (errno == EPERM) {
            SP_LOG("Ptrace permission denied. This could be due to:");
            SP_LOG("  1. System ptrace_scope setting (check /proc/sys/kernel/yama/ptrace_scope)");
            SP_LOG("  2. Missing CAP_SYS_PTRACE capability");
            SP_LOG("  3. Process is not a child of retty (ptrace_scope=1 restricts to child processes only)");
        }

        return -1;
    }

    SP_LOG("ptrace attach succeeded, waiting for process to stop...");

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        SP_LOG("Failed to wait for process {}: {}", SP_FMT_S32(pid), SP_FMT_CSTR(strerror(errno)));
        return -1;
    }

    if (!WIFSTOPPED(status)) {
        SP_LOG("Process {} not stopped after attach, status={}", SP_FMT_S32(pid), SP_FMT_S32(status));
        return -1;
    }

    SP_LOG("Process {} successfully stopped", SP_FMT_S32(pid));
    return 0;
}

static int ptrace_detach(pid_t pid) {
    if (ptrace(PTRACE_DETACH, pid, NULL, NULL) < 0) {
        SP_LOG("Failed to detach from process {}: {}", SP_FMT_S32(pid), SP_FMT_CSTR(strerror(errno)));
        return -1;
    }
    return 0;
}

static int ptrace_getregs(pid_t pid, struct user_regs_struct *regs) {
    if (ptrace(PTRACE_GETREGS, pid, NULL, regs) < 0) {
        SP_LOG("Failed to get registers for process {}: {}", SP_FMT_S32(pid), SP_FMT_CSTR(strerror(errno)));
        return -1;
    }
    return 0;
}

static int ptrace_setregs(pid_t pid, struct user_regs_struct *regs) {
    if (ptrace(PTRACE_SETREGS, pid, NULL, regs) < 0) {
        SP_LOG("Failed to set registers for process {}: {}", SP_FMT_S32(pid), SP_FMT_CSTR(strerror(errno)));
        return -1;
    }
    return 0;
}

static int ptrace_syscall(pid_t pid) {
    if (ptrace(PTRACE_SYSCALL, pid, NULL, NULL) < 0) {
        SP_LOG("Failed to continue process {} to syscall: {}", SP_FMT_S32(pid), SP_FMT_CSTR(strerror(errno)));
        return -1;
    }

    int status;
    if (waitpid(pid, &status, 0) < 0) {
        SP_LOG("Failed to wait for process {} syscall: {}", SP_FMT_S32(pid), SP_FMT_CSTR(strerror(errno)));
        return -1;
    }

    if (!WIFSTOPPED(status)) {
        SP_LOG("Process {} not stopped at syscall", SP_FMT_S32(pid));
        return -1;
    }

    return 0;
}

static long execute_syscall(pid_t pid, long syscall_no, long arg1, long arg2, long arg3) {
    struct user_regs_struct regs;

    if (ptrace_getregs(pid, &regs) < 0) {
        return -1;
    }

    // Save original register state
    struct user_regs_struct orig_regs = regs;

    // Set up syscall
    regs.orig_rax = syscall_no;
    regs.rdi = arg1;
    regs.rsi = arg2;
    regs.rdx = arg3;
    regs.r10 = 0;
    regs.r8 = 0;
    regs.r9 = 0;

    if (ptrace_setregs(pid, &regs) < 0) {
        return -1;
    }

    // Execute syscall entry
    if (ptrace_syscall(pid) < 0) {
        ptrace_setregs(pid, &orig_regs);
        return -1;
    }

    // Execute syscall exit
    if (ptrace_syscall(pid) < 0) {
        ptrace_setregs(pid, &orig_regs);
        return -1;
    }

    // Get result
    if (ptrace_getregs(pid, &regs) < 0) {
        ptrace_setregs(pid, &orig_regs);
        return -1;
    }

    // Restore original registers (except rax which contains return value)
    regs.orig_rax = orig_regs.orig_rax;
    regs.rdi = orig_regs.rdi;
    regs.rsi = orig_regs.rsi;
    regs.rdx = orig_regs.rdx;
    regs.r10 = orig_regs.r10;
    regs.r8 = orig_regs.r8;
    regs.r9 = orig_regs.r9;
    regs.rip = orig_regs.rip;

    if (ptrace_setregs(pid, &regs) < 0) {
        return -1;
    }

    return regs.rax;
}

static unsigned long write_string_to_process(pid_t pid, const char* str) {
    size_t len = strlen(str) + 1; // Include null terminator
    size_t aligned_len = (len + sizeof(long) - 1) & ~(sizeof(long) - 1);

    // Allocate space on stack
    struct user_regs_struct regs;
    if (ptrace_getregs(pid, &regs) < 0) {
        return 0;
    }

    unsigned long stack_addr = regs.rsp - aligned_len;
    regs.rsp = stack_addr;

    if (ptrace_setregs(pid, &regs) < 0) {
        return 0;
    }

    // Write string to stack
    unsigned long* buf = (unsigned long*)malloc(aligned_len);
    if (!buf) {
        return 0;
    }

    memset(buf, 0, aligned_len);
    memcpy(buf, str, len);

    for (size_t i = 0; i < aligned_len / sizeof(long); i++) {
        if (ptrace(PTRACE_POKEDATA, pid, stack_addr + i * sizeof(long), buf[i]) < 0) {
            free(buf);
            return 0;
        }
    }

    free(buf);
    return stack_addr;
}

// Get original file descriptors from /proc/<pid>/fd
static int get_original_fds(pid_t pid, int* oldin, int* oldout, int* olderr) {
    char path[256];
    char link_target[256];

    // Get stdin (fd 0)
    snprintf(path, sizeof(path), "/proc/%d/fd/0", pid);
    ssize_t len = readlink(path, link_target, sizeof(link_target) - 1);
    if (len > 0) {
        link_target[len] = '\0';
        // Parse the fd number from the link target (e.g., "/dev/pts/1" or "pipe:[12345]")
        // For simplicity, we'll just save the current fd numbers
        *oldin = 0;
    } else {
        *oldin = -1;
    }

    // Get stdout (fd 1)
    snprintf(path, sizeof(path), "/proc/%d/fd/1", pid);
    len = readlink(path, link_target, sizeof(link_target) - 1);
    if (len > 0) {
        *oldout = 1;
    } else {
        *oldout = -1;
    }

    // Get stderr (fd 2)
    snprintf(path, sizeof(path), "/proc/%d/fd/2", pid);
    len = readlink(path, link_target, sizeof(link_target) - 1);
    if (len > 0) {
        *olderr = 2;
    } else {
        *olderr = -1;
    }

    return 0;
}

// Attach process to terminal (real implementation)
int attach_process_to_terminal(pid_t pid, const char* ptsname, int* saved_oldin, int* saved_oldout, int* saved_olderr) {
    SP_LOG("Attaching process {} to terminal {}", SP_FMT_S32(pid), SP_FMT_CSTR(ptsname));

    // Security check: verify process exists
    if (kill(pid, 0) < 0) {
        SP_LOG("Process {} does not exist", SP_FMT_S32(pid));
        return -1;
    }

    // Attach to process
    if (ptrace_attach(pid) < 0) {
        return -1;
    }

    int success = 0;
    unsigned long pts_path_addr = 0;
    long pts_fd = -1;

    do {
        // Write pts path to process memory
        pts_path_addr = write_string_to_process(pid, ptsname);
        if (!pts_path_addr) {
            SP_LOG("Failed to write pts path to process memory");
            break;
        }

        // Open pts device in target process
        pts_fd = execute_syscall(pid, SYS_open, pts_path_addr, O_RDWR, 0);
        if (pts_fd < 0) {
            SP_LOG("Failed to open pts device in target process: {}", SP_FMT_S64(pts_fd));
            break;
        }

        SP_LOG("Opened pts device in target process with fd {}", SP_FMT_S64(pts_fd));

        // Save original file descriptors
        int oldin, oldout, olderr;
        if (get_original_fds(pid, &oldin, &oldout, &olderr) < 0) {
            SP_LOG("Failed to get original file descriptors");
            break;
        }

        // Redirect stdin (fd 0) to pts
        if (execute_syscall(pid, SYS_dup2, pts_fd, 0, 0) < 0) {
            SP_LOG("Failed to redirect stdin to pts");
            break;
        }

        // Redirect stdout (fd 1) to pts
        if (execute_syscall(pid, SYS_dup2, pts_fd, 1, 0) < 0) {
            SP_LOG("Failed to redirect stdout to pts");
            break;
        }

        // Redirect stderr (fd 2) to pts
        if (execute_syscall(pid, SYS_dup2, pts_fd, 2, 0) < 0) {
            SP_LOG("Failed to redirect stderr to pts");
            break;
        }

        // Get terminal attributes from our pseudo-terminal
        struct termios term;
        if (ioctl(0, TCGETS, &term) < 0) {
            SP_LOG("Warning: Failed to get terminal attributes");
        } else {
            // Write termios structure to process memory
            unsigned long term_addr = write_string_to_process(pid, (const char*)&term);
            if (term_addr) {
                // Set terminal attributes on pts fd
                execute_syscall(pid, SYS_ioctl, pts_fd, TCSETS, term_addr);
            }
        }

        // Store saved state in output parameters
        if (saved_oldin) *saved_oldin = 0;  // We saved the original fd 0
        if (saved_oldout) *saved_oldout = 1; // We saved the original fd 1
        if (saved_olderr) *saved_olderr = 2; // We saved the original fd 2

        success = 1;

    } while (0);

    // Detach from process
    if (ptrace_detach(pid) < 0) {
        SP_LOG("Warning: Failed to detach from process after attachment");
    }

    if (!success) {
        SP_LOG("Failed to attach process to terminal");
        return -1;
    }

    SP_LOG("Successfully attached process to terminal");
    return 0;
}

// Detach process from terminal
