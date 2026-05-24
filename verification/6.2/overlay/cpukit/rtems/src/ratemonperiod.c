/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSImplClassicRateMonotonic
 *
 * @brief This source file contains the implementation of
 *   rtems_rate_monotonic_period_states().
 */

/*
 *  COPYRIGHT (c) 1989-2010.
 *  On-Line Applications Research Corporation (OAR).
 *  Copyright (c) 2016 embedded brains GmbH & Co. KG
 *  COPYRIGHT (c) 2016 Kuan-Hsun Chen.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

#include <rtems/rtems/ratemonimpl.h>
#include <rtems/score/schedulerimpl.h>
#include <rtems/score/todimpl.h>

#ifdef __FRAMAC__
/*@
  requires \valid_read( lock_context );
  assigns \nothing;
  ensures \valid( \result );
  ensures \result == &_Per_CPU_Information[ 0 ].per_cpu;
*/
Per_CPU_Control *_RM_Assume_Thread_Dispatch_disable_critical(
  const ISR_lock_Context *lock_context
);

/*@
  requires \valid( the_watchdog );
  requires \valid( cpu );
  requires cpu->Watchdog.ticks < 0x8000000000000000;
  requires ticks < 0x8000000000000000 - cpu->Watchdog.ticks;
  assigns \nothing;
  ensures \result == \at( cpu->Watchdog.ticks, Pre ) + ticks;
  ensures \result < 0x8000000000000000;
*/
uint64_t _RM_Assume_Watchdog_Per_CPU_insert_ticks(
  Watchdog_Control  *the_watchdog,
  Per_CPU_Control   *cpu,
  Watchdog_Interval  ticks
);

/*@
  requires \valid( the_period );
  requires \valid_read( lock_context );
  assigns \nothing;
*/
void _RM_Assume_Rate_monotonic_Release(
  Rate_monotonic_Control *the_period,
  ISR_lock_Context       *lock_context
);

/*@
  requires \valid( cpu_self );
  assigns \nothing;
*/
void _RM_Assume_Thread_Dispatch_enable( Per_CPU_Control *cpu_self );

static Thread_queue_Context _Rate_monotonic_Release_job_queue_context;

#define _Thread_Dispatch_disable_critical \
  _RM_Assume_Thread_Dispatch_disable_critical
#define _Watchdog_Per_CPU_insert_ticks \
  _RM_Assume_Watchdog_Per_CPU_insert_ticks
#define _Rate_monotonic_Release _RM_Assume_Rate_monotonic_Release
#define _Thread_Dispatch_enable _RM_Assume_Thread_Dispatch_enable
#endif

void _Rate_monotonic_Get_status(
  const Rate_monotonic_Control *the_period,
  Timestamp_Control            *wall_since_last_period,
  Timestamp_Control            *cpu_since_last_period
)
{
  Timestamp_Control        uptime;
  Thread_Control          *owning_thread = the_period->owner;
  Timestamp_Control        used;

  /*
   *  Determine elapsed wall time since period initiated.
   */
  _TOD_Get_uptime( &uptime );
  _Timestamp_Subtract(
    &the_period->time_period_initiated, &uptime, wall_since_last_period
  );

  /*
   *  Determine cpu usage since period initiated.
   */
  used = _Thread_Get_CPU_time_used( owning_thread );

   /* used = current cpu usage - cpu usage at start of period */
  _Timestamp_Subtract(
    &the_period->cpu_usage_period_initiated,
    &used,
    cpu_since_last_period
  );
}

static void _Rate_monotonic_Release_postponed_job(
  Rate_monotonic_Control *the_period,
  Thread_Control         *owner,
  rtems_interval          next_length,
  ISR_lock_Context       *lock_context
)
{
  Per_CPU_Control      *cpu_self;
  Thread_queue_Context  queue_context;

  --the_period->postponed_jobs;
  _Scheduler_Release_job(
    owner,
    &the_period->Priority,
    the_period->latest_deadline,
    &queue_context
  );

  cpu_self = _Thread_Dispatch_disable_critical( lock_context );
  _Rate_monotonic_Release( the_period, lock_context );
  _Thread_Priority_update( &queue_context );
  _Thread_Dispatch_direct( cpu_self );
}

