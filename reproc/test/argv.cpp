#include <doctest/doctest.h>
#include <reproc/reproc.h>

#include <array>
#include <string>
#include <algorithm>
#include <iostream>

TEST_SUITE("reproc")
{
  TEST_CASE("argv")
  {
    reproc_t process;

    std::array<const char *, 4> argv = { "reproc/resources/argv", "argument 1",
                                         "argument 2", nullptr };

    REPROC_ERROR error = reproc_start(&process, argv.size() - 1, argv.data(),
                                      nullptr);
    REQUIRE(!error);

    std::string output;
    std::array<uint8_t, 1024> buffer = {};

    while (true) {
      unsigned int bytes_read = 0;
      error = reproc_read(&process, REPROC_STREAM_OUT, buffer.data(),
                          buffer.size(), &bytes_read);
      if (error != REPROC_SUCCESS) {
        break;
      }

      output.append(reinterpret_cast<const char *>(buffer.data()), bytes_read);
    }

    std::cout << output << std::endl;

    REQUIRE(error == REPROC_ERROR_STREAM_CLOSED);
    REQUIRE(std::count(output.begin(), output.end(), '\n') == argv.size() - 1);
  }
}
