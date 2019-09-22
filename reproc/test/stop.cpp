#include <doctest.h>

#include <reproc/reproc.h>

#include <array>

TEST_CASE("stop")
{
  reproc_t infinite;

  REPROC_ERROR error = REPROC_SUCCESS;
  INFO(reproc_strerror(error));

  std::array<const char *, 2> argv{ RESOURCE_DIRECTORY "/infinite", nullptr };

  error = reproc_start(&infinite, argv.data(), nullptr, nullptr);
  REQUIRE(!error);

  error = reproc_wait(&infinite, 50);
  REQUIRE(error == REPROC_ERROR_WAIT_TIMEOUT);

  SUBCASE("terminate")
  {
    error = reproc_terminate(&infinite);
    REQUIRE(!error);
  }

  SUBCASE("kill")
  {
    error = reproc_kill(&infinite);
    REQUIRE(!error);
  }

  error = reproc_wait(&infinite, 50);
  REQUIRE(!error);

  reproc_destroy(&infinite);
}
