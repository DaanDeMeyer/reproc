#include <doctest.h>
#include <reproc/reproc.h>

#include <array>

// NOLINTNEXTLINE(cert-err58-cpp)
TEST_CASE("working-directory")
{
  reproc_type reproc;

  REPROC_ERROR error = REPROC_SUCCESS;
  CAPTURE(error);

  error = reproc_init(&reproc);
  REQUIRE(!error);

  int argc = 1;
  std::array<const char *, 2> argv = { { NOOP_PATH, nullptr } };
  const char *working_directory = NOOP_DIR;

  error = reproc_start(&reproc, argc, argv.data(), working_directory);
  REQUIRE(!error);

  error = reproc_wait(&reproc, REPROC_INFINITE);
  REQUIRE(!error);

  int exit_status = 0;
  error = reproc_exit_status(&reproc, &exit_status);
  REQUIRE(!error);
  REQUIRE((exit_status == 0));

  error = reproc_destroy(&reproc);
  REQUIRE(!error);
}
