#include <doctest.h>
#include <process-lib/process.h>

#include <cstdlib>

TEST_CASE("stop")
{
  const char *argv[2] = { INFINITE_PATH, nullptr };
  int argc = 1;

  auto process = static_cast<process_type *>(malloc(process_size()));
  REQUIRE(process);

  PROCESS_LIB_ERROR error = PROCESS_LIB_SUCCESS;
  CAPTURE(error);

  error = process_init(process);
  REQUIRE(!error);

  error = process_start(process, argc, argv, nullptr);
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

  error = process_destroy(process);
  REQUIRE(!error);

  free(process);
}
