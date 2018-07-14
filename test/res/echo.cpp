#include <iostream>
#include <string>

int main(int /*unused*/, char *argv[])
{
  std::string output = argv[1];

  std::string input;
  std::getline(std::cin, input);

  if (output == "stdout") {
    std::cout << input;
  } else if (output == "stderr") {
    std::cerr << input;
  }

  return 0;
}
