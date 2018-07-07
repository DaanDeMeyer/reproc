#include <process.hpp>
#include <iostream>

int main(void)
{
  Process::Error error = Process::SUCCESS;
  Process process;

  int argc = 2;
  const char *argv[3] = {"cmake", "--help", nullptr};

  error = process.start(argc, argv, nullptr);
  if (error) { return 1; }

  char buffer[1024];
  unsigned int buffer_size = 1024;
  unsigned int actual;

  while (true) {
    error = process.read(buffer, buffer_size - 1, &actual);
    if (error) { break; }

    buffer[actual] = '\0';
    std::cout << buffer;
  }

  if (error != Process::STREAM_CLOSED) { return 1; }

  error = process.wait(Process::INFINITE);
  if (error) { return 1; }

  int exit_status;
  error = process.exit_status(&exit_status);
  if (error) { return 1; }

  return exit_status;
}
