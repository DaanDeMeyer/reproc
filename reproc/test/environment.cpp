#include <doctest.h>

#include <reproc/reproc.h>

#include <array>
#include <iterator>
#include <sstream>

TEST_CASE("environment")
{
  reproc_t process;

  REPROC_ERROR error = REPROC_SUCCESS;
  INFO(reproc_error_string(error));

  std::array<const char *, 2> argv = { RESOURCE_DIRECTORY "/environment",
                                       nullptr };
  std::array<const char *, 3> environment = { "IP=127.0.0.1", "PORT=8080",
                                              nullptr };

  error = reproc_start(&process, argv.data(), environment.data(), nullptr);
  REQUIRE(!error);

  std::string output;

  static constexpr unsigned int BUFFER_SIZE = 1024;
  std::array<uint8_t, BUFFER_SIZE> buffer = {};

  while (true) {
    REPROC_STREAM stream = {};
    unsigned int bytes_read = 0;
    error = reproc_read(&process, &stream, buffer.data(), BUFFER_SIZE,
                        &bytes_read);
    if (error != REPROC_SUCCESS) {
      break;
    }

    REQUIRE(stream == REPROC_STREAM_OUT);
    output.append(reinterpret_cast<const char *>(buffer.data()), bytes_read);
  }

  REQUIRE(error == REPROC_ERROR_STREAM_CLOSED);

  std::ostringstream concatenated;
  std::copy(environment.begin(), environment.end() - 1,
            std::ostream_iterator<const char *>(concatenated, ""));

  REQUIRE(output == concatenated.str());
}
