#pragma once

#include "reproc/reproc.h"

#include <sys/types.h>

REPROC_ERROR wait_no_hang(long long pid, int *exit_status);

REPROC_ERROR wait_infinite(long long pid, int *exit_status);

REPROC_ERROR wait_timeout(long long pid, int *exit_status,
                          unsigned int milliseconds);
