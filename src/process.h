
typedef struct process process;

/*
argc and argv follow the conventions of the main function in c and c++ programs.
argv must begin with the name of the program to execute and end with a NULL
value (example: ["ls", "-l", NULL]). argc represents the number of arguments
excluding the NULL value (for the previous example, argc would be 2).
*/
int process_init(process *process, int argc, char *argv[]);

