#include <doctest.h>

#include <reproc/reproc.h>

#include <array>

TEST_CASE("stop")
{
  reproc_t *process = reproc_new();

  int r = -1;
  INFO(-r);
  INFO(reproc_error_string(r));

  std::array<const char *, 2> argv{ RESOURCE_DIRECTORY "/stop", nullptr };

  r = reproc_start(process, argv.data(), {});
  REQUIRE(r >= 0);

  r = reproc_wait(process, 50);
  REQUIRE(r == REPROC_ERROR_WAIT_TIMEOUT);

  SUBCASE("terminate")
  {
    r = reproc_terminate(process);
    REQUIRE(r >= 0);
  }

  SUBCASE("kill")
  {
    r = reproc_kill(process);
    REQUIRE(r >= 0);
  }

  r = reproc_wait(process, 50);
  REQUIRE(r >= 0);

  reproc_destroy(process);
}
