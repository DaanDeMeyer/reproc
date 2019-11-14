#include <doctest.h>

#include <reproc/reproc.h>
#include <reproc/sink.h>

#include <array>
#include <string>

static void
io(const char *mode, const std::string &input, const std::string &expected)
{
  reproc_t *process = reproc_new();
  REQUIRE(process);

  REPROC_ERROR error = REPROC_SUCCESS;
  INFO(reproc_error_system());
  INFO(reproc_error_string(error));

  std::array<const char *, 3> argv = { RESOURCE_DIRECTORY "/io", mode,
                                       nullptr };

  error = reproc_start(process, argv.data(), {});
  REQUIRE(!error);

  unsigned int size = static_cast<unsigned int>(input.size());

  error = reproc_write(process,
                       reinterpret_cast<const uint8_t *>(input.data()), size);
  REQUIRE(!error);

  reproc_close(process, REPROC_STREAM_IN);

  char *output = nullptr;
  error = reproc_drain(process, reproc_sink_string, &output);
  REQUIRE(!error);
  REQUIRE(output != nullptr);

  REQUIRE_EQ(output, expected);

  error = reproc_wait(process, REPROC_INFINITE);
  REQUIRE(!error);
  REQUIRE(reproc_exit_status(process) == 0);

  reproc_destroy(process);

  free(output);
}

TEST_CASE("io")
{
  std::string message = "reproc stands for REdirected PROCess";

  io("stdout", message, message);
  io("stderr", message, message);
  io("both", message, message + message);
}