/*@
  requires \valid( the_period );
  requires \valid( owner );
  requires \valid_read( lock_context );
  requires next_length < 0x8000000000000000;
  requires _Per_CPU_Information[ 0 ].per_cpu.Watchdog.ticks <
    0x8000000000000000;
  requires next_length <
    0x8000000000000000 -
      _Per_CPU_Information[ 0 ].per_cpu.Watchdog.ticks;

  requires \valid_read( _Scheduler_Table + ( 0 .. 0 ) );
  requires _Scheduler_Table[ 0 ].Operations.release_job ==
    _Scheduler_EDF_Release_job;
  requires _Scheduler_Table[ 0 ].Operations.update_priority ==
    _Scheduler_EDF_Update_priority;
  requires \valid( (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
  requires edf_ready_context_well_formed{Pre}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
  requires edf_ready_context_cache_consistent{Pre}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );

  // `\valid(Thread_Control *)` lifted out of the predicate body — see note
  // on the predicate definitions in threadimpl.h.
  requires \valid( owner );
  requires \valid( _Thread_Heir );
  requires thread_priority_edf_node_valid{Pre}( owner );
  requires thread_priority_edf_heir_valid{Pre}( _Thread_Heir );
  requires edf_scheduler_decision{Pre}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    _Per_CPU_Information[ 0 ].per_cpu.executing,
    _Thread_Heir,
    _Thread_Heir->is_preemptible,
    _Thread_Dispatch_necessary_ghost );

  requires priority_aggregation_well_formed{Pre}(
    &owner->Scheduler.nodes->Wait.Priority );
  requires priority_aggregation_cached_minimum{Pre}(
    &owner->Scheduler.nodes->Wait.Priority );
  requires priority_node_active_iff_contributor{Pre}(
    &owner->Scheduler.nodes->Wait.Priority,
    &the_period->Priority );
  requires \exists Priority_Node *node;
    node \in priority_contributors{Pre}(
      &owner->Scheduler.nodes->Wait.Priority );
  requires \forall Priority_Node *node;
    node \in priority_contributors{Pre}(
      &owner->Scheduler.nodes->Wait.Priority ) ==>
        \separated( the_period + (..), node + (..) );
  requires \forall Priority_Node *node;
    node \in priority_contributors{Pre}(
      &owner->Scheduler.nodes->Wait.Priority ) ==>
        \separated(
          &_Rate_monotonic_Release_job_queue_context + (..),
          node + (..)
        );

  requires \valid_read( &owner->Wait.operations );
  requires \valid( owner->Wait.operations );
  requires owner->Wait.operations->priority_actions ==
    _Thread_queue_Do_nothing_priority_actions;
  requires \valid( _Priority_Verify_scheduler_node_of_aggregation(
    &owner->Scheduler.nodes->Wait.Priority ) );
  requires &owner->Scheduler.nodes->Wait.Priority ==
    &_Priority_Verify_scheduler_node_of_aggregation(
      &owner->Scheduler.nodes->Wait.Priority )->Wait.Priority;
  requires (uintptr_t) &owner->Scheduler.nodes->Wait.Priority >=
    _Priority_Verify_wait_priority_node_offset;
  requires (uintptr_t) &owner->Scheduler.nodes->Wait.Priority <= UINTPTR_MAX;

  requires owner->current_state == STATES_READY ==>
    edf_ready_member{Pre}(
      (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
      (Scheduler_EDF_Node *) owner->Scheduler.nodes );
  requires owner->current_state == STATES_READY ==>
    SCHEDULER_PRIORITY_PURIFY( owner->Scheduler.nodes->Priority.value ) ==
      ((Scheduler_EDF_Node *)
        owner->Scheduler.nodes)->Base.Wait.Priority.Node.priority;
  requires thread_priority_edf_update_separated{Pre}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    owner );

  requires \separated(
    (Scheduler_Control const *) _Scheduler_Table + (..),
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context + (..),
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..),
    the_period + (..),
    owner + (..),
    owner->Scheduler.nodes + (..),
    owner->Wait.operations + (..),
    &_Rate_monotonic_Release_job_queue_context + (..),
    lock_context
  );
  requires \forall Scheduler_EDF_Node *node;
    node \in edf_ready_set{Pre}(
      (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context ) ==>
      \separated(
        &_Rate_monotonic_Release_job_queue_context + (..),
        node + (..)
      );
  requires \separated(
    &the_period->Priority,
    owner->Scheduler.nodes + (..)
  );
  requires \separated(
    &owner->Scheduler.nodes->Wait.Priority.Contributors,
    &owner->Scheduler.nodes->Wait.Priority.Node.priority,
    &owner->Scheduler.nodes->Priority.value,
    &the_period->Priority.priority
  );
  requires \separated(
    &the_period->Priority.priority,
    &owner->Scheduler.nodes->Wait.Priority.Contributors,
    &owner->Scheduler.nodes->Wait.Priority.Node.priority,
    &_Priority_Verify_scheduler_node_of_aggregation(
      &owner->Scheduler.nodes->Wait.Priority )->Priority.value
  );
  requires \forall Priority_Node *contributor;
    contributor \in priority_contributors{Pre}(
      &owner->Scheduler.nodes->Wait.Priority ) ==>
      \separated(
        contributor + (..),
        &_Priority_Verify_scheduler_node_of_aggregation(
          &owner->Scheduler.nodes->Wait.Priority )->Priority.value
      );
  requires \forall Scheduler_EDF_Node *node;
    node \in edf_ready_set{Pre}(
      (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context ) ==>
      \separated( &node->priority, &the_period->Priority.priority );
  requires \forall Scheduler_EDF_Node *node;
    node \in edf_ready_set{Pre}(
      (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context ) ==>
      \separated(
        &node->priority,
        &owner->Scheduler.nodes->Wait.Priority,
        &owner->Scheduler.nodes->Priority.value
      );

  assigns the_period->Priority.priority,
          _Rate_monotonic_Release_job_queue_context.Priority,
          owner->Scheduler.nodes->Wait.Priority,
          owner->Scheduler.nodes->Priority.value,
          ((Scheduler_EDF_Node *) owner->Scheduler.nodes)->priority,
          ((Scheduler_EDF_Node *) owner->Scheduler.nodes)->Base.Priority,
          ((Scheduler_EDF_Node *) \at( owner->Scheduler.nodes, Pre ))->priority,
          ((Scheduler_EDF_Node *)
            \at( owner->Scheduler.nodes, Pre ))->Base.Priority,
          ((Scheduler_EDF_Node *) \at(
            _Rate_monotonic_Release_job_queue_context.Priority.update[ 0 ]->
              Scheduler.nodes,
            Pre ))->priority,
          ((Scheduler_EDF_Node *) \at(
            _Rate_monotonic_Release_job_queue_context.Priority.update[ 0 ]->
              Scheduler.nodes,
            Pre ))->Base.Priority,
          ((Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context)->Ready,
          _Per_CPU_Information[ 0 ].per_cpu.heir,
          _Per_CPU_Information[ 0 ].per_cpu.dispatch_necessary,
          _Thread_Dispatch_necessary_ghost,
          _Thread_Heir->cpu_time_used,
          _Per_CPU_Information[ 0 ].per_cpu.heir->cpu_time_used,
          ((Thread_Control *) \at( _Thread_Heir, Pre ))->cpu_time_used,
          ((Thread_Control *)
            \at( _Per_CPU_Information[ 0 ].per_cpu.heir, Pre ))->
              cpu_time_used,
          _Per_CPU_Information[ 0 ].per_cpu.cpu_usage_timestamp;

  ensures edf_ready_context_well_formed{Post}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
  ensures priority_aggregation_well_formed{Post}(
    &owner->Scheduler.nodes->Wait.Priority );
  ensures priority_aggregation_cached_minimum{Post}(
    &owner->Scheduler.nodes->Wait.Priority );
  ensures priority_contributor_member{Post}(
    &owner->Scheduler.nodes->Wait.Priority,
    &the_period->Priority );
  ensures the_period->Priority.priority ==
    SCHEDULER_PRIORITY_MAP(
      \at( _Per_CPU_Information[ 0 ].per_cpu.Watchdog.ticks, Pre ) +
      next_length
    );
  ensures edf_scheduler_decision{Post}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    _Per_CPU_Information[ 0 ].per_cpu.executing,
    _Thread_Heir,
    _Thread_Heir->is_preemptible,
    _Thread_Dispatch_necessary_ghost );
  ensures owner->current_state == STATES_READY ==>
    edf_ready_node_cache_consistent{Post}(
      (Scheduler_EDF_Node *) owner->Scheduler.nodes );
  ensures edf_ready_set{Post}(
            (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context ) ==
          edf_ready_set{Pre}(
            (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );

  behavior active:
    assumes priority_node_active{Pre}( &the_period->Priority );
    ensures priority_contributors{Post}(
              &owner->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors{Pre}(
              &owner->Scheduler.nodes->Wait.Priority );

  behavior inactive:
    assumes !priority_node_active{Pre}( &the_period->Priority );
    ensures priority_contributors{Post}(
              &owner->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors_insert(
              priority_contributors{Pre}(
                &owner->Scheduler.nodes->Wait.Priority ),
              &the_period->Priority );

  complete behaviors active, inactive;
  disjoint behaviors active, inactive;
*/
static void _Rate_monotonic_Release_job(
  Rate_monotonic_Control *the_period,
  Thread_Control         *owner,
  rtems_interval          next_length,
  ISR_lock_Context       *lock_context
)
{
  Per_CPU_Control       *cpu_self;
  Thread_queue_Context  *queue_context;
  uint64_t               deadline;
#ifndef __FRAMAC__
  Thread_queue_Context   local_queue_context;
#endif

#ifdef __FRAMAC__
  queue_context = &_Rate_monotonic_Release_job_queue_context;
#else
  queue_context = &local_queue_context;
#endif

  cpu_self = _Thread_Dispatch_disable_critical( lock_context );

  deadline = _Watchdog_Per_CPU_insert_ticks(
    &the_period->Timer,
    cpu_self,
    next_length
  );
  _Scheduler_Release_job(
    owner,
    &the_period->Priority,
    deadline,
    queue_context
  );
  /*@ assert priority_contributor_member{Here}(
        &owner->Scheduler.nodes->Wait.Priority,
        &the_period->Priority ); */

  _Rate_monotonic_Release( the_period, lock_context );
  /*@ assert thread_priority_edf_heir_valid{Here}( _Thread_Heir ); */
  /*@ assert edf_scheduler_decision{Here}(
        (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
        _Per_CPU_Information[ 0 ].per_cpu.executing,
        _Thread_Heir,
        _Thread_Heir->is_preemptible,
        _Thread_Dispatch_necessary_ghost ); */
  /*@ assert edf_preemptible_heir_is_earliest_ready{Here}(
        (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
        _Thread_Heir,
        _Thread_Heir->is_preemptible ); */
  /*@ assert _Thread_Heir == \at( _Thread_Heir, Pre ); */
  /*@ assert _Per_CPU_Information[ 0 ].per_cpu.heir ==
        \at( _Per_CPU_Information[ 0 ].per_cpu.heir, Pre ); */
  /*@ assert &_Thread_Heir->cpu_time_used ==
        &((Thread_Control *) \at( _Thread_Heir, Pre ))->cpu_time_used; */
  /*@ assert &_Per_CPU_Information[ 0 ].per_cpu.heir->cpu_time_used ==
        &((Thread_Control *)
          \at( _Per_CPU_Information[ 0 ].per_cpu.heir, Pre ))->cpu_time_used; */
  /*@ assert owner->Scheduler.nodes ==
        \at( owner->Scheduler.nodes, Pre ); */
  /*@ assert queue_context->Priority.update_count == 1 ==>
        queue_context->Priority.update[ 0 ] == owner; */
  /*@ assert queue_context->Priority.update_count == 1 ==>
        queue_context->Priority.update[ 0 ]->Scheduler.nodes ==
          \at( owner->Scheduler.nodes, Pre ); */
  /*@ assert queue_context->Priority.update_count == 1 ==>
        &((Scheduler_EDF_Node *)
          queue_context->Priority.update[ 0 ]->Scheduler.nodes)->priority ==
        &((Scheduler_EDF_Node *)
          \at( owner->Scheduler.nodes, Pre ))->priority; */
  /*@ assert queue_context->Priority.update_count == 1 ==>
        &((Scheduler_EDF_Node *)
          queue_context->Priority.update[ 0 ]->Scheduler.nodes)->
            Base.Priority ==
        &((Scheduler_EDF_Node *)
          \at( owner->Scheduler.nodes, Pre ))->Base.Priority; */
  /*@ assert queue_context == &_Rate_monotonic_Release_job_queue_context; */
  /*@ assert queue_context->Priority.update_count == 0 ==>
        queue_context->Priority.update[ 0 ] ==
        \at( _Rate_monotonic_Release_job_queue_context.Priority.update[ 0 ],
             Pre ); */
  /*@ assert queue_context->Priority.update_count == 0 ==>
        queue_context->Priority.update[ 0 ]->Scheduler.nodes ==
        \at(
          _Rate_monotonic_Release_job_queue_context.Priority.update[ 0 ]->
            Scheduler.nodes,
          Pre ); */
  /*@ assert queue_context->Priority.update_count == 0 ==>
        &((Scheduler_EDF_Node *)
          queue_context->Priority.update[ 0 ]->Scheduler.nodes)->priority ==
        &((Scheduler_EDF_Node *) \at(
          _Rate_monotonic_Release_job_queue_context.Priority.update[ 0 ]->
            Scheduler.nodes,
          Pre ))->priority; */
  /*@ assert queue_context->Priority.update_count == 0 ==>
        &((Scheduler_EDF_Node *)
          queue_context->Priority.update[ 0 ]->Scheduler.nodes)->
            Base.Priority ==
        &((Scheduler_EDF_Node *) \at(
          _Rate_monotonic_Release_job_queue_context.Priority.update[ 0 ]->
            Scheduler.nodes,
          Pre ))->Base.Priority; */
  /*@ assert queue_context->Priority.update_count == 1 &&
        queue_context->Priority.update[ 0 ]->current_state == STATES_READY ==>
          edf_ready_member{Here}(
            (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
            (Scheduler_EDF_Node *)
              queue_context->Priority.update[ 0 ]->Scheduler.nodes ); */
  /*@ assert queue_context->Priority.update_count == 1 &&
        queue_context->Priority.update[ 0 ]->current_state == STATES_READY ==>
          SCHEDULER_PRIORITY_PURIFY(
            queue_context->Priority.update[ 0 ]->Scheduler.nodes->
              Priority.value ) ==
          ((Scheduler_EDF_Node *)
            queue_context->Priority.update[ 0 ]->Scheduler.nodes)->
              Base.Wait.Priority.Node.priority; */
  _Thread_Priority_update( queue_context );
  /*@ assert priority_contributor_member{Here}(
        &owner->Scheduler.nodes->Wait.Priority,
        &the_period->Priority ); */
  _Thread_Dispatch_enable( cpu_self );
}

