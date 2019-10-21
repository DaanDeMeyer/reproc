#include <doctest.h>

#include <reproc/reproc.h>

#include <array>

TEST_CASE("working-directory")
{
  reproc_t process;

  REPROC_ERROR error = REPROC_SUCCESS;
  INFO(reproc_error_string(error));

  const char *working_directory = RESOURCE_DIRECTORY;
  std::array<const char *, 2> argv{ RESOURCE_DIRECTORY "/noop", nullptr };

  error = reproc_start(&process, argv.data(), nullptr, working_directory);
  REQUIRE(!error);

  error = reproc_wait(&process, REPROC_INFINITE);
  REQUIRE(!error);
  REQUIRE(reproc_exit_status(&process) == 0);

  reproc_destroy(&process);
}
