#include <string>
#include <iostream>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

int main(void)
{
  // Get rid of windows \r\n magic
  #ifdef _WIN32
    _setmode(_fileno(stdout), O_BINARY);
    _setmode(_fileno(stdin), O_BINARY);
    _setmode(_fileno(stderr), O_BINARY);
  #endif

  std::string input;
  std::getline(std::cin, input);
  fprintf(stdout, "%s\n", input.c_str());
  fflush(stdout);
  std::getline(std::cin, input);
  fprintf(stderr, "%s\n", input.c_str());
  fflush(stderr);
  std::getline(std::cin, input);
  fprintf(stdout, "%s\n", input.c_str());
  fflush(stdout);
  std::getline(std::cin, input);
  fprintf(stderr, "%s\n", input.c_str());
  fflush(stderr);

  return 0;
}