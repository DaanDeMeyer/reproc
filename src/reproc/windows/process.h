#ifndef REPROC_WINDOWS_PROCESS_H
#define REPROC_WINDOWS_PROCESS_H

#include "reproc/error.h"

#include <wchar.h>
#include <windows.h>

struct process_options {
  wchar_t *working_directory;
  HANDLE stdin_handle;
  HANDLE stdout_handle;
  HANDLE stderr_handle;
};

REPROC_ERROR process_create(wchar_t *command_line,
                            struct process_options *options, DWORD *pid,
                            HANDLE *handle);

REPROC_ERROR process_wait(HANDLE process, unsigned int timeout,
                          unsigned int *exit_status);

REPROC_ERROR process_terminate(HANDLE process, unsigned long pid);

REPROC_ERROR process_kill(HANDLE process);

#endif
