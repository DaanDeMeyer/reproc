#ifndef _STDBOOL
#define _STDBOOL

#pragma once

#include <wtypes.h>
#define __bool_true_false_are_defined 1

#ifndef __cplusplus

#if !defined(__clang__) && defined(_MSC_VER) && (_MSC_VER < 1800)
/* MSVC doesn't define _Bool or bool in C, but does have BOOL */
/* Note this doesn't pass autoconf's test because (bool) 0.5 != true */
typedef BYTE _Bool; /* sizeof(bool) == 1 in MSVC 2013 and upper */
#endif

#define bool  _Bool
#define false 0
#define true  1

#endif /* __cplusplus */

#endif /* _STDBOOL */
