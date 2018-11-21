#include <reproc/error.h>

#include <errno.h>

unsigned int reproc_system_error(void) { return (unsigned int) errno; }
