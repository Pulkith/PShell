#define _GNU_SOURCE
#define MAGIC_NUMBER 0644
#include "exec.h"
#include "Job.h"
#include "Vec.h"
#include "jobs.h"

#include <fcntl.h>  // for flags
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

/**
 * Creates pipes for communication.
 *
 * @param pipefds Array to store pipe descriptors.
 * @param num_pipes Number of pipes to create.
 */
static void create_pipes(int pipefds[], size_t num_pipes) {
  for (int i = 0; i < num_pipes; i++) {
    ptrdiff_t offset = (ptrdiff_t)i * 2;
    if (pipe2(pipefds + offset, __O_CLOEXEC) < 0) {
      perror("pipe");
      exit(EXIT_FAILURE);
    }
  }
}

/**
 * Handles input redirection for a child.
 *
 * @param cmd Parsed command.
 * @param command_index Index of the current command (loop) in the pipeline.
 * @param pipefds Array of pipe descriptors.
 */
static void handle_child_input_redirection(struct parsed_command* cmd,
                                           int command_index,
                                           int pipefds[]) {
  ptrdiff_t offset = (ptrdiff_t)command_index * 2;

  // For commands after the first, set stdin to read from the previous pipe.
  if (command_index > 0) {
    if (dup2(pipefds[offset - 2], STDIN_FILENO) < 0) {
      perror("dup2 (stdin)");
      exit(EXIT_FAILURE);
    }
  } else if (cmd->stdin_file != NULL) {
    // For the first command, if input redirection is requested, open the file.
    int fd_in = open(cmd->stdin_file, O_RDONLY);
    if (fd_in < 0) {
      perror("open (stdin redirection)");
      exit(EXIT_FAILURE);
    }
    if (dup2(fd_in, STDIN_FILENO) < 0) {
      perror("dup2 (stdin redirection)");
      exit(EXIT_FAILURE);
    }
    close(fd_in);
  }
}

/**
 * Handles output redirection for a child.
 *
 * @param cmd Parsed command.
 * @param command_index Index in pipeline.
 * @param pipefds Array of pipe descriptors.
 */
static void handle_child_output_redirection(struct parsed_command* cmd,
                                            int command_index,
                                            int pipefds[]) {
  size_t num_cmds = cmd->num_commands;
  ptrdiff_t offset = (ptrdiff_t)command_index * 2;

  // For commands before the last, set stdout to write to the next pipe.
  if (command_index < num_cmds - 1) {
    if (dup2(pipefds[offset + 1], STDOUT_FILENO) < 0) {
      perror("dup2 (stdout)");
      exit(EXIT_FAILURE);
    }
  } else if (cmd->stdout_file != NULL) {
    // For the last command, handle output redirection.
    int flags = O_WRONLY | O_CREAT;
    if (cmd->is_file_append) {
      flags |= O_APPEND;
    } else {
      flags |= O_TRUNC;
    }

    int fd_out = open(cmd->stdout_file, flags, MAGIC_NUMBER);
    if (fd_out < 0) {
      perror("open (stdout redirection)");
      exit(EXIT_FAILURE);
    }
    if (dup2(fd_out, STDOUT_FILENO) < 0) {
      perror("dup2 (stdout redirection)");
      exit(EXIT_FAILURE);
    }
    close(fd_out);
  }
}

/**
 * Executes a command in pipeline within child.
 *
 * @param cmd Parsed command.
 * @param command_index Index of the command to execute.
 * @param pipefds Array of pipe descriptors.
 */
static void execute_command_stage(struct parsed_command* cmd,
                                  int command_index,
                                  int pipefds[]) {
  handle_child_input_redirection(cmd, command_index, pipefds);
  handle_child_output_redirection(cmd, command_index, pipefds);

  unsigned long num_pipes = (cmd->num_commands > 1 ? cmd->num_commands - 1 : 0);
  for (unsigned long i = 0; i < 2 * num_pipes; i++) {
    close(pipefds[i]);
  }

  // Execute
  char** command_args = cmd->commands[command_index];
  execvp(command_args[0], command_args);

  // error occured
  perror("execvp");
  exit(EXIT_FAILURE);
}

/**
 * Closes all pipe in the parent process.
 *
 * @param pipefds Array of pipe descriptors.
 * @param num_pipes Number of pipes.
 */
static void close_pipes_parent(int pipefds[], size_t num_pipes) {
  for (int i = 0; i < 2 * num_pipes; i++) {
    close(pipefds[i]);
  }
}

/**
 * Waits for all childs in the pipeline to complete.
 *
 * @param num_cmds Number of commands in the pipeline.
 * @param pids Array of pids to wait for
 */
