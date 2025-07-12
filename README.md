# retty

A simple tool to attach to processes running on another terminal.

## Description

retty is a simple tool which will let you attach to a process currently running on another terminal. Unlike screen, you need to make no special provisions in advance - just get the process' pid and attach it anytime.

## Features

- Attach to any running process by PID
- No advance preparation needed (unlike screen/tmux)
- Escape sequences for detaching
- Support for custom file descriptors

## Building

```bash
make
```

## Usage

```bash
retty [-v] [-h] [-0 fd] [-1 fd] [-2 fd] PID
```

### Options

- `-v`: Display version information
- `-h`: Display usage information  
- `-0 fd`: Specify file descriptor for input (default: 0)
- `-1 fd`: Specify file descriptor for output (default: 1)
- `-2 fd`: Specify file descriptor for error output (default: 2)
- `PID`: Process ID to attach to

### Examples

```bash
# Attach to process with PID 1234
retty 1234

# Attach to SSH process (which uses different file descriptors)
retty -0 4 -1 4 -2 4 1234
```

## Detaching

Use escape sequences to detach:
- Enter + `` ` `` + `h` for help
- Enter + `` ` `` + `d` to detach

## Limitations

- x86-specific and works only on Linux with executable stack
- Some applications may not work if they do I/O on /dev/tty
- Controlling terminal is not switched appropriately
- Must run with same uid as target process

## Tools

- `retty`: Main attachment tool
- `blindtty`: Tool for running processes detached for later attachment

## Authors

Written and maintained by Petr Baudis <pasky@ucw.cz> and Jan Sembera <fis@bofh.cz>.

## See Also

- `blindtty(1)`, `screen(1)`
