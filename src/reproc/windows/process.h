#ifndef REPROC_WINDOWS_PROCESS_H
#define REPROC_WINDOWS_PROCESS_H

#include "reproc/error.h"

#include <wchar.h>
#include <windows.h>

REPROC_ERROR process_create(wchar_t *command_line, wchar_t *working_directory,
                            HANDLE child_stdin, HANDLE child_stdout,
                            HANDLE child_stderr, DWORD *pid, HANDLE *handle);

REPROC_ERROR process_wait(HANDLE process, unsigned int timeout,
                          unsigned int *exit_status);

REPROC_ERROR process_terminate(HANDLE process, unsigned long pid,
                               unsigned int timeout, unsigned int *exit_status);

REPROC_ERROR process_kill(HANDLE process, unsigned int timeout,
                          unsigned int *exit_status);

#endif
