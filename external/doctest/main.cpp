#define DOCTEST_CONFIG_IMPLEMENT
#define DOCTEST_CONFIG_NO_POSIX_SIGNALS
#include <doctest.h>

#if !defined(DOCTEST_WORKING_DIRECTORY)
#error "DOCTEST_WORKING_DIRECTORY not defined"
#endif

#if defined(_WIN32)
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <iostream>

int main(int argc, char **argv)
{
#if defined(_WIN32)
  int rv = _chdir(DOCTEST_WORKING_DIRECTORY);
#else
  int rv = chdir(DOCTEST_WORKING_DIRECTORY);
#endif

  if (rv == -1) {
    std::cerr << "Failed to change working directory to "
              << DOCTEST_WORKING_DIRECTORY << std::endl;
    return 1;
  }

  return doctest::Context(argc, argv).run();
}
