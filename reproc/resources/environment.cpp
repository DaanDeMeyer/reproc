#include <iostream>

int main(int argc, char *argv[], char *envp[])
{
  (void) argc;
  (void) argv;

  for (size_t i = 0; envp[i] != nullptr; i++) {
    std::cout << envp[i];
  }
}
