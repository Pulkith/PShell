# PShell


##  ource Files:

*   `exec.c`
*   `exec.h`
*   `jobs.c`
*   `jobs.h`
*   `panic.c` 
*   `panic.h` 
*   `Vec.c` 
*   `Vec.h` 
*   `parser.c`
*   `parser.h`
*   `Job.h`  
*   `penn-shell.c` (main entry point)
*   `Makefile`  

## Overview of Work Accomplished:
This code represents the completion of all the code for `pshell`. The core functionality includes the following:

*   **Pipelines:** The shell allows executing pipelines (of commands), connecting the standard input of one command to the standard output of the next. We made sure that parallel execution 
was used of the stages by forking all of the processes before waiting for competion.
*   **Input / Output Redirection:**  Basic input redirection (`<`) and output redirection (`>` and `>>`) are implemented, allowing commands to read from files and write to files as intended.
*   **Process Groups:** Each pipeline (job) is placed in its own process group, which is different from the shell's process group and other job groups.
*   **Non-Interactive Mode:** The shell can run in non-interactive mode (e.g., when commands are piped in from a file). In this mode, the shell does not prompt the user and simply executes the commands in sequence.
*   **Job Control**: In interactive mode, pshell supports both foreground and background jobs. Background jobs are indicated by a trailing & and the shell immediately re-prompts after starting them. Job control builtins (jobs, fg, bg) allow the user to view, resume, or foreground stopped/background jobs.
The shell maintains a job queue (implemented using our Penn-Vec data structure), assigns unique job ids (starting at 1), and prints status messages when a job status changes.
*   **Terminal Control & Signals**: Using tcsetpgrp(3), the shell delegates terminal control to the foreground job. This allows correct handling of signals like SIGINT (Ctrl-C) and SIGTSTP (Ctrl-Z). The shell itself installs custom handlers for these signals so that it never terminates or stops unexpectedly.
* **Extra Credit**: We implemented asynchronous zombie reaping by registering a SIGCHLD handler that calls waitpid with WNOHANG to reap child processes immediately when they change state. Finished job notifications are printed immediately. This is not fully working properly when a task is killed, but most of the other functionality works.

## Code Layout:

Below is the organization:

*   **`exec.c` and `exec.h`:**  These files contain the header and implementation for executing pipelines. The main function of `exec.c` is the `execute_pipeline` function, which handles pipe creation,  forking, redirections, process groups, and waiting for completion, etc...
*   **`parser.c` and `parser.h`:** Given, these are for parsing the inputs.
*   **`job.h`:** Given, represents a job. We added some to help with background and completion status.
*   **`main.c`:** This is the entry point for the file, and supports the rest of the code. It prints the prompt, reading/outputting some of the messages, running the main loop, running the parse, and running the executor for jobs. It also sets up the signals and the async handler for the extra credit.
*   ** `jobs.c` and `jobs.h`:** These files contain the header and implementation for managing jobs. This includes the implementation for the bg, fg, and jobs commands, as well as helpers to update job status and print job status (when it changes)
*   **`panic.c` and `panic.h`:** These are from the penn-vec library, and are used for error handling. We used these along with the penn-vec classes.
*   **`Vec.c` and `Vec.h`:** These are from the penn-vec library, and are used for storing the jobs in a mutable vector. It is used in the penn-shell.c and the jobs.c files.
*   **`Makefile`:**  Given, makes the executable.

We tried to keep the code modular, so we can build on it in the future. We also tried our best for good commenting practices.
