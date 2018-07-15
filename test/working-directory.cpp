#include <doctest.h>
#include <process-lib/process.h>

#include <cstdlib>

TEST_CASE("working-directory")
{
  auto process = static_cast<process_type *>(malloc(process_size()));
  REQUIRE(process);

  PROCESS_LIB_ERROR error = PROCESS_LIB_SUCCESS;
  CAPTURE(error);

  error = process_init(process);
  REQUIRE(!error);

  int argc = 1;
  const char *argv[2] = { NOOP_PATH, nullptr };
  const char *working_directory = NOOP_DIR;

  error = process_start(process, argc, argv, working_directory);
  REQUIRE(!error);

  error = process_wait(process, PROCESS_LIB_INFINITE);
  REQUIRE(!error);

  int exit_status = 0;
  error = process_exit_status(process, &exit_status);
  REQUIRE(!error);
  REQUIRE((exit_status == 0));

  error = process_destroy(process);
  REQUIRE(!error);

  free(process);
}
