# Custom Shell

A simple Unix-like shell implemented in C for fun and learning.

## Features

- Command prompt with current working directory and PID
- Built-in commands:
  - `cd <path>`: Change directory
  - `exit`: Exit the shell
  - `!history`: Show command history
  - `#<n>`: Execute the nth command from history
  - `!<prefix>`: Execute the most recent command starting with `<prefix>`
  - `ps`: Show background jobs and shell process info
  - `kill <pid>`, `stop <pid>`, `cont <pid>`: Manage processes
- External command execution (foreground and background with `&`)
- Logical operators: `&&`, `||`, `;`
- Input/output redirection: `>`, `>>`, `<`
- Script execution with `-f <filename>`
- Command history loading/saving with `-h <filename>`
- Basic signal handling (e.g., `SIGINT`)
