/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * SIMPLIFIED VERIFICATION HEADER — replaces the original schedulerimpl.h.
 * See verification/6.2/HEADERS.md for documentation of changes.
 *
 * Original: cpukit/include/rtems/score/schedulerimpl.h (RTEMS 6.2)
 * This version keeps only the functions needed for EDF scheduler
 * verification and cuts off the transitive includes that pull in
 * ~200 uncontracted inline functions (threadimpl.h, smpimpl.h, etc.).
 */

#ifndef _RTEMS_SCORE_SCHEDULERIMPL_H
#define _RTEMS_SCORE_SCHEDULERIMPL_H

#include <rtems/score/scheduler.h>
#include <rtems/score/priorityimpl.h>
#include <rtems/score/schedulernodeimpl.h>
#include <rtems/score/status.h>
#include <rtems/score/threadimpl.h>
/* REMOVED: #include <rtems/score/assert.h>    — not needed */
/* REMOVED: #include <rtems/score/smpimpl.h>   — pulls in processormaskimpl.h (22 functions) */
/* NOTE: threadimpl.h is our SIMPLIFIED version (~10 functions, not the original 71+) */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Gets the context of the scheduler.
 *
 * Original: schedulerimpl.h lines 104-109
 */
/*@
  requires \valid_read(scheduler);
  assigns \result \from scheduler;
  ensures \result == scheduler->context;
 */
static inline Scheduler_Context *_Scheduler_Get_context(
  const Scheduler_Control *scheduler
)
{
  return scheduler->context;
}

/**
 * @brief Acquires the scheduler instance inside a critical section.
 *
 * Original: schedulerimpl.h lines 120-131
 * On uniprocessor this is a no-op.
 */
/*@
  assigns \nothing;
*/
static inline void _Scheduler_Acquire_critical(
  const Scheduler_Control *scheduler,
  ISR_lock_Context        *lock_context
)
{
#if defined(RTEMS_SMP)
  Scheduler_Context *context;

  context = _Scheduler_Get_context( scheduler );
  _ISR_lock_Acquire( &context->Lock, lock_context );
#else
  (void) scheduler;
  (void) lock_context;
#endif
}

/**
 * @brief Releases the scheduler instance inside a critical section.
 *
 * Original: schedulerimpl.h lines 140-151
 * On uniprocessor this is a no-op.
 */
/*@
  assigns \nothing;
*/
static inline void _Scheduler_Release_critical(
  const Scheduler_Control *scheduler,
  ISR_lock_Context        *lock_context
)
{
#if defined(RTEMS_SMP)
  Scheduler_Context *context;

  context = _Scheduler_Get_context( scheduler );
  _ISR_lock_Release( &context->Lock, lock_context );
#else
  (void) scheduler;
  (void) lock_context;
#endif
}

/*
 * The following functions are declared but not defined here.
 * They are defined in other translation units and used via
 * function pointers or direct calls from the verified code.
 * Stubs with ACSL contracts are provided in the stubs header files.
 */

void _Scheduler_Handler_initialization( void );

void _Scheduler_Update_priority( Thread_Control *the_thread );

#ifdef __cplusplus
}
#endif

#endif /* _RTEMS_SCORE_SCHEDULERIMPL_H */
