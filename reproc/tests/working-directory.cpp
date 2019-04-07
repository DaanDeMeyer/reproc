#include <doctest.h>
#include <reproc/reproc.h>

#include <array>

TEST_SUITE("reproc")
{
  TEST_CASE("working-directory")
  {
    reproc_type noop;

    // Executable path is relative to reproc/tests/resources because we change
    // working directory before executing noop as a child process.
    const char *working_directory = "reproc/resources";
    static constexpr unsigned int ARGV_SIZE = 2;

#if defined(_WIN32) || defined(__APPLE__)
    std::array<const char *, ARGV_SIZE> argv{ { "reproc/resources/noop",
                                                nullptr } };
#else
    std::array<const char *, ARGV_SIZE> argv{ { "./noop", nullptr } };
#endif

    int error = REPROC_SUCCESS;
    CAPTURE(error);

    error = reproc_start(&noop, ARGV_SIZE - 1, argv.data(), working_directory);
    REQUIRE(!error);

    unsigned int exit_status = 0;
    error = reproc_wait(&noop, REPROC_INFINITE, &exit_status);
    REQUIRE(!error);
    REQUIRE((exit_status == 0));

    reproc_destroy(&noop);
  }
}