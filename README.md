![UnixShell](https://plus.unsplash.com/premium_photo-1685086785054-d047cdc0e525?w=500&auto=format&fit=crop&q=60&ixlib=rb-4.0.3&ixid=M3wxMjA3fDB8MHxzZWFyY2h8MXx8Y29kaW5nJTIwd2FsbHBhcGVyfGVufDB8fDB8fHww)
# UnixShell 

`UnixShell`implements a subset of features of well-known shells, such as bash.

This program does the following:

1. Provides a prompt for running commands
2. Handles blank lines and comments, which are lines beginning with the # character
3. Provides expansion for the variable $$, $!, $?, ${param} in a string.
4. Executes 3 commands exit, cd, and status via code built into the shell.
5. Supports input and output redirection
6. Supports running commands in foreground and background processes
7. Implements custom handlers for 2 signals, SIGINT and SIGTSTP

### Running this program

Makefile included, please use `make` to compile

Run program using `./UnixShell`
