#include <reproc/reproc.h>

#include <assert.h>
#include <stdbool.h>

REPROC_ERROR reproc_stop(reproc_t *process,
                         REPROC_CLEANUP c1,
                         unsigned int t1,
                         REPROC_CLEANUP c2,
                         unsigned int t2,
                         REPROC_CLEANUP c3,
                         unsigned int t3)
{
  assert(process);

  REPROC_CLEANUP operations[3] = { c1, c2, c3 };
  unsigned int timeouts[3] = { t1, t2, t3 };

  // We don't set `error` to `REPROC_SUCCESS` so we can check if `reproc_wait`,
  // `reproc_terminate` or `reproc_kill` succeed (in which case `error` is set
  // to `REPROC_SUCCESS`).
  REPROC_ERROR error = REPROC_WAIT_TIMEOUT;

  for (int i = 0; i < 3; i++) {
    REPROC_CLEANUP operation = operations[i];
    unsigned int timeout = timeouts[i];

    switch (operation) {
    case REPROC_NOOP:
      continue;
    case REPROC_WAIT:
      break;
    case REPROC_TERMINATE:
      error = reproc_terminate(process);
      break;
    case REPROC_KILL:
      error = reproc_kill(process);
      break;
    }

    // Stop if `reproc_terminate` or `reproc_kill` return an error.
    if (error != REPROC_SUCCESS && error != REPROC_WAIT_TIMEOUT) {
      break;
    }

    error = reproc_wait(process, timeout);
    if (error != REPROC_WAIT_TIMEOUT) {
      break;
    }
  }

  return error;
}

#define BUFFER_SIZE 1024

REPROC_ERROR reproc_parse(reproc_t *process,
                          REPROC_STREAM stream,
                          bool (*parser)(void *context,
                                         const char *buffer,
                                         unsigned int size),
                          void *context)
{
  assert(process);
  assert(parser);

  // A single call to `read` might contain multiple messages. By always calling
  // `parser` once with no data before reading, we give it the chance to process
  // all previous output one by one before reading from the child process again.
  if (!parser(context, "", 0)) {
    return REPROC_SUCCESS;
  }

  char buffer[BUFFER_SIZE];
  REPROC_ERROR error = REPROC_SUCCESS;

  while (true) {
    unsigned int bytes_read = 0;
    error = reproc_read(process, stream, buffer, BUFFER_SIZE, &bytes_read);
    if (error) {
      break;
    }

    // `parser` returns false to tell us to stop reading.
    if (!parser(context, buffer, bytes_read)) {
      break;
    }
  }

  return error;
}

REPROC_ERROR
reproc_drain(reproc_t *process,
             REPROC_STREAM stream,
             bool (*sink)(void *context, const char *buffer, unsigned int size),
             void *context)
{
  assert(process);
  assert(sink);

  char buffer[BUFFER_SIZE];
  REPROC_ERROR error = REPROC_SUCCESS;

  while (true) {
    unsigned int bytes_read = 0;
    error = reproc_read(process, stream, buffer, BUFFER_SIZE, &bytes_read);
    if (error) {
      break;
    }

    // `sink` return false to tell us to stop reading.
    if (!sink(context, buffer, bytes_read)) {
      break;
    }
  }

  // The child process closing the stream is not treated as an error.
  if (error == REPROC_STREAM_CLOSED) {
    return REPROC_SUCCESS;
  }

  return error;
}

bool reproc_running(reproc_t *process)
{
  return reproc_wait(process, 0) == REPROC_SUCCESS ? false : true;
}

unsigned int reproc_exit_status(reproc_t *process)
{
  assert(process);
  return process->exit_status;
}
