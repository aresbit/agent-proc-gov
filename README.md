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

## retty
  是一个终端进程附加工具，其核心功能是将正在其他终端运行的进程"拉"到当前终端，接管其标准输入/输出/错误流。

  retty 的基本工作原理

  1. 目标进程注入 - 通过 ptrace 向目标进程注入代码
  2. 文件描述符重定向 - 将目标进程的 stdin/stdout/stderr 重定向到新创建的伪终端 (pty)
  3. 终端会话接管 - 当前终端成为目标进程的新控制终端
  4. 双向通信 - 用户在当前终端与目标进程交互，就像直接运行它一样

  在 Claude Code 中的潜在用途

  基于项目名称 agent-proc-gov（代理进程治理）和 Claude Code 的工作模式，retty 可能用于：

  1. 代理进程管理 🛠️

  - Claude Code 可能启动多个代理进程处理不同任务
  - retty 用于重新连接和监控这些代理的状态
  - 示例：连接到一个正在执行代码分析的代理进程

  2. 调试辅助 🐛

  - 连接到用户正在调试的进程，无需重启
  - 实时查看进程输出，发送调试命令
  - 示例：retty $(pgrep my_app) 连接到正在运行的应用进行交互式调试

  3. 会话恢复 🔄

  - 恢复因终端断开而"丢失"的进程会话
  - 重新获取对后台进程的控制权
  - 示例：SSH 断开后重新连接到仍在运行的开发服务器

  4. 进程监控 👁️

  - 监控长时间运行进程的输出（构建、测试、服务器等）
  - 无需 tail -f 日志文件，直接查看实时输出
  - 示例：监控一个正在运行的测试套件进度

  5. 多工具集成 🔗

  - Claude Code 可能需要与用户的其他工具交互
  - 连接到编辑器、构建系统、版本控制系统等
  - 示例：连接到正在运行的 vim 或 emacs 会话

  典型使用场景

  # 1. 连接到后台编辑器
  $ ps aux | grep vim
  user     12345  0.0  0.1  ...  vim main.c
  $ retty 12345  # 现在可以在当前终端继续编辑

  # 2. 调试运行中的应用
  $ ./my_server &
  [1] 23456
  $ retty 23456  # 查看服务器输出，发送控制命令

  # 3. 恢复断开的会话
  $ ssh user@server
  $ ./long_task.sh
  # SSH 连接断开...
  $ ssh user@server
  $ retty $(pgrep long_task.sh)  # 重新连接

  技术限制与替代方案
  ┌──────────────┬──────────────────┬────────────────────────────────┐
  │     场景     │    retty 方案    │            替代方案             
  ├──────────────┼──────────────────┼────────────────────────────────┤
  │ 终端多路复用 │ retty + 进程注入   │ screen/tmux（需提前准备）      
  ├──────────────┼──────────────────┼────────────────────────────────┤
  │ 进程调试     │ 实时附加调试     │ gdb attach（更专业的调试）     
  ├──────────────┼──────────────────┼────────────────────────────────┤
  │ 日志监控     │ 直接输出查看     │ tail -f + 日志文件             
  ├──────────────┼──────────────────┼────────────────────────────────┤
  │ 会话管理     │ 动态重新连接     │ 终端多路复用器的 detach/attach 
  └──────────────┴──────────────────┴────────────────────────────────┘
  总结

  retty 为 Claude Code 提供了动态进程连接能力，使其能够：
  - 主动管理用户的其他进程
  - 无缝集成到现有的开发工作流
  - 实时交互与运行中的工具和应用程序
  - 灵活恢复因各种原因"丢失"的终端会话
