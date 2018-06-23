#include <doctest/doctest.h>
#include <process.h>

TEST_CASE("kill")
{
  const char *argv[2] = {INFINITE_PATH, 0};
  int argc = 1;

  Process *process = 0;
  PROCESS_LIB_ERROR error = process_init(&process);
  REQUIRE(!error);
  REQUIRE(process);

  error = process_start(process, argc, argv, 0);
  REQUIRE(!error);

  // Wait 50ms to avoid terminating the process on Windows before it is
  // initialized
  error = process_wait(process, 50);
  REQUIRE((error == PROCESS_LIB_WAIT_TIMEOUT));

  error = process_terminate(process, 50);
  REQUIRE(!error);
}