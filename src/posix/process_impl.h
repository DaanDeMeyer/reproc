#pragma once

#include <sys/types.h>

struct process {
  pid_t pid;
  int stdin;
  int stdout;
  int stderr;
  int child_stdin;
  int child_stdout;
  int child_stderr;
  int exit_status;
};
