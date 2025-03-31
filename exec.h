#ifndef EXEC_H
#define EXEC_H

#include "parser.h"  // for struct parsed_command

// Function to execute a pipeline based on the parsed_command struct.
void execute_pipeline(struct parsed_command* cmd);

#endif