#ifdef __FRAMAC__
#undef _Thread_Dispatch_disable_critical
#undef _Watchdog_Per_CPU_insert_ticks
#undef _Rate_monotonic_Release
#undef _Thread_Dispatch_enable
#endif

void _Rate_monotonic_Restart(
  Rate_monotonic_Control *the_period,
  Thread_Control         *owner,
  ISR_lock_Context       *lock_context
)
{
  /*
   *  Set the starting point and the CPU time used for the statistics.
   */
  _TOD_Get_uptime( &the_period->time_period_initiated );
  the_period->cpu_usage_period_initiated = _Thread_Get_CPU_time_used( owner );

  _Rate_monotonic_Release_job(
    the_period,
    owner,
    the_period->next_length,
    lock_context
  );
}

static void _Rate_monotonic_Update_statistics(
  Rate_monotonic_Control    *the_period
)
{
  Timestamp_Control          executed;
  Timestamp_Control          since_last_period;
  Rate_monotonic_Statistics *stats;

  /*
   *  Assume we are only called in states where it is appropriate
   *  to update the statistics.  This should only be RATE_MONOTONIC_ACTIVE
   *  and RATE_MONOTONIC_EXPIRED.
   */

  /*
   *  Update the counts.
   */
  stats = &the_period->Statistics;
  stats->count++;

  if ( the_period->state == RATE_MONOTONIC_EXPIRED )
    stats->missed_count++;

  /*
   *  Grab status for time statistics.
   */
  _Rate_monotonic_Get_status( the_period, &since_last_period, &executed );

  /*
   *  Update CPU time
   */
  _Timestamp_Add_to( &stats->total_cpu_time, &executed );

  if ( _Timestamp_Less_than( &executed, &stats->min_cpu_time ) )
    stats->min_cpu_time = executed;

  if ( _Timestamp_Greater_than( &executed, &stats->max_cpu_time ) )
    stats->max_cpu_time = executed;

  /*
   *  Update Wall time
   */
  _Timestamp_Add_to( &stats->total_wall_time, &since_last_period );

  if ( _Timestamp_Less_than( &since_last_period, &stats->min_wall_time ) )
    stats->min_wall_time = since_last_period;

  if ( _Timestamp_Greater_than( &since_last_period, &stats->max_wall_time ) )
    stats->max_wall_time = since_last_period;
}

