#include <string>
#include <iostream>

int main(void)
{
  std::string input;
  std::getline(std::cin, input);
  std::cout << input << std::endl;
  std::getline(std::cin, input);
  std::cerr << input << std::endl;

  return 0;
}