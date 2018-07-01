#include <cstdlib>
#include <doctest.h>
#include <process.h>

TEST_CASE("stop")
{
  const char *argv[2] = { INFINITE_PATH, 0 };
  int argc = 1;

  process_type *process = (process_type *) malloc(process_size());
  REQUIRE(process);

  PROCESS_LIB_ERROR error = process_init(process);
  REQUIRE(!error);

  error = process_start(process, argv, argc, 0);
  REQUIRE(!error);

  // Wait 50ms to avoid terminating the process on Windows before it is
  // initialized (which would result in an error window appearing)
  error = process_wait(process, 50);
  REQUIRE((error == PROCESS_LIB_WAIT_TIMEOUT));

  SUBCASE("terminate")
  {
    error = process_terminate(process, 50);
    REQUIRE(!error);
  }

  SUBCASE("kill")
  {
    error = process_kill(process, 50);
    REQUIRE(!error);
  }

  error = process_free(process);
  REQUIRE(!error);

  free(process);
}
