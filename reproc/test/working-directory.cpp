#include <doctest.h>

#include <reproc/reproc.h>
#include <reproc/sink.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

TEST_CASE("working-directory")
{
  int r = REPROC_ENOMEM;

  INFO(-r);
  INFO(reproc_strerror(r));

  reproc_t *process = reproc_new();
  REQUIRE(process);

  const char *working_directory = RESOURCE_DIRECTORY;
  std::array<const char *, 2> argv{ RESOURCE_DIRECTORY "/working-directory",
                                    nullptr };

  reproc_options options = {};
  options.working_directory = working_directory;

  r = reproc_start(process, argv.data(), options);
  REQUIRE(r == 0);

  char *output = nullptr;
  r = reproc_drain(process, reproc_sink_string, &output);
  REQUIRE(r == 0);

  std::replace(output, output + strlen(output), '\\', '/');
  REQUIRE(std::string(output) == RESOURCE_DIRECTORY);

  r = reproc_wait(process, REPROC_INFINITE);
  REQUIRE(r == 0);

  reproc_destroy(process);

  free(output);
}
