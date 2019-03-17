#include <reproc/error.h>

#include <errno.h>

unsigned int reproc_system_error(void)
{
  // Only positive numbers are valid `errno` values so casting to unsigned int
  // is safe.
  return (unsigned int) errno;
}
