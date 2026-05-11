/*
 * Verification harness for uniprocessor scheduler inline helpers.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __FRAAMC__
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

int64_t tstosbt( struct timespec );
struct timespec sbttots( int64_t );
struct timeval sbttotv( int64_t );

#ifndef SBT_1S
#define SBT_1S ( (int64_t) 1 << 32 )
#endif
#endif

#include <rtems/score/scheduleruniimpl.h>
