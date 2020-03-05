#include "assert.h"

#include <reproc/reproc.h>
#include <reproc/sink.h>

#include <string.h>

#define MESSAGE "reproc stands for REdirected PROCess"

static void io()
{
  int r = -1;

  reproc_t *process = reproc_new();
  ASSERT(process);

  const char *argv[] = { RESOURCE_DIRECTORY "/io", NULL };

  r = reproc_start(process, argv,
                   (reproc_options){
                       .redirect.err.type = REPROC_REDIRECT_STDOUT });
  ASSERT(r >= 0);

  r = reproc_write(process, (uint8_t *) MESSAGE, strlen(MESSAGE));
  ASSERT(r == (int) strlen(MESSAGE));

  r = reproc_close(process, REPROC_STREAM_IN);
  ASSERT(r == 0);

  char *out = NULL;
  r = reproc_drain(process, reproc_sink_string(&out), REPROC_SINK_NULL);
  ASSERT(r == 0);

  ASSERT(out != NULL);

  ASSERT(strcmp(out, MESSAGE MESSAGE) == 0);

  r = reproc_wait(process, REPROC_INFINITE);
  ASSERT(r == 0);

  reproc_destroy(process);

  reproc_free(out);
}

static void timeout(void)
{
  int r = -1;

  reproc_t *process = reproc_new();
  ASSERT(process);

  const char *argv[] = { RESOURCE_DIRECTORY "/io", NULL };

  r = reproc_start(process, argv, (reproc_options){ 0 });
  ASSERT(r >= 0);

  reproc_event_source source = { process, REPROC_EVENT_OUT | REPROC_EVENT_ERR,
                                 0 };
  r = reproc_poll(&source, 1, 200);
  ASSERT(r == REPROC_ETIMEDOUT);

  r = reproc_close(process, REPROC_STREAM_IN);
  ASSERT(r == 0);

  r = reproc_poll(&source, 1, 200);
  ASSERT(r == 0);

  reproc_destroy(process);
}

int main(void)
{
  io();
  timeout();
}
