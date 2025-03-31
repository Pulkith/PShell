#ifndef JOB_H_
#define JOB_H_

#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "./parser.h"

// define new type "job id"
typedef uint64_t jid_t;

// Represents a job
typedef struct job_st {
  uint64_t id;
  struct parsed_command* cmd;
  pid_t* pids;
  bool is_background;
  bool is_completed;
  bool is_stopped;
  size_t num_processes;
} job;

// Function to properly free a job structure and its contents
void free_job(void* job_ptr);

#endif  // JOB_H_
