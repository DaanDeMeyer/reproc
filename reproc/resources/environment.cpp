// We use cstdio instead of iostream because this program is invoked with a
// custom environment without the parent's PATH which means it cannot find
// libstdc++-6.dll in the PATH when compiling with MinGW causing a crash.
#include <cstdio>

int main(int argc, char *argv[], char *envp[])
{
  (void) argc;
  (void) argv;

  for (size_t i = 0; envp[i] != nullptr; i++) {
    printf("%s", envp[i]); // NOLINT
  }
}
