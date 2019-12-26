#include <doctest.h>

#include <reproc/reproc.h>
#include <reproc/sink.h>

#include <array>

TEST_CASE("overflow")
{
  reproc_t *process = reproc_new();
  REQUIRE(process);

  int r = -1;
  INFO(-r);
  INFO(reproc_error_string(r));

  std::array<const char *, 2> argv = { RESOURCE_DIRECTORY "/overflow",
                                       nullptr };

  r = reproc_start(process, argv.data(), {});
  REQUIRE(r >= 0);

  char *output = nullptr;
  r = reproc_drain(process, reproc_sink_string, &output);
  REQUIRE(r >= 0);
  REQUIRE(output != nullptr);

  r = reproc_wait(process, REPROC_INFINITE);
  REQUIRE(r >= 0);
  REQUIRE(reproc_exit_status(process) == 0);

  reproc_destroy(process);

  free(output);
}
