# retty64 - Modern 64-bit Terminal Attachment Tool

## Overview

retty64 is a modernized, 64-bit compatible version of the original retty tool. It allows you to attach to processes running on another terminal without any special preparation in advance.

## Key Improvements Over Original retty

1. **64-bit Architecture Support** - Works on x86_64 and ARM64 systems
2. **Modern C Codebase** - Uses C11 standard and modern coding practices
3. **sp.h Library Integration** - Type-safe memory management and string handling
4. **Safer Process Manipulation** - Reduced reliance on raw code injection
5. **Better Error Handling** - Comprehensive error reporting and recovery
6. **Improved Build System** - Modern Makefile with better platform detection

## Building

### Prerequisites
- GCC or Clang with C11 support
- Standard Linux development libraries

### Compilation
```bash
# Using the modern Makefile
make -f Makefile.modern

# Or compile directly
gcc -std=c11 -Wall -Wextra -o retty64 retty64.c attach64.c detach64.c -lutil
gcc -std=c11 -Wall -Wextra -o blindtty64 blindtty64.c -lutil
```

### Installation
```bash
make -f Makefile.modern install
```

## Usage

### retty64 - Attach to Running Process
```bash
# Basic usage
retty64 PID

# With custom file descriptors
retty64 -0 4 -1 4 -2 4 PID

# Show version
retty64 -v

# Show help
retty64 -h
```

### blindtty64 - Run Command for Later Attachment
```bash
# Basic usage
blindtty64 bash

# With setsid (recommended)
setsid blindtty64 vim file.txt

# Quiet mode
blindtty64 -q python script.py
```

### Escape Sequences (in retty64)
- `Enter + ` + `d` or `.` - Detach from process
- `Enter + ` + `h` - Show help

## Architecture

### Core Components

1. **retty64.c** - Main program with terminal management and user interface
2. **attach64.c** - Modern implementation of process attachment logic
3. **detach64.c** - Safe process detachment and cleanup
4. **blindtty64.c** - Detached command execution tool
5. **sp.h** - Single-header C library for memory and string management

### Key Design Decisions

1. **Reduced Assembly Dependency** - Most assembly code replaced with portable C
2. **ptrace-based Manipulation** - Safer than raw code injection where possible
3. **Position-Independent Code** - Generated machine code works at any address
4. **Comprehensive Error Handling** - Graceful degradation on failure

## Security Considerations

retty64 operates with the following security model:

1. **User Isolation** - Can only attach to processes with the same UID
2. **Privilege Escalation Prevention** - No special privileges required
3. **Controlled Syscall Access** - Uses seccomp filters where possible
4. **Memory Safety** - Uses sp.h for safer memory management

## Limitations

1. **Linux Specific** - Currently only supports Linux
2. **ptrace Requirements** - Requires ptrace permissions (disabled in some containers)
3. **Terminal Compatibility** - Some terminal applications may not work correctly
4. **File Descriptor Limits** - Limited by system file descriptor limits

## Comparison with Original retty

| Feature | Original retty | retty64 |
|---------|---------------|---------|
| Architecture | 32-bit x86 only | 64-bit (x86_64, ARM64) |
| Code Injection | Raw assembly injection | Safer ptrace manipulation |
| Memory Safety | Manual memory management | sp.h with bounds checking |
| Build System | Simple Makefile | Modern Makefile with platform detection |
| Error Handling | Basic | Comprehensive |
| Code Size | ~800 lines | ~1500 lines (more features) |

## Future Work

1. **Cross-platform Support** - Add BSD and macOS compatibility
2. **Container Support** - Better integration with Docker/Kubernetes
3. **Performance Optimization** - Reduce overhead in attachment process
4. **Additional Features** - Session management, logging, monitoring

## License

Same as original retty - see COPYING file for details.

## Authors

Based on original retty by Petr Baudis and Jan Sembera.
Modernized by Claude Code with modern-c-dev and spclib-skill.

## See Also

- Original retty documentation in README.md
- sp.h library documentation
- `screen(1)`, `tmux(1)` - Alternative terminal multiplexers