static rtems_status_code _Rate_monotonic_Get_status_for_state(
  rtems_rate_monotonic_period_states state
)
{
  switch ( state ) {
    case RATE_MONOTONIC_INACTIVE:
      return RTEMS_NOT_DEFINED;
    case RATE_MONOTONIC_EXPIRED:
      return RTEMS_TIMEOUT;
    default:
      _Assert( state == RATE_MONOTONIC_ACTIVE );
      return RTEMS_SUCCESSFUL;
  }
}

static rtems_status_code _Rate_monotonic_Activate(
  Rate_monotonic_Control *the_period,
  rtems_interval          length,
  Thread_Control         *executing,
  ISR_lock_Context       *lock_context
)
{
  _Assert( the_period->postponed_jobs == 0 );
  the_period->state = RATE_MONOTONIC_ACTIVE;
  the_period->next_length = length;
  _Rate_monotonic_Restart( the_period, executing, lock_context );
  return RTEMS_SUCCESSFUL;
}

static rtems_status_code _Rate_monotonic_Block_while_active(
  Rate_monotonic_Control *the_period,
  rtems_interval          length,
  Thread_Control         *executing,
  ISR_lock_Context       *lock_context
)
{
  Per_CPU_Control *cpu_self;
  bool             success;

  /*
   *  Update statistics from the concluding period.
   */
  _Rate_monotonic_Update_statistics( the_period );

  /*
   *  This tells the _Rate_monotonic_Timeout that this task is
   *  in the process of blocking on the period and that we
   *  may be changing the length of the next period.
   */
  the_period->next_length = length;
  executing->Wait.return_argument = the_period;
  _Thread_Wait_flags_set( executing, RATE_MONOTONIC_INTEND_TO_BLOCK );

  cpu_self = _Thread_Dispatch_disable_critical( lock_context );
  _Rate_monotonic_Release( the_period, lock_context );

  _Thread_Set_state( executing, STATES_WAITING_FOR_PERIOD );

  success = _Thread_Wait_flags_try_change_acquire(
    executing,
    RATE_MONOTONIC_INTEND_TO_BLOCK,
    RATE_MONOTONIC_BLOCKED
  );
  if ( !success ) {
    _Assert(
      _Thread_Wait_flags_get( executing ) == THREAD_WAIT_STATE_READY
    );
    _Thread_Unblock( executing );
  }

  _Thread_Dispatch_direct( cpu_self );
  return RTEMS_SUCCESSFUL;
}

