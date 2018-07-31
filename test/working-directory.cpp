#include <doctest.h>
#include <reproc/reproc.h>

#include <array>

// NOLINTNEXTLINE(cert-err58-cpp)
TEST_CASE("working-directory")
{
  reproc_type noop;

  std::array<const char *, 2> argv = { { NOOP_PATH, nullptr } };
  auto argc = static_cast<int>(argv.size() - 1);
  const char *working_directory = REPROC_NOOP_DIR;

  REPROC_ERROR error = REPROC_SUCCESS;
  CAPTURE(error);

  error = reproc_start(&noop, argc, argv.data(), working_directory);
  REQUIRE(!error);

  unsigned int exit_status = 0;
  error = reproc_wait(&noop, REPROC_INFINITE, &exit_status);
  REQUIRE(!error);
  REQUIRE((exit_status == 0));

  reproc_destroy(&noop);
}
