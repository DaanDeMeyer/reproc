#include <doctest/doctest.h>
#include <process.h>

TEST_CASE("simple-process")
{
  const char *argv[2] = {"res/echo", 0};
  int argc = 1;
  uint32_t timeout = 100;
  char buffer[1000];
  uint32_t actual = 0;

  Process *process = process_alloc();
  REQUIRE(process);

  PROCESS_LIB_ERROR error = process_init(process);
  REQUIRE(!error);

  error = process_start(process, argc, argv, NULL);
  REQUIRE(!error);

  error = process_write(process, "stdout\n", 7, &actual);
  REQUIRE(!error);

  error = process_read(process, buffer, 1000, &actual);
  REQUIRE(!error);
  
  buffer[actual] = '\0';
  REQUIRE_EQ(buffer, "stdout\n");

  error = process_write(process, "stderr\n", 7, &actual);
  REQUIRE(!error);

  error = process_read_stderr(process, buffer, 1000, &actual);
  REQUIRE(!error);

  buffer[actual] = '\0';
  REQUIRE_EQ(buffer, "stderr\n");

  error = process_wait(process, timeout);
  REQUIRE(!error);

  int32_t exit_status;
  error = process_exit_status(process, &exit_status);
  REQUIRE(!error);
  REQUIRE(exit_status == 0);
  
  error = process_free(process);
  REQUIRE(!error);
}