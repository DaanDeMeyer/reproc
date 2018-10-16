#include "reproc/error.h"

#include <windows.h>

unsigned int reproc_system_error(void) { return GetLastError(); }
