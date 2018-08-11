#include <doctest.h>
#include <reproc/reproc.h>

// NOLINTNEXTLINE(cert-err58-cpp)
TEST_CASE("working-directory")
{
  reproc_type noop;

  static constexpr unsigned int ARGV_SIZE = 2;
  const char *argv[ARGV_SIZE] = { NOOP_PATH, nullptr };
  const char *working_directory = NOOP_DIR;

  REPROC_ERROR error = REPROC_SUCCESS;
  CAPTURE(error);

  error = reproc_start(&noop, ARGV_SIZE - 1, argv, working_directory);
  REQUIRE(!error);

  unsigned int exit_status = 0;
  error = reproc_stop(&noop, REPROC_WAIT, REPROC_INFINITE, &exit_status);
  REQUIRE(!error);
  REQUIRE((exit_status == 0));
}