/*
 * There are two possible cases: one is that the previous deadline is missed,
 * The other is that the number of postponed jobs is not 0, but the current
 * deadline is still not expired, i.e., state = RATE_MONOTONIC_ACTIVE.
 */
static rtems_status_code _Rate_monotonic_Block_while_expired(
  Rate_monotonic_Control *the_period,
  rtems_interval          length,
  Thread_Control         *executing,
  ISR_lock_Context       *lock_context
)
{
  /*
   * No matter the just finished jobs in time or not,
   * they are actually missing their deadlines already.
   */
  the_period->state = RATE_MONOTONIC_EXPIRED;

  /*
   * Update statistics from the concluding period
   */
  _Rate_monotonic_Update_statistics( the_period );

  the_period->state = RATE_MONOTONIC_ACTIVE;
  the_period->next_length = length;

  _Rate_monotonic_Release_postponed_job(
      the_period,
      executing,
      length,
      lock_context
  );
  return RTEMS_TIMEOUT;
}

rtems_status_code rtems_rate_monotonic_period(
  rtems_id       id,
  rtems_interval length
)
{
  Rate_monotonic_Control            *the_period;
  ISR_lock_Context                   lock_context;
  Thread_Control                    *executing;
  rtems_status_code                  status;
  rtems_rate_monotonic_period_states state;

  the_period = _Rate_monotonic_Get( id, &lock_context );
  if ( the_period == NULL ) {
    return RTEMS_INVALID_ID;
  }

  executing = _Thread_Executing;
  if ( executing != the_period->owner ) {
    _ISR_lock_ISR_enable( &lock_context );
    return RTEMS_NOT_OWNER_OF_RESOURCE;
  }

  _Rate_monotonic_Acquire_critical( the_period, &lock_context );

  state = the_period->state;

  if ( length == RTEMS_PERIOD_STATUS ) {
    status = _Rate_monotonic_Get_status_for_state( state );
    _Rate_monotonic_Release( the_period, &lock_context );
  } else {
    switch ( state ) {
      case RATE_MONOTONIC_ACTIVE:

        if( the_period->postponed_jobs > 0 ){
          /*
           * If the number of postponed jobs is not 0, it means the
           * previous postponed instance is finished without exceeding
           * the current period deadline.
           *
           * Do nothing on the watchdog deadline assignment but release the
           * next remaining postponed job.
           */
          status = _Rate_monotonic_Block_while_expired(
            the_period,
            length,
            executing,
            &lock_context
          );
        }else{
          /*
           * Normal case that no postponed jobs and no expiration, so wait for
           * the period and update the deadline of watchdog accordingly.
           */
          status = _Rate_monotonic_Block_while_active(
            the_period,
            length,
            executing,
            &lock_context
          );
        }
        break;
      case RATE_MONOTONIC_INACTIVE:
        status = _Rate_monotonic_Activate(
          the_period,
          length,
          executing,
          &lock_context
        );
        break;
      default:
        /*
         * As now this period was already TIMEOUT, there must be at least one
         * postponed job recorded by the watchdog. The one which exceeded
         * the previous deadlines was just finished.
         *
         * Maybe there is more than one job postponed due to the preemption or
         * the previous finished job.
         */
        _Assert( state == RATE_MONOTONIC_EXPIRED );
        status = _Rate_monotonic_Block_while_expired(
          the_period,
          length,
          executing,
          &lock_context
        );
        break;
    }
  }

  return status;
}
