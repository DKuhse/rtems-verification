/**
 *  @file
 *
 *  @brief Scheduler EDF Allocate
 *  @ingroup RTEMSScoreScheduler
 */

/*
 *  Copyright (C) 2011 Petr Benes.
 *  Copyright (C) 2011-2012 On-Line Applications Research Corporation (OAR).
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.org/license/LICENSE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __FRAMAC__
/*
 * FC 32 is strict about implicit function declarations. Forward-declare
 * timestamp helpers reachable through timestampimpl.h.
 */
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

#include <rtems/score/scheduleredfimpl.h>

/*@
  requires \valid_read( scheduler );
  requires \valid( (Scheduler_EDF_Context *) scheduler->context );
  requires edf_ready_context_well_formed{Pre}(
    (Scheduler_EDF_Context *) scheduler->context );

  // Schedule is only meaningful when there is at least one ready node.
  requires \exists Scheduler_EDF_Node *some;
    some \in edf_ready_set{Pre}(
      (Scheduler_EDF_Context *) scheduler->context );

  requires \valid( _Thread_Heir );
  requires \valid_read( &_Thread_Heir->is_preemptible );
  requires \valid( &_Thread_Heir->cpu_time_used );
  requires \separated(
    _Thread_Heir + (..),
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..),
    scheduler + (..),
    (Scheduler_EDF_Context *) scheduler->context + (..)
  );

  assigns _Per_CPU_Information[ 0 ].per_cpu.heir;
  assigns _Per_CPU_Information[ 0 ].per_cpu.dispatch_necessary,
          _Thread_Dispatch_necessary_ghost;
  assigns _Thread_Heir->cpu_time_used,
          _Per_CPU_Information[ 0 ].per_cpu.heir->cpu_time_used,
          _Per_CPU_Information[ 0 ].per_cpu.cpu_usage_timestamp;

  // Either the heir survived (was non-preemptible, or already earliest),
  // or it was replaced by the owner of an EDF-earliest ready node.
  ensures \at( _Thread_Heir, Pre ) == _Thread_Heir ||
          ( \exists Scheduler_EDF_Node *node;
              edf_ready_earliest_node{Pre}(
                edf_ready_set{Pre}(
                  (Scheduler_EDF_Context *) scheduler->context ),
                node ) &&
              _Thread_Heir == node->Base.owner );
  ensures edf_ready_context_well_formed{Post}(
    (Scheduler_EDF_Context *) scheduler->context );
*/
void _Scheduler_EDF_Schedule(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread
)
{
  _Scheduler_EDF_Schedule_body( scheduler, the_thread, false );
}
