#include <doctest.h>

#include <reproc/reproc.h>

#include <array>

TEST_CASE("stop")
{
  reproc_t process;

  REPROC_ERROR error = REPROC_SUCCESS;
  INFO(reproc_error_string(error));

  std::array<const char *, 2> argv{ RESOURCE_DIRECTORY "/infinite", nullptr };

  error = reproc_start(&process, argv.data(), nullptr, nullptr);
  REQUIRE(!error);

  error = reproc_wait(&process, 50);
  REQUIRE(error == REPROC_ERROR_WAIT_TIMEOUT);

  SUBCASE("terminate")
  {
    error = reproc_terminate(&process);
    REQUIRE(!error);
  }

  SUBCASE("kill")
  {
    error = reproc_kill(&process);
    REQUIRE(!error);
  }

  error = reproc_wait(&process, 50);
  REQUIRE(!error);

  reproc_destroy(&process);
}
