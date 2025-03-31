#include "jobs.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "parser.h"

/**
 *
 * Find job by id
 *
 * @param job_id
 *
 * @return job*
 *
 */
job* find_job_by_id(jid_t job_id) {
  for (size_t i = 0; i < jobs.length; i++) {
    job* curj = (job*)vec_get(&jobs, i);
    if (curj->id == job_id) {
      return curj;
    }
  }
  return NULL;
}

/**
 *
 * Get current job (last running job)
 *
 * @return job*
 */
job* get_current_job() {
  // First check for stopped jobs
  for (size_t i = jobs.length; i > 0; i--) {
    job* curj = (job*)vec_get(&jobs, i - 1);
    if (curj->is_stopped) {
      return curj;
    }
  }

  for (size_t i = jobs.length; i > 0; i--) {
    job* curj = (job*)vec_get(&jobs, i - 1);
    if (!curj->is_completed) {
      return curj;
    }
  }

  return NULL;
}

/**
 * Check if command is a builtin
 *
 * @param cmd
 *
 * @return bool
 *
 */
bool is_builtin(char* cmd) {
  return (cmd != NULL) && (strcmp(cmd, "bg") == 0 || strcmp(cmd, "fg") == 0 ||
                           strcmp(cmd, "jobs") == 0);
}

/**
 *
 * Helper function to print a command string for a job
 *
 * @param j The job
 *
 *
 */
static void print_job_command(job* j) {
  if (j == NULL) {
    return;
  }

  for (size_t i = 0; i < j->cmd->num_commands; i++) {
    char** args = j->cmd->commands[i];
    for (size_t k = 0; args[k] != NULL; k++) {
      printf("%s", args[k]);
      if (args[k + 1] != NULL) {
        printf(" ");
      }
    }

    if (i < j->cmd->num_commands - 1) {
      printf(" | ");
    }
  }
}

/**
 *
 * Print job status (Finished, running, stopped, etc...)
 *
 * @param j The job
 */
void print_job_status(job* j) {
  if (j == NULL || j->is_completed) {
    return;
  }

  printf("[%lu] ", j->id);
  print_job_command(j);
  printf(" (%s)\n", j->is_stopped ? "stopped" : "running");
}

/**
 *
 * Print job status change
 *
 * @param j The job
 * @param status The status
 *
 */
void print_job_status_change(job* j, const char* status) {
  if (j == NULL) {
    return;
  }

  printf("%s: ", status);
  print_job_command(j);
  printf("\n");
}

/**
 *
 * Helper function to give terminal control to a job
 *
 * @param pgid The process group ID
 *
 * @return bool
 */
static bool give_terminal_control(pid_t pgid) {
  if (!isatty(STDIN_FILENO)) {
    return true;  // Not a terminal, nothing to do
  }

  if (tcsetpgrp(STDIN_FILENO, pgid) < 0) {
    perror("tcsetpgrp");
    return false;
  }
  return true;
}

/**
 * Helper function to check if a job is completed
 *
 * @param j The job
 *
 * @return bool
 */
static bool is_job_completed(job* j) {
  for (size_t i = 0; i < j->num_processes; i++) {
    if (j->pids[i] != -1) {
      return false;
    }
  }
  return true;
}

/**
 *
 * Helper function to wait for a process with specific options
 *
 * @param pid The process ID
 * @param status The status
 * @param options The options
 */
static int wait_for_process(pid_t pid, int* status, int options) {
  pid_t result = waitpid(pid, status, options);
  if (result < 0 && errno != ECHILD) {
    perror("waitpid");
  }
  return result;
}

/**
 *
 * Helper function to continue a job
 *
 * @param j The job
 * @param is_foreground Whether the job is running in foreground
 * */
static bool continue_job(job* j, bool is_foreground) {
  if (killpg(j->pids[0], SIGCONT) < 0) {
    perror("killpg");
    return false;
  }

  j->is_stopped = false;
  if (!is_foreground) {
    j->is_background = true;
    print_job_status_change(j, "Running");
  }

  return true;
}

/**
 *
 *
 *
 */
void jobs_builtin() {
  for (size_t i = 0; i < jobs.length; i++) {
    job* curj = (job*)vec_get(&jobs, i);
    if (!curj->is_completed) {
      print_job_status(curj);
    }
  }
}

/**
 *
 * Send a job to the background
 *
 */
bool bg_builtin(char** args) {
  job* curj = NULL;

  if (args[1] != NULL) {
    char* endptr;
    jid_t job_id = (jid_t)strtol(args[1], &endptr, 10);
    if (*endptr != '\0') {
      fprintf(stderr, "bg: invalid job id: %s\n", args[1]);
      return false;
    }

    curj = find_job_by_id(job_id);
    if (curj == NULL) {
      fprintf(stderr, "bg: no such job: %s\n", args[1]);
      return false;
    }
  } else {
    // Use current job
    curj = get_current_job();
    if (curj == NULL) {
      fprintf(stderr, "bg: no current job\n");
      return false;
    }
  }

  // Check if job running
  if (!curj->is_stopped) {
    fprintf(stderr, "bg: job %lu is already running\n", curj->id);
    return false;
  }

  return continue_job(curj, false);
}

