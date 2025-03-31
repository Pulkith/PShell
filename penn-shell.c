#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "Job.h"
#include "Vec.h"
#include "exec.h"
#include "jobs.h"
#include "parser.h"

#ifndef PROMPT
#define PROMPT "penn-shell# "
#endif

/**
 * Helper function to check if there are any foreground jobs
 *
 * @return true if there are any foreground jobs, false otherwise
 */
static bool has_foreground_jobs() {
  for (size_t i = 0; i < jobs.length; i++) {
    job* curj = (job*)vec_get(&jobs, i);
    if (!curj->is_background && !curj->is_completed && !curj->is_stopped) {
      return true;
    }
  }
  return false;
}

/**
 *
 * Handle signals for the shell.
 *
 * @param signo signal number
 *
 */
void handle_signal(int signo) {
  // Only print prompt if there are no foreground jobs
  if ((signo == SIGINT || signo == SIGTSTP) && !has_foreground_jobs()) {
    write(STDOUT_FILENO, "\n", 1);
    write(STDOUT_FILENO, PROMPT,
          sizeof(PROMPT) - 1);  // -1 to exclude null terminator
  }
}

static bool async_mode = false;

/**
 * Async signal handler for SIGCHLD for the extra credit part.
 *
 * @param signo signal number
 *
 */
static void async_sigchld_handler(int signo) {
  int status;
  pid_t pid;
  while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
    for (size_t i = 0; i < jobs.length; i++) {
      job* curj = (job*)vec_get(&jobs, i);
      bool found = false;
      bool all_done = true;

      for (size_t j = 0; j < curj->num_processes; j++) {
        if (curj->pids[j] == pid) {
          curj->pids[j] = -1;
          found = true;
        }
        if (curj->pids[j] != -1) {
          all_done = false;
        }
      }

      if (found) {
        if (WIFSTOPPED(status)) {
          curj->is_stopped = true;
          write(STDOUT_FILENO, "\n", 1);
          print_job_status_change(curj, "Stopped");
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
          if (all_done) {
            curj->is_completed = true;
            if (curj->is_background) {
              printf("\n");
              print_job_status_change(curj, "Finished");
              write(STDOUT_FILENO, "\n", 1);

              if (isatty(STDIN_FILENO)) {
                write(STDOUT_FILENO, PROMPT, strlen(PROMPT));
              }
            }
            vec_erase(&jobs, i);
            i--;  // Adjust index after removal.
          }
        }
        break;
      }
    }
  }
}

/**
 * Set up signal handlers for required signals.
 *
 * @return void
 */
void setup_handlers() {
  // Ignore SIGTTOU and SIGTTIN to prevent shell from stopping
  struct sigaction sar;
  sar.sa_flags = SA_RESTART;
  sigemptyset(&sar.sa_mask);

  // Ignore SIGTTOU and SIGTTIN
  sar.sa_handler = SIG_IGN;
  sigaction(SIGTTOU, &sar, NULL);
  sigaction(SIGTTIN, &sar, NULL);

  if (async_mode) {
    struct sigaction sa_chld;
    sa_chld.sa_flags = SA_RESTART;
    sigemptyset(&sa_chld.sa_mask);
    sa_chld.sa_handler = async_sigchld_handler;
    if (sigaction(SIGCHLD, &sa_chld, NULL) < 0) {
      perror("sigaction (SIGCHLD)");
      exit(EXIT_FAILURE);
    }
  }

  // Handle SIGINT and SIGTSTP to keep shell running (but propagate them to
  // childs)
  sar.sa_handler = handle_signal;
  sigaction(SIGINT, &sar, NULL);
  sigaction(SIGTSTP, &sar, NULL);
}

/**
 * Helper function to check if a job status has changed and print the appropiate
 * message
 *
 */

static void check_background_jobs() {
  update_job_status();
}

/**
 * Main entry point for the penn-shell program.
 *
 * Startsshell, enters the main loop to read / execute commands,
 * and allows for both interactive and non-interactive modes.
 * Also sets up a signal handler for SIGINT.
 *
 * @param argc Argument count
 * @param argv Argument vector
 * @return int Exit status
 */
// Global jobs vector definition
Vec jobs;

// Initialize jobs vector in main
int main(int argc, char* argv[]) {
  char* line = NULL;
  size_t len = 0;
  struct parsed_command* cmd = NULL;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--async") == 0) {
      async_mode = true;
      break;
    }
  }

  // Initialize jobs vector with proper cleanup function
  jobs = vec_new(10, free_job);

  // Set up signal handlers
  setup_handlers();

  // Main interactive loop
  while (1) {
    // If standard input is a terminal, print the prompt
    if (isatty(STDIN_FILENO)) {
      printf(PROMPT);
    }

    // Read a line from standard input
    if (getline(&line, &len, stdin) == -1) {
      // End-of-file (Ctrl-D at beginning of line) -> exit
      break;
    }

    if (!async_mode) {
      check_background_jobs();
    }

    // Parse the command line using the provided parser
    int parse_err = parse_command(line, &cmd);
    if (parse_err != 0) {
      // Report parsing error
      print_parser_errcode(stderr, parse_err);
      fprintf(stderr, "Parsing error: invalid\n");
      continue;
    }

    //  check if a command is a builtin
    if (cmd && cmd->num_commands > 0) {
      char** first_command = cmd->commands[0];
      if (is_builtin(first_command[0])) {
        execute_builtin(first_command);
        free(cmd);  // Free command only for builtins
        cmd = NULL;
      } else {
        execute_pipeline(cmd);
        // Don't free cmd here
        cmd = NULL;
      }
    } else if (cmd) {
      free(cmd);  // Free empty commands
      cmd = NULL;
    }
  }

  // Clean up jobs vector before exit
  vec_destroy(&jobs);
  free(line);
  return 0;
}
