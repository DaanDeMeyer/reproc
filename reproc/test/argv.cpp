#include <doctest.h>

#include <reproc/reproc.h>
#include <reproc/sink.h>

#include <array>
#include <iterator>
#include <sstream>

TEST_CASE("argv")
{
  reproc_t process;

  REPROC_ERROR error = REPROC_SUCCESS;
  INFO(reproc_error_string(error));

  std::array<const char *, 4> argv = { RESOURCE_DIRECTORY "/argv",
                                       "\"argument 1\"", "\"argument 2\"",
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

  std::ostringstream concatenated;
  std::copy(argv.begin(), argv.end() - 1,
            std::ostream_iterator<std::string>(concatenated, "$"));

  REQUIRE(output == concatenated.str());

  free(output);
}
