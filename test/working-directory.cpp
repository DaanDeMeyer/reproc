#include <cstdlib>
#include <doctest.h>
#include <process.h>

TEST_CASE("working-directory")
{
  process_type *process = (process_type *) malloc(process_size());
  REQUIRE(process);

  PROCESS_LIB_ERROR error;
  int system_error;
  char *system_error_string;
  CAPTURE(error);
  CAPTURE(system_error);
  CAPTURE(system_error_string);

  error = process_init(process);
  REQUIRE(!error);

  int argc = 1;
  const char *argv[2] = { NOOP_PATH, NULL };
  const char *working_directory = NOOP_DIR;

  error = process_start(process, argc, argv, working_directory);
  system_error = process_system_error();
  process_system_error_string(&system_error_string);
  REQUIRE(!error);

  error = process_wait(process, PROCESS_LIB_INFINITE);
  REQUIRE(!error);

  int exit_status;
  error = process_exit_status(process, &exit_status);
  REQUIRE(!error);
  REQUIRE((exit_status == 0));

  error = process_destroy(process);
  REQUIRE(!error);

  free(process);
}
