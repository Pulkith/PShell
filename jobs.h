#ifndef JOBS_H
#define JOBS_H

#include <stdbool.h>
#include "Job.h"
#include "Vec.h"

// Global jobs vector
extern Vec jobs;

// Job lookup functions
job* find_job_by_id(jid_t job_id);
job* get_current_job();

// Command type checking
bool is_builtin(char* cmd);
bool execute_builtin(char** args);

// Job status and printing functions
void print_job_status(job* j);
void print_job_status_change(job* j, const char* status);

// Built-in command implementations
void jobs_builtin();
bool fg_builtin(char** args);
bool bg_builtin(char** args);

// Job cleanup and management
void cleanup_job(job* j);
void update_job_status();

#endif  // JOBS_H
