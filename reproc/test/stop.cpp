#include <doctest.h>

#include <reproc/reproc.h>

#include <array>

TEST_CASE("stop")
{
  int r = REPROC_ENOMEM;

  INFO(-r);
  INFO(reproc_strerror(r));

  reproc_t *process = reproc_new();
  REQUIRE(process);

  std::array<const char *, 2> argv{ RESOURCE_DIRECTORY "/stop", nullptr };

  r = reproc_start(process, argv.data(), {});
  REQUIRE(r == 0);

  r = reproc_wait(process, 50);
  REQUIRE(r == REPROC_ETIMEDOUT);

  SUBCASE("terminate")
  {
    r = reproc_terminate(process);
    REQUIRE(r == 0);

    r = reproc_wait(process, 50);
    REQUIRE(r == REPROC_SIGTERM);
  }

  SUBCASE("kill")
  {
    r = reproc_kill(process);
    REQUIRE(r == 0);

    r = reproc_wait(process, 50);
    REQUIRE(r == REPROC_SIGKILL);
  }

  reproc_destroy(process);
}
