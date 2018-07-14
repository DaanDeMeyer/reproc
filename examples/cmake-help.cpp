#include <process.hpp>

#include <array>
#include <iostream>

int main()
{
  Process::Error error = Process::SUCCESS;
  Process process;

  std::vector<std::string> args = { "cmake", "--help" };

  error = process.start(args, nullptr);
  if (error) { return 1; }

  std::array<char, 1024> buffer{};
  auto buffer_length = static_cast<unsigned int>(buffer.size() - 1);
  unsigned int actual = 0;

  while (true) {
    error = process.read(buffer.data(), buffer_length, &actual);
    if (error) { break; }

    buffer[actual] = '\0';
    std::cout << buffer.data();
  }

  if (error != Process::STREAM_CLOSED) { return 1; }

  error = process.wait(Process::INFINITE);
  if (error) { return 1; }

  int exit_status;
  error = process.exit_status(&exit_status);
  if (error) { return 1; }

  return exit_status;
}
