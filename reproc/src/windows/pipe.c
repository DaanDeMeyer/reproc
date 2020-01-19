// Make sure `WSAPoll` and friends are defined.
#define _WIN32_WINNT _WIN32_WINNT_VISTA

#include "pipe.h"

#include "error.h"
#include "handle.h"
#include "macro.h"

#include <assert.h>
#include <limits.h>
#include <windows.h>
#include <winsock2.h>

const SOCKET PIPE_INVALID = INVALID_SOCKET;

// Inspired by https://gist.github.com/geertj/4325783.
static int socketpair(int domain, int type, int protocol, SOCKET *out)
{
  assert(out);

  SOCKET server = PIPE_INVALID;
  SOCKET pair[2] = { PIPE_INVALID, PIPE_INVALID };
  int r = -1;

  server = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, 0);
  if (server == INVALID_SOCKET) {
    goto finish;
  }

  SOCKADDR_IN localhost = { 0 };
  localhost.sin_family = AF_INET;
  localhost.sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
  localhost.sin_port = 0;

  r = bind(server, (SOCKADDR *) &localhost, sizeof(localhost));
  if (r < 0) {
    goto finish;
  }

  r = listen(server, 1);
  if (r < 0) {
    goto finish;
  }

  SOCKADDR_STORAGE name = { 0 };
  int size = sizeof(name);
  r = getsockname(server, (SOCKADDR *) &name, &size);
  if (r < 0) {
    goto finish;
  }

  pair[0] = WSASocketW(domain, type, protocol, NULL, 0, 0);
  if (pair[0] == INVALID_SOCKET) {
    goto finish;
  }

  u_long mode = 1;
  r = ioctlsocket(pair[0], (long) FIONBIO, &mode);
  if (r < 0) {
    goto finish;
  }

  r = connect(pair[0], (SOCKADDR *) &name, size);
  if (r < 0 && WSAGetLastError() != WSAEWOULDBLOCK) {
    goto finish;
  }

  pair[1] = accept(server, NULL, NULL);
  if (pair[1] == INVALID_SOCKET) {
    r = -1;
    goto finish;
  }

  out[0] = pair[0];
  out[1] = pair[1];

  pair[0] = PIPE_INVALID;
  pair[1] = PIPE_INVALID;

  r = 0;

finish:
  pipe_destroy(server);
  pipe_destroy(pair[0]);
  pipe_destroy(pair[1]);

  return error_unify(r);
}

int pipe_init(SOCKET *read, SOCKET *write)
{
  assert(read);
  assert(write);

  SOCKET pair[2] = { PIPE_INVALID, PIPE_INVALID };
  int r = 0;

  // Use sockets instead of pipes so we can use `WSAPoll` which only works with
  // sockets.
  r = socketpair(AF_INET, SOCK_STREAM, 0, pair);
  if (r < 0) {
    goto finish;
  }

  r = SetHandleInformation((HANDLE) pair[0], HANDLE_FLAG_INHERIT, 0);
  if (r == 0) {
    goto finish;
  }

  r = SetHandleInformation((HANDLE) pair[1], HANDLE_FLAG_INHERIT, 0);
  if (r == 0) {
    goto finish;
  }

  // Make the connection unidirectional to better emulate a pipe.

  r = shutdown(pair[0], SD_SEND);
  if (r < 0) {
    goto finish;
  }

  r = shutdown(pair[1], SD_RECEIVE);
  if (r < 0) {
    goto finish;
  }

  *read = pair[0];
  *write = pair[1];

  pair[0] = PIPE_INVALID;
  pair[1] = PIPE_INVALID;

finish:
  pipe_destroy(pair[0]);
  pipe_destroy(pair[1]);

  return error_unify(r);
}

int pipe_read(SOCKET pipe, uint8_t *buffer, size_t size)
{
  assert(pipe != PIPE_INVALID);
  assert(buffer);
  assert(size <= INT_MAX);

  int r = recv(pipe, (char *) buffer, (int) size, 0);

  if (r == 0 || (r < 0 && WSAGetLastError() == WSAECONNRESET)) {
    return -ERROR_BROKEN_PIPE;
  }

  return error_unify_or_else(r, r);
}

int pipe_write(SOCKET pipe, const uint8_t *buffer, size_t size, int timeout)
{
  assert(pipe != PIPE_INVALID);
  assert(buffer);
  assert(size <= INT_MAX);

  int r = -1;

  if (timeout > 0) {
    WSAPOLLFD pollfd = { .fd = pipe, .events = POLLOUT };

    r = WSAPoll(&pollfd, 1, timeout);
    if (r <= 0) {
      if (r == 0) {
        r = -WAIT_TIMEOUT;
      }

      return error_unify(r);
    }
  }

  r = send(pipe, (const char *) buffer, (int) size, 0);

  if (r < 0 && WSAGetLastError() == WSAECONNRESET) {
    r = -ERROR_BROKEN_PIPE;
  }

  return error_unify_or_else(r, r);
}

int pipe_wait(SOCKET out, SOCKET err, SOCKET *ready, int timeout)
{
  assert(ready);

  WSAPOLLFD pollfds[2] = { { 0 }, { 0 } };
  ULONG num_pollfds = 0;

  if (out != PIPE_INVALID) {
    pollfds[num_pollfds++] = (WSAPOLLFD){ .fd = out, .events = POLLIN };
  }

  if (err != PIPE_INVALID) {
    pollfds[num_pollfds++] = (WSAPOLLFD){ .fd = err, .events = POLLIN };
  }

  if (num_pollfds == 0) {
    return -ERROR_BROKEN_PIPE;
  }

  int r = WSAPoll(pollfds, num_pollfds, timeout);
  if (r <= 0) {
    if (r == 0) {
      r = -WAIT_TIMEOUT;
    }

    return error_unify(r);
  }

  for (size_t i = 0; i < num_pollfds; i++) {
    WSAPOLLFD pollfd = pollfds[i];

    if (pollfd.revents > 0) {
      *ready = pollfd.fd;
      break;
    }
  }

  return 0;
}

SOCKET pipe_destroy(SOCKET pipe)
{
  if (pipe == PIPE_INVALID) {
    return PIPE_INVALID;
  }

  int saved = WSAGetLastError();

  int r = closesocket(pipe);
  assert_unused(r == 0);

  WSASetLastError(saved);

  return PIPE_INVALID;
}
