#include <iostream>
#include <string>

int main(int argc, char *argv[])
{
  (void) argc;
  std::string mode = argv[1];

  std::string input;
  std::getline(std::cin, input);

  if (mode == "stdout" || mode == "both") {
    std::cout << input;
  }

  if (mode == "stderr" || mode == "both") {
    std::cerr << input;
  }

  return 0;
}
