#include <doctest.h>
#include <reproc/reproc.h>

#include <cstdlib>

TEST_CASE("working-directory")
{
  auto reproc = static_cast<reproc_type *>(malloc(reproc_size()));
  REQUIRE(reproc);

  REPROC_ERROR error = REPROC_SUCCESS;
  CAPTURE(error);

  error = reproc_init(reproc);
  REQUIRE(!error);

  int argc = 1;
  const char *argv[2] = { NOOP_PATH, nullptr };
  const char *working_directory = NOOP_DIR;

  error = reproc_start(reproc, argc, argv, working_directory);
  REQUIRE(!error);

  error = reproc_wait(reproc, REPROC_INFINITE);
  REQUIRE(!error);

  int exit_status = 0;
  error = reproc_exit_status(reproc, &exit_status);
  REQUIRE(!error);
  REQUIRE((exit_status == 0));

  error = reproc_destroy(reproc);
  REQUIRE(!error);

  free(reproc);
}
