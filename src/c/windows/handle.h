#pragma once

#include "process.h"

#include <windows.h>

PROCESS_LIB_ERROR handle_close(HANDLE *handle_address);

PROCESS_LIB_ERROR
handle_inherit_list_create(HANDLE *handles, int amount,
                           LPPROC_THREAD_ATTRIBUTE_LIST *result);
