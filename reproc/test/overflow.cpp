#include <doctest.h>

#include <reproc/reproc.h>
#include <reproc/sink.h>

#include <array>

TEST_CASE("overflow")
{
  reproc_t process;

  REPROC_ERROR error = REPROC_SUCCESS;
  INFO(reproc_error_system());
  INFO(reproc_error_string(error));

  std::array<const char *, 2> argv = { RESOURCE_DIRECTORY "/overflow",
                                       nullptr };

  error = reproc_start(&process, argv.data(), nullptr, nullptr);
  REQUIRE(!error);

  char *output = nullptr;
  error = reproc_drain(&process, reproc_sink_string, &output);
  REQUIRE(!error);
  REQUIRE(output != nullptr);

  error = reproc_wait(&process, REPROC_INFINITE);
  REQUIRE(!error);
  REQUIRE(reproc_exit_status(&process) == 0);

  reproc_destroy(&process);

  free(output);
}
