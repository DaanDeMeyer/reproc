#include <doctest.h>

#include <reproc/reproc.h>
#include <reproc/sink.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

TEST_CASE("working-directory")
{
  reproc_t *process = reproc_new();
  REQUIRE(process);

  REPROC_ERROR error = REPROC_SUCCESS;
  INFO(reproc_error_system());
  INFO(reproc_error_string(error));

  const char *working_directory = RESOURCE_DIRECTORY;
  std::array<const char *, 2> argv{ RESOURCE_DIRECTORY "/working-directory",
                                    nullptr };

  reproc_options options = {};
  options.working_directory = working_directory;

  error = reproc_start(process, argv.data(), options);
  REQUIRE(!error);

  char *output = nullptr;
  error = reproc_drain(process, reproc_sink_string, &output);
  REQUIRE(!error);

  std::replace(output, output + strlen(output), '\\', '/');
  REQUIRE(std::string(output) == RESOURCE_DIRECTORY);

  error = reproc_wait(process, REPROC_INFINITE);
  REQUIRE(!error);
  REQUIRE(reproc_exit_status(process) == 0);

  reproc_destroy(process);

  free(output);
}
