#include <reproc/reproc.h>

// Sets up the requested redirection type for the given stream and stores the
// resulting file descriptors in `parent` and `child`.
//
// `parent` will only contain a valid file descriptor if `type` is
// `REPROC_REDIRECT_PIPE`. `child` has to be duplicated onto its corresponding
// stream in the child process after forking.
REPROC_ERROR
redirect(int *parent, int *child, REPROC_STREAM stream, REPROC_REDIRECT type);