static void wait_for_pipeline_completion(size_t num_cmds,
                                         pid_t* pids,
                                         job* job) {
  int status;
  bool job_stopped = false;

  for (int i = 0; i < num_cmds; i++) {
    pid_t wait_result = waitpid(pids[i], &status, WUNTRACED);

    if (wait_result < 0) {
      perror("waitpid");
      continue;
    }

    // If a process was stopped, mark it and continue waiting for other
    // processes
    if (WIFSTOPPED(status)) {
      printf("\n");
      job_stopped = true;
      job->is_stopped = true;

      // Continue waiting for other processes in the pipeline to also stop
      for (int j = i + 1; j < num_cmds; j++) {
        wait_result = waitpid(pids[j], &status, WUNTRACED);
        if (wait_result < 0) {
          perror("waitpid");
          continue;
        }
      }
      break;
    }
  }

  // If any process in the pipeline was stopped, stop the entire job group
  if (job_stopped) {
    killpg(pids[0], SIGTSTP);
    print_job_status_change(job, "Stopped");
  }
}

/**
 * Executes a pipeline of commands.
 *
 * Sets up necessary pipes for multiple commands, handles standard input
 * redirection, and standard output redirection.
 *  Forks a child for each part of the pipeline.
 *
 * @param cmd Parsed command for the pipeline.
 */
void execute_pipeline(struct parsed_command* cmd) {
  size_t num_cmds = cmd->num_commands;
  pid_t shell_pgid = getpgrp();

  // Create new job
  job* new_job = calloc(1, sizeof(job));
  new_job->id = jobs.length + 1;
  new_job->cmd = cmd;
  new_job->pids = calloc(num_cmds, sizeof(pid_t));
  new_job->is_background = cmd->is_background;
  new_job->num_processes = num_cmds;
  new_job->is_completed = false;
  new_job->is_stopped = false;

  // If there is more than one command, we need (num_cmds - 1) pipes.
  size_t num_pipes = (num_cmds > 1 ? num_cmds - 1 : 0);
  int pipefds[2 * (num_pipes ? num_pipes : 1)];  // each pipe has two fds

  if (num_pipes > 0) {
    create_pipes(pipefds, num_pipes);
  }

  // Fork processes
  for (int i = 0; i < num_cmds; i++) {
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      exit(EXIT_FAILURE);
    }

    if (pid == 0) {  // Child process
      // Reset signal handlers to default in child
      struct sigaction sar;
      sar.sa_flags = 0;
      sar.sa_mask = (sigset_t){0};
      sar.sa_handler = SIG_DFL;
      sigaction(SIGINT, &sar, NULL);
      sigaction(SIGTSTP, &sar, NULL);
      sigaction(SIGTTOU, &sar, NULL);
      sigaction(SIGTTIN, &sar, NULL);

      execute_command_stage(cmd, i, pipefds);
      exit(EXIT_FAILURE);
    } else {
      // Parent process
      new_job->pids[i] = pid;

      // Set process group for first process
      if (i == 0) {
        setpgid(pid, pid);
      } else {
        setpgid(pid, new_job->pids[0]);
      }
    }
  }

  // Add job to jobs list
  vec_push_back(&jobs, new_job);

  // Give terminal control to foreground job
  if (!cmd->is_background && isatty(STDIN_FILENO)) {
    tcsetpgrp(STDIN_FILENO, new_job->pids[0]);
  }

  if (num_pipes > 0) {
    close_pipes_parent(pipefds, num_pipes);
  }

  // Wait for completion if foreground job
  if (!cmd->is_background) {
    wait_for_pipeline_completion(num_cmds, new_job->pids, new_job);

    // Check if job is stopped
    if (new_job->is_stopped) {
      // Return control to shell but keep job in list
      if (isatty(STDIN_FILENO)) {
        tcsetpgrp(STDIN_FILENO, shell_pgid);
      }
      return;
    }

    // Return terminal control to shell
    if (isatty(STDIN_FILENO)) {
      tcsetpgrp(STDIN_FILENO, shell_pgid);
    }

    // If completed successfully and not stopped, mark as complete and remove
    // from jobs
    if (!new_job->is_stopped) {
      new_job->is_completed = true;
      for (size_t i = 0; i < jobs.length; i++) {
        job* curj = (job*)vec_get(&jobs, i);
        if (curj->id == new_job->id) {
          vec_erase(&jobs, i);  // Let free_job handle cleanup
          break;
        }
      }
    }
  } else {
    printf("Running: ");
    print_parsed_command(cmd);
  }
}
