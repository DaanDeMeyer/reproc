#include <doctest.h>

#include <reproc/reproc.h>
#include <reproc/sink.h>

#include <array>
#include <iterator>
#include <sstream>

TEST_CASE("argv")
{
  int r = REPROC_ENOMEM;

  INFO(-r);
  INFO(reproc_strerror(r));

  reproc_t *process = reproc_new();
  REQUIRE(process);

  std::array<const char *, 4> argv = { RESOURCE_DIRECTORY "/argv",
                                       "\"argument 1\"", "\"argument 2\"",
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

  std::ostringstream concatenated;
  std::copy(argv.begin(), argv.end() - 1,
            std::ostream_iterator<std::string>(concatenated, "$"));

  REQUIRE(output == concatenated.str());

  free(output);
}
