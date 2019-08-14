#include <doctest.h>

#include <reproc/reproc.h>

#include <algorithm>
#include <array>
#include <string>

TEST_CASE("argv")
{
  reproc_t process;

  REPROC_ERROR error = REPROC_SUCCESS;
  INFO(reproc_strerror(error));

  std::array<const char *, 4> argv = { "reproc/resources/argv", "argument 1",
                                       "argument 2", nullptr };

  error = reproc_start(&process, argv.data(), nullptr);
  REQUIRE(!error);

  std::string output;

  static constexpr unsigned int BUFFER_SIZE = 4;
  std::array<uint8_t, BUFFER_SIZE> buffer = {};

  while (true) {
    unsigned int bytes_read = 0;
    error = reproc_read(&process, REPROC_STREAM_OUT, buffer.data(), BUFFER_SIZE,
                        &bytes_read);
    if (error != REPROC_SUCCESS) {
      break;
    }

    output.append(reinterpret_cast<const char *>(buffer.data()), bytes_read);
  }

  REQUIRE(error == REPROC_ERROR_STREAM_CLOSED);
  REQUIRE(std::count(output.begin(), output.end(), '\n') == argv.size() - 1);
}
