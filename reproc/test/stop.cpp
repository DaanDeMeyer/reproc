#include <doctest.h>
#include <reproc/reproc.h>

#include <array>

TEST_SUITE("reproc")
{
  TEST_CASE("stop")
  {
    reproc_t infinite;

    std::array<const char *, 2> argv{ "reproc/resources/infinite",
                                              nullptr };

    int error = REPROC_SUCCESS;
    CAPTURE(error);

    error = reproc_start(&infinite, argv.data(), nullptr);
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
}
