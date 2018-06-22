#include <string>
#include <iostream>

// #ifdef _WIN32
// #include <io.h>
// #include <fcntl.h>
// #endif

int main(void)
{
  // #ifdef _WIN32
  //   _setmode(_fileno(stdout), O_BINARY);
  //   _setmode(_fileno(stdin), O_BINARY);
  //   _setmode(_fileno(stderr), O_BINARY);
  // #endif

  std::string input;
  std::getline(std::cin, input);
  std::cout << input << std::endl;
  std::getline(std::cin, input);
  std::cout << input << std::endl;
  std::getline(std::cin, input);
  std::cout << input << std::endl;

  return 0;
}