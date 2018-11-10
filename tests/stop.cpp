#include <doctest.h>
#include <reproc/reproc.h>

#include <array>

#if defined(_WIN32)
#  include <windows.h>
#endif

TEST_CASE("stop")
{
  reproc_type infinite;

  static constexpr unsigned int ARGV_SIZE = 2;
  std::array<const char *, ARGV_SIZE> argv{ { INFINITE_PATH, nullptr } };

  int error = REPROC_SUCCESS;
  CAPTURE(error);

  error = reproc_start(&infinite, ARGV_SIZE - 1, argv.data(), nullptr);
  REQUIRE(!error);

// Wait to avoid terminating the child process on Windows before it is
// initialized (which would result in an error window appearing).
#if defined(_WIN32)
  Sleep(50);
#endif

  SUBCASE("terminate")
  {
    error = reproc_terminate(&infinite, 50, nullptr);
    REQUIRE(!error);
  }

  SUBCASE("kill")
  {
    error = reproc_kill(&infinite, 50, nullptr);
    REQUIRE(!error);
  }

  reproc_destroy(&infinite);
}
