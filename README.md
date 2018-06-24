# process-lib

[![Build Status](https://travis-ci.com/DaanDeMeyer/process-lib.svg?branch=master)](https://travis-ci.com/DaanDeMeyer/process-lib)

## Gotcha's

- While `process_terminate` allows the child process to perform cleanup it is up
  to the child process to correctly clean up after itself. process-lib only
  sends a termination signal to the child process itself. The child process is
  responsible for cleaning up its own child processes and other resources in its
  signal handler.

- When using `process_kill` the child process does not receive a chance to
  perform cleanup which could result in leaking of resources. Chief among these
  leaks is that the child process will not be able to stop its own child
  processes. Always let a child process exit normally or try to stop it with
  `process_terminate` before switching to `process_kill`.

- (POSIX) On POSIX a parent process is required to wait on a child process
  (using `process_wait`) after it has exited before all resources related to
  that process can be freed by the kernel. If the parent doesn't wait on a child
  process after it exits, the child process becomes a
  [zombie process](https://en.wikipedia.org/wiki/Zombie_process).

- (Windows) `process_kill` is not guaranteed to kill a process on Windows. For
  more information, read the Remarks section in the documentation of
  [TerminateProcess](<https://msdn.microsoft.com/en-us/library/windows/desktop/ms686714(v=vs.85).aspx>)
  which process-lib uses to kill processes on Windows.

- (Windows) Immediately terminating a process after starting it on Windows might
  result in an error window with error 0xc0000142 popping up. This indicates the
  process was terminated before it was fully initialized. I was not able to find
  a Windows function that allows waiting until a console process is fully
  initialized. This problem shouldn't pop up with normal use of the library
  since most of the time you'll want to read/write to the process or wait until
  it exits normally.

  If someone runs into this problem, I mitigated it in process-lib's tests by
  waiting a few milliseconds using `process_wait` before terminating the
  process.

## Design

process-lib is designed to be a minimal wrapper around the platform-specific
API's for starting a process, interacting with its standard streams and finally
terminating it.

### Opaque pointer

process-lib uses a process struct to store information between calls to library
functions. This struct is forward-declared in process.h with each
platform-specific implementation hidden in the source files for that platform.

This struct is typedefed to Process and exposed to the user as an opaque pointer
(Process \*).

```c
typedef struct process Process;
```

The process.h header only contains a forward-declaration of the process struct.
We provide an implementation in the source files of each supported platform
where we store platform-specific members such as pipe handles, process id's,
....

Advantages:

- Fewer includes required in process.h

  Because we only have a forward declaration of the process struct in process.h
  we don't need any platform-specific includes (such as windows.h) in the header
  file to define the data types of all the members that the struct contains.

- No leaking of implementation details

  Including the process.h header only gives the user access to the forward
  declaration of the process struct and not its implementation which means its
  impossible to access the internals of the struct outside of the library. This
  allows us to change its implementation between versions without having to
  worry about breaking user code.

Disadvantages:

- No simple allocation on the stack

  Because including process.h does not give the compiler access to the full
  definition of the process struct it is unable to allocate its implementation
  on the stack since it doesn't know its size. Allocating on the stack is still
  possible but requires functions such as
  [alloca](https://linux.die.net/man/3/alloca) which is harder compared to just
  writing

  ```c
  Process process;
  ```

- Not possible to allocate on the heap without help from the library

  Because the compiler doesn't know the size of the process struct the user
  can't easily allocate the process struct on the heap because it can't figure
  out its size with sizeof.

  Because the library has to allocate memory regardless (see
  [Memory Allocation](#memory-allocation)), this problem is solved by allocating
  the required memory in the `process_init` function. If no memory allocations
  were required in the rest of the library, we could have the user allocate the
  memory himself by providing him with a `process_size` function which would
  allow the user to allocate the required memory himself and allow the library
  code to be completely free of dynamic memory allocations.

### Memory allocation

process-lib aims to do as few dynamic memory allocations as possible. As of this
moment, memory allocation is only done when allocating memory for the process
struct in `process_init` and when converting the array of program arguments to a
single string as required by the Windows
[CreateProcess](<https://msdn.microsoft.com/en-us/library/windows/desktop/ms682425(v=vs.85).aspx>)
function.

I have not found a way to avoid allocating memory while keeping a cross-platform
api for POSIX and Windows. (Windows `CreateProcessW` requires a single string of
arguments delimited by spaces while POSIX `execvp` requires an array of
arguments). Since process-lib's api takes process arguments as an array we have
to allocate memory to convert the array into a single string as required by
Windows.

process-lib uses the standard `malloc` and `free` functions to allocate and free
memory. However, providing support for custom allocators should be
straightforward. If you need them, please open an issue.

### (Posix) Waiting on child process with timeout

I did not find a counterpart for the Windows
[WaitForSingleObject](<https://msdn.microsoft.com/en-us/library/windows/desktop/ms687032(v=vs.85).aspx>)
function which can be used to wait until a process exits or the provided timeout
expires. POSIX has a similar function called
[waitpid](https://linux.die.net/man/2/waitpid) but this function does not
support specifying a timeout value.

To support waiting with a timeout value on POSIX, each process is put in its own
process group with the same id as the process id with a call to `setpgid` after
forking the process. When calling the `process_wait` function, a timeout process
is forked which we put in the same process group as the process we want to wait
for with the same `setpgid` function and puts itself to sleep for the requested
amount of time (timeout value) before exiting. We then call the `waitpid`
function in the main process but instead of passing the process id of the
process we want to wait for we pass the negative value of the process id
`-process->pid`. Passing a negative value for the process id to `waitpid`
instructs it to wait for all processes in the process group of the absolute
value of the passed negative value. In our case it will wait for both the
timeout process we started and the process we actually want to wait for. If
`waitpid` returns the process id of the timeout process we know the timeout
value has been exceeded. If `waitpid` returns the process id of the process we
want to wait for we know it has exited before the timeout process and that the
timeout value has not been exceeded.

This solution was inspired by [this](https://stackoverflow.com/a/8020324)
Stackoverflow answer.
