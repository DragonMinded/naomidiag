#ifndef __COMMON_H
#define __COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

// Define this to a nonzero value to get extra debugging information.
#define DEBUG_ENABLED 0

#ifdef __cplusplus
}
#endif

#endif
