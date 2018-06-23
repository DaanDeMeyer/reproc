#include <doctest/doctest.h>
#include <process.h>
#include <string.h>

/* 2x write to stdin and read from stdout
 * 2x write to stdin and read from stderr
 * See res/echo for the child process code
 */
TEST_CASE("read-write")
{
  const char *argv[2] = {"res/echo", 0};
  int argc = 1;
  uint32_t timeout = 100;

  const char *stdout_msg = "stdout\n";
  const char *stderr_msg = "stderr\n";

  char buffer[1000];
  uint32_t actual = 0;

  Process *process = NULL;
  PROCESS_LIB_ERROR error = process_init(&process);
  REQUIRE(!error);
  REQUIRE(process);

  error = process_start(process, argc, argv, NULL);
  REQUIRE(!error);

  error = process_write(process, stdout_msg, (uint32_t) strlen(stdout_msg),
                        &actual);
  REQUIRE(!error);

  error = process_read(process, buffer, 1000, &actual);
  REQUIRE(!error);

  buffer[actual] = '\0';
  CHECK_EQ(buffer, stdout_msg);

  error = process_write(process, stdout_msg, (uint32_t) strlen(stdout_msg),
                        &actual);
  REQUIRE(!error);

  error = process_read(process, buffer, 1000, &actual);
  REQUIRE(!error);

  buffer[actual] = '\0';
  CHECK_EQ(buffer, stdout_msg);

  error = process_write(process, stderr_msg, (uint32_t) strlen(stderr_msg),
                        &actual);
  REQUIRE(!error);

  error = process_read_stderr(process, buffer, 1000, &actual);
  REQUIRE(!error);

  buffer[actual] = '\0';
  CHECK_EQ(buffer, stderr_msg);

  error = process_write(process, stderr_msg, (uint32_t) strlen(stderr_msg),
                        &actual);
  REQUIRE(!error);

  error = process_read_stderr(process, buffer, 1000, &actual);
  REQUIRE(!error);

  buffer[actual] = '\0';
  CHECK_EQ(buffer, stderr_msg);

  error = process_wait(process, timeout);
  REQUIRE(!error);

  int32_t exit_status;
  error = process_exit_status(process, &exit_status);
  REQUIRE(!error);
  REQUIRE((exit_status == 0));

  error = process_free(process);
  REQUIRE(!error);
}