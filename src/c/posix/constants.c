#include "constants.h"

// process id 0 is reserved by the system so we can use it as a null value
const pid_t PID_NULL = 0;
// File descriptor 0 won't be assigned by pipe() call (its reserved for stdin)
// so we use it as a null value
const int PIPE_NULL = INT_MIN;
// Exit codes on unix are (should be) in range [0,256) so INT_MIN shoudl be a
// safe null value.
const int EXIT_STATUS_NULL = INT_MIN;