/**
 *
 * Helper function to wait for a job to complete or stop
 *
 * @param j The job
 */
static void wait_for_job(job* j) {
  if (j == NULL) {
    return;
  }

  int status;
  pid_t pid;

  for (size_t i = 0; i < j->num_processes; i++) {
    if (j->pids[i] == -1) {
      continue;  // Skip processes that have already exited
    }

    pid = wait_for_process(j->pids[i], &status, WUNTRACED);
    if (pid < 0) {
      break;
    }

    if (WIFSTOPPED(status)) {
      j->is_stopped = true;
      print_job_status_change(j, "Stopped");
      break;
    }
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      // Process terminated
      j->pids[i] = -1;
      if (is_job_completed(j)) {
        j->is_completed = true;
      }
    }
  }
}

/**
 *
 * Send a job to the foreground
 *
 */
bool fg_builtin(char** args) {
  job* curj = NULL;

  if (args[1] != NULL) {
    char* endptr;
    jid_t job_id = (jid_t)strtol(args[1], &endptr, 10);
    if (*endptr != '\0') {
      fprintf(stderr, "fg: invalid job id: %s\n", args[1]);
      return false;
    }

    curj = find_job_by_id(job_id);
    if (curj == NULL) {
      fprintf(stderr, "fg: no such job: %s\n", args[1]);
      return false;
    }
  } else {
    // Use current job if no job ID provided
    curj = get_current_job();
    if (curj == NULL) {
      fprintf(stderr, "fg: no current job\n");
      return false;
    }
  }

  // Print the command that's being brought to foreground
  print_job_command(curj);
  printf("\n");

  // If the job is stopped, print "Restarting" message
  if (curj->is_stopped) {
    printf("Restarting: ");
    print_job_command(curj);
    printf("\n");
  }

  // Give terminal control to the job
  give_terminal_control(curj->pids[0]);

  // Continue the job if it was stopped
  if (!continue_job(curj, true)) {
    return false;
  }

  // Wait for the job to complete/stop
  wait_for_job(curj);

  // Give terminal control back to the shell
  give_terminal_control(getpgrp());

  return true;
}

/**
 * Execute a builtin functiion (fg, bg, jobs)
 *
 */
bool execute_builtin(char** args) {
  if (args == NULL || args[0] == NULL) {
    return false;
  }

  if (strcmp(args[0], "jobs") == 0) {
    jobs_builtin();
    return true;
  }
  if (strcmp(args[0], "fg") == 0) {
    return fg_builtin(args);
  }
  if (strcmp(args[0], "bg") == 0) {
    return bg_builtin(args);
  }

  return false;
}

/**
 * Free the memory uysed by a job in the vector
 */
void free_job(void* job_ptr) {
  if (!job_ptr) {
    return;
  }

  job* curr_job = (job*)job_ptr;
  if (curr_job->pids) {
    free(curr_job->pids);
    curr_job->pids = NULL;
  }

  // Free command structure if it exists
  if (curr_job->cmd) {
    free(curr_job->cmd);
    curr_job->cmd = NULL;
  }

  free(curr_job);
}

/**
 * Free a job wrapper function
 */
void cleanup_job(job* j) {
  // This function is now just a wrapper around free_job
  // for backwards compatibility
  free_job(j);
}

/**
 *
 * Helper function to update the status of a specific process in a job
 *
 * @param j The job
 * @param process_index The index of the process
 * @param status The status
 */
static void update_process_status(job* j, size_t process_index, int status) {
  if (j == NULL || process_index >= j->num_processes) {
    return;
  }

  if (WIFEXITED(status) || WIFSIGNALED(status)) {
    // Process terminated
    j->pids[process_index] = -1;
  } else if (WIFSTOPPED(status)) {
    // Process stopped
    j->is_stopped = true;
  }
}

/**
 *
 *Helper function to check if all processes in a job are completed
 * @param j The job
 */
static bool check_job_completion(job* j) {
  if (j == NULL) {
    return false;
  }

  return is_job_completed(j);
}

void update_job_status() {
  int status;
  pid_t pid;

  // Use WNOHANG to poll for completed processes without blocking
  while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
    // Find the job containing this pid
    for (size_t i = 0; i < jobs.length; i++) {
      job* curj = (job*)vec_get(&jobs, i);
      bool found_pid = false;

      // Check all processes in the pipeline
      for (size_t k = 0; k < curj->num_processes; k++) {
        if (curj->pids[k] == pid) {
          found_pid = true;
          update_process_status(curj, k, status);
        }
      }

      if (found_pid) {
        if (WIFSTOPPED(status)) {
          print_job_status_change(curj, "Stopped");
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
          if (check_job_completion(curj)) {
            curj->is_completed = true;
            // Only print "Finished" for background jobs
            if (curj->is_background) {
              print_job_status_change(curj, "Finished");
            }
            vec_erase(&jobs, i);
            i--;  // Adjust index since we removed an element
          }
        }
        break;
      }
    }
  }
}
