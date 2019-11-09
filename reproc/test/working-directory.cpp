#include <doctest.h>

#include <reproc/reproc.h>

#include <array>

TEST_CASE("working-directory")
{
  reproc_t process;

  REPROC_ERROR error = REPROC_SUCCESS;
  INFO(reproc_error_system());
  INFO(reproc_error_string(error));

  const char *working_directory = RESOURCE_DIRECTORY;
  std::array<const char *, 2> argv{ RESOURCE_DIRECTORY "/noop", nullptr };

  reproc_options options = {};
  options.working_directory = working_directory;

  error = reproc_start(&process, argv.data(), options);
  REQUIRE(!error);

  error = reproc_wait(&process, REPROC_INFINITE);
  REQUIRE(!error);
  REQUIRE(reproc_exit_status(&process) == 0);

  reproc_destroy(&process);
}
