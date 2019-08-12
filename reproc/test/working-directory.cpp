#include <doctest.h>
#include <reproc/reproc.h>

#include <array>

TEST_SUITE("reproc")
{
  TEST_CASE("working-directory")
  {
    reproc_t noop;

    // Executable path is relative to reproc/resources because we change
    // working directory before executing noop as a child process.
    const char *working_directory = "reproc/resources";

#if defined(_WIN32)
    std::array<const char *, 2> argv{ "reproc/resources/noop",
                                              nullptr };
#else
    std::array<const char *, 2> argv{ "./noop", nullptr };
#endif

    int error = REPROC_SUCCESS;
    CAPTURE(error);

    error = reproc_start(&noop, argv.data(), working_directory);
    REQUIRE(!error);

    error = reproc_wait(&noop, REPROC_INFINITE);
    REQUIRE(!error);
    REQUIRE((reproc_exit_status(&noop) == 0));

    reproc_destroy(&noop);
  }
}
