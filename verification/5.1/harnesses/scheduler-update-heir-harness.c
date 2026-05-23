/*
 * Verification-only translation unit.
 *
 * In 5.1, _Scheduler_Update_heir() is a static-inline helper in
 * schedulerimpl.h.  We give it its own harness, mirroring 6.2's
 * scheduleruni-unblock-harness.c, so WP can verify the helper's contract
 * against its body in isolation.  EDF entry-point slices then consume the
 * verified contract.
 */

#ifdef __FRAMAC__
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

int64_t          tstosbt( struct timespec );
struct timespec  sbttots( int64_t );
struct timeval   sbttotv( int64_t );

#ifndef SBT_1S
#define SBT_1S ( (int64_t) 1 << 32 )
#endif
#endif

#include <rtems/score/schedulerimpl.h>
