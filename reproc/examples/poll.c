#ifdef _WIN32
  #include <windows.h>
  #define sleep(x) Sleep((DWORD) (x))
  #define getpid() (int) GetCurrentProcessId()
#else
  #define _POSIX_C_SOURCE 200809L
  #include <time.h>
  #include <unistd.h>
  #define sleep(x)                                                             \
    nanosleep(&(struct timespec){ .tv_sec = (x) / 1000,                        \
                                  .tv_nsec = ((x) % 1000) * 1000000 },         \
              NULL);
#endif

#include <reproc/reproc.h>

#include <stdlib.h>
#include <string.h>

enum { NUM_CHILDREN = 20 };

static int parent(const char *program)
{
  reproc_event_source children[NUM_CHILDREN] = { { 0 } };
  int r = -1;

  for (int i = 0; i < NUM_CHILDREN; i++) {
    reproc_t *process = reproc_new();

    const char *args[] = { program, "child", NULL };

    r = reproc_start(process, args, (reproc_options){ .nonblocking = true });
    if (r < 0) {
      goto finish;
    }

    children[i].process = process;
    children[i].interests = REPROC_EVENT_OUT;
  }

  for (;;) {
    r = reproc_poll(children, NUM_CHILDREN, REPROC_INFINITE);
    if (r < 0) {
      r = r == REPROC_EPIPE ? 0 : r;
      goto finish;
    }

    for (int i = 0; i < NUM_CHILDREN; i++) {
      if (children[i].process == NULL || !children[i].events) {
        continue;
      }

      uint8_t output[4096];
      r = reproc_read(children[i].process, REPROC_STREAM_OUT, output,
                      sizeof(output));
      if (r == REPROC_EPIPE) {
        // `reproc_destroy` returns `NULL`. Event sources with their process set
        // to `NULL` are ignored by `reproc_poll`.
        children[i].process = reproc_destroy(children[i].process);
        continue;
      }

      if (r < 0) {
        goto finish;
      }

      output[r] = '\0';
      printf("%s\n", output);
    }
  }

finish:
  for (int i = 0; i < NUM_CHILDREN; i++) {
    reproc_destroy(children[i].process);
  }

  if (r < 0) {
    fprintf(stderr, "%s\n", reproc_strerror(r));
  }

  return abs(r);
}

static int child(void)
{
  srand(((unsigned int) getpid()));
  int ms = rand() % NUM_CHILDREN * 4; // NOLINT
  sleep(ms);
  printf("Process %i slept %i milliseconds.", getpid(), ms);
  fclose(stdout);
  return EXIT_SUCCESS;
}

// Starts a number of child processes that each sleep a random amount of
// milliseconds before printing a message and exiting. The parent process polls
// each of the child processes and prints their messages to stdout.
int main(int argc, const char **argv)
{
  return argc > 1 && strcmp(argv[1], "child") == 0 ? child() : parent(argv[0]);
}
