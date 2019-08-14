#include <doctest.h>
#include <reproc/reproc.h>

#include <array>

TEST_SUITE("reproc")
{
  TEST_CASE("working-directory")
  {
    reproc_t noop;

    REPROC_ERROR error = REPROC_SUCCESS;
    INFO(reproc_strerror(error));

    const char *working_directory = "reproc/resources";
    std::array<const char *, 2> argv{ "reproc/resources/noop", nullptr };

    error = reproc_start(&noop, argv.data(), working_directory);
    REQUIRE(!error);

    error = reproc_wait(&noop, REPROC_INFINITE);
    REQUIRE(!error);
    REQUIRE((reproc_exit_status(&noop) == 0));

    reproc_destroy(&noop);
  }
}
