#ifndef REPROC_C_WINDOWS_PROCESS_UTILS_H
#define REPROC_C_WINDOWS_PROCESS_UTILS_H

#include "reproc/error.h"

#include <wchar.h>
#include <windows.h>

REPROC_ERROR process_create(wchar_t *command_line, wchar_t *working_directory,
                            HANDLE child_stdin, HANDLE child_stdout,
                            HANDLE child_stderr, DWORD *pid, HANDLE *handle);

#endif
