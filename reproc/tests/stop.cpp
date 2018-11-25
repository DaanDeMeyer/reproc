#include <doctest.h>
#include <reproc/reproc.h>

#include <array>

TEST_CASE("stop")
{
  reproc_type infinite;

  static constexpr unsigned int ARGV_SIZE = 2;
  std::array<const char *, ARGV_SIZE> argv{ { INFINITE_PATH, nullptr } };

  int error = REPROC_SUCCESS;
  CAPTURE(error);

  error = reproc_start(&infinite, ARGV_SIZE - 1, argv.data(), nullptr);
  REQUIRE(!error);

  error = reproc_wait(&infinite, 50, nullptr);
  REQUIRE(error == REPROC_WAIT_TIMEOUT);

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

  error = reproc_wait(&infinite, 50, nullptr);
  REQUIRE(!error);

  reproc_destroy(&infinite);
}
