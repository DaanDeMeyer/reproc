#pragma once

#include <windows.h>

struct process {
  PROCESS_INFORMATION info;
  HANDLE stdin;
  HANDLE stdout;
  HANDLE stderr;
  HANDLE child_stdin;
  HANDLE child_stdout;
  HANDLE child_stderr;
};
