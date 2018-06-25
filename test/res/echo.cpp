#include <string>
#include <iostream>
#include <assert.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

int main(int argc, char *argv[])
{
  // Get rid of windows \r\n magic
  #ifdef _WIN32
    _setmode(_fileno(stdout), O_BINARY);
    _setmode(_fileno(stdin), O_BINARY);
    _setmode(_fileno(stderr), O_BINARY);
  #endif

  assert(argc == 2);
  assert(argv);
  assert(argv[1]);

  std::string output = argv[1];

  std::string input;
  std::getline(std::cin, input);

  if (output == "stdout") {
    fprintf(stdout, "%s\n", input.c_str());
    fflush(stdout);
  } else if(output == "stderr") {
    fprintf(stderr, "%s\n", input.c_str());
    fflush(stderr);
  }
  
  return 0;
}