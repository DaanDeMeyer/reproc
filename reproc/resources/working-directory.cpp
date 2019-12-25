#include <cstdio>

#if defined(_WIN32)
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

int main()
{
  char working_directory[8096]; // NOLINT

  if (getcwd(working_directory, sizeof(working_directory)) == nullptr) {
    return 1;
  }

  printf("%s", working_directory); // NOLINT

  return 0;
}
