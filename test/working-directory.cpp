#include <doctest.h>
#include <process.hpp>

TEST_CASE("working-directory")
{
  Process process;

  int argc = 1;
  const char *argv[2] = { NOOP_PATH, NULL };
  const char *working_directory = NOOP_DIR;

  Process::Error error;

  error = process.start(argc, argv, working_directory);
  REQUIRE(!error);

  error = process.wait(Process::INFINITE);
  REQUIRE(!error);
}
