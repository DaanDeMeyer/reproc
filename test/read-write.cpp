#include <cstdlib>
#include <cstring>
#include <doctest.h>
#include <process.h>
#include <sstream>

// Alternates between writing to stdin and reading from stdout and writing to
// stdin and reading from stderr.
// See res/echo for the child process code
TEST_CASE("read-write")
{
  char buffer[1000];
  unsigned int buffer_size = 1000;
  unsigned int actual = 0;

  process_type *process = (process_type *) malloc(process_size());
  REQUIRE(process);

  PROCESS_LIB_ERROR error;
  CAPTURE(error);

  error = process_init(process);
  REQUIRE(!error);

  SUBCASE("stdout")
  {
    const char *msg = "This is stdout\n";

    int argc = 2;
    const char *argv[3] = { ECHO_PATH, "stdout", 0 };

    error = process_start(process, argc, argv, NOOP_DIR);
    REQUIRE(!error);

    error = process_write(process, msg, (unsigned int) strlen(msg), &actual);
    REQUIRE(!error);

    std::stringstream ss;

    while (true) {
      error = process_read(process, buffer, buffer_size - 1, &actual);
      if (error) { break; }

      buffer[actual] = '\0';
      ss << buffer;
    }

    REQUIRE_EQ(ss.str().c_str(), msg);
  }

  SUBCASE("stderr")
  {
    const char *msg = "This is stderr\n";

    int argc = 2;
    const char *argv[3] = { ECHO_PATH, "stderr", 0 };

    error = process_start(process, argc, argv, NOOP_DIR);
    REQUIRE(!error);

    error = process_write(process, msg, (unsigned int) strlen(msg), &actual);
    REQUIRE(!error);

    std::stringstream ss;

    while (true) {
      error = process_read_stderr(process, buffer, buffer_size - 1, &actual);
      if (error) { break; }

      buffer[actual] = '\0';
      ss << buffer;
    }

    REQUIRE_EQ(ss.str().c_str(), msg);
  }

  error = process_wait(process, PROCESS_LIB_INFINITE);
  REQUIRE(!error);

  int exit_status;
  error = process_exit_status(process, &exit_status);
  REQUIRE(!error);
  REQUIRE((exit_status == 0));

  error = process_destroy(process);
  REQUIRE(!error);

  free(process);
}
