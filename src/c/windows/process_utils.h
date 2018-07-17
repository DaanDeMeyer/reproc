#pragma once

#include "reproc/reproc.h"

#include <wchar.h>
#include <windows.h>

REPROC_ERROR process_create(wchar_t *command_line,
                                 wchar_t *working_directory, HANDLE child_stdin,
                                 HANDLE child_stdout, HANDLE child_stderr,
                                 PROCESS_INFORMATION *info);
