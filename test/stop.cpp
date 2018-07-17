#include <doctest.h>
#include <reproc/reproc.h>

#include <cstdlib>

TEST_CASE("stop")
{
  const char *argv[2] = { INFINITE_PATH, nullptr };
  int argc = 1;

  auto reproc = static_cast<reproc_type *>(malloc(reproc_size()));
  REQUIRE(reproc);

  REPROC_ERROR error = REPROC_SUCCESS;
  CAPTURE(error);

  error = reproc_init(reproc);
  REQUIRE(!error);

  error = reproc_start(reproc, argc, argv, nullptr);
  REQUIRE(!error);

  // Wait 50ms to avoid terminating the child process on Windows before it is
  // initialized (which would result in an error window appearing)
  error = reproc_wait(reproc, 50);
  REQUIRE((error == REPROC_WAIT_TIMEOUT));

  SUBCASE("terminate")
  {
    error = reproc_terminate(reproc, 50);
    REQUIRE(!error);
  }

  SUBCASE("kill")
  {
    error = reproc_kill(reproc, 50);
    REQUIRE(!error);
  }

  error = reproc_destroy(reproc);
  REQUIRE(!error);

  free(reproc);
}
