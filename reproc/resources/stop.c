#ifdef _WIN32
  #include <windows.h>
  #define sleep(x) Sleep((x))
#else
  #define _POSIX_C_SOURCE 200809L
  #include <time.h>
  #define sleep(x)                                                             \
    nanosleep(&(struct timespec){ .tv_sec = (x) / 1000,                        \
                                  .tv_nsec = ((x) % 1000) * 1000000 },         \
              NULL);
#endif

int main(void)
{
  sleep(25000);
  return 0;
}
