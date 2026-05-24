/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSImplClassicRateMonotonic
 *
 * @brief This source file contains the implementation of
 *   rtems_rate_monotonic_cancel().
 */

/*
 *  COPYRIGHT (c) 1989-2007.
 *  On-Line Applications Research Corporation (OAR).
 *  Copyright (c) 2016 embedded brains GmbH & Co. KG
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

#ifdef __FRAMAC__
/*@
  requires \valid( the_period );
  requires \valid( lock_context );
  assigns \nothing;
*/
void _RM_Assume_Rate_monotonic_Acquire_critical(
  Rate_monotonic_Control *the_period,
  ISR_lock_Context       *lock_context
);

/*@
  requires \valid( the_watchdog );
  assigns \nothing;
*/
void _RM_Assume_Watchdog_Per_CPU_remove_ticks(
  Watchdog_Control *the_watchdog
);

/*@
  requires \valid( lock_context );
  assigns \nothing;
  ensures \valid( \result );
  ensures \result == &_Per_CPU_Information[ 0 ].per_cpu;
*/
Per_CPU_Control *_RM_Assume_Thread_Dispatch_disable_critical(
  const ISR_lock_Context *lock_context
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

static Thread_queue_Context _Rate_monotonic_Cancel_queue_context;

#define _Rate_monotonic_Acquire_critical \
  _RM_Assume_Rate_monotonic_Acquire_critical
#define _Watchdog_Per_CPU_remove_ticks \
  _RM_Assume_Watchdog_Per_CPU_remove_ticks
#define _Thread_Dispatch_disable_critical \
  _RM_Assume_Thread_Dispatch_disable_critical
#define _Rate_monotonic_Release _RM_Assume_Rate_monotonic_Release
#define _Thread_Dispatch_enable _RM_Assume_Thread_Dispatch_enable
#endif

/*@
  requires \valid( the_period );
  requires \valid( owner );
  requires \valid( lock_context );
  requires the_period->owner == owner;

  requires \valid_read( _Scheduler_Table + ( 0 .. 0 ) );
  requires _Scheduler_Table[ 0 ].Operations.cancel_job ==
    _Scheduler_EDF_Cancel_job;
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
  requires edf_preemptible_heir_is_earliest_ready{Pre}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    _Thread_Heir,
    _Thread_Heir->is_preemptible );

  requires priority_aggregation_well_formed{Pre}(
    &owner->Scheduler.nodes->Wait.Priority );
  requires priority_aggregation_cached_minimum{Pre}(
    &owner->Scheduler.nodes->Wait.Priority );
  requires priority_node_active_iff_contributor{Pre}(
    &owner->Scheduler.nodes->Wait.Priority,
    &the_period->Priority );
  requires priority_node_active{Pre}( &the_period->Priority ) ==>
    ( \exists Priority_Node *other;
        other != &the_period->Priority &&
        other \in priority_contributors{Pre}(
          &owner->Scheduler.nodes->Wait.Priority ) );
  requires \forall Priority_Node *node;
    node \in priority_contributors{Pre}(
      &owner->Scheduler.nodes->Wait.Priority ) ==>
        \separated( the_period + (..), node + (..) );
  requires \forall Priority_Node *node;
    node \in priority_contributors{Pre}(
      &owner->Scheduler.nodes->Wait.Priority ) ==>
        \separated(
          &_Rate_monotonic_Cancel_queue_context + (..),
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

  requires thread_priority_edf_update_ready_pre{Pre}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    owner );
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
    &_Rate_monotonic_Cancel_queue_context + (..),
    lock_context
  );
  requires \forall Scheduler_EDF_Node *node;
    node \in edf_ready_set{Pre}(
      (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context ) ==>
      \separated(
        &_Rate_monotonic_Cancel_queue_context + (..),
        node + (..)
      );
  requires \separated(
    &the_period->Priority.Node.RBTree.Node.rbe_color,
    &the_period->postponed_jobs,
    &the_period->state,
    &owner->Scheduler.nodes->Wait.Priority.Contributors,
    &owner->Scheduler.nodes->Wait.Priority.Node.priority,
    &owner->Scheduler.nodes->Priority.value
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

  assigns the_period->postponed_jobs,
          the_period->state,
          the_period->Priority.Node.RBTree.Node.rbe_color,
          _Rate_monotonic_Cancel_queue_context.Priority,
          owner->Scheduler.nodes->Wait.Priority,
          owner->Scheduler.nodes->Priority.value,
          ((Scheduler_EDF_Node *) owner->Scheduler.nodes)->priority,
          ((Scheduler_EDF_Node *) owner->Scheduler.nodes)->Base.Priority,
          ((Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context)->Ready,
          _Per_CPU_Information[ 0 ].per_cpu.heir,
          _Per_CPU_Information[ 0 ].per_cpu.dispatch_necessary,
          _Thread_Dispatch_necessary_ghost,
          _Thread_Heir->cpu_time_used,
          _Per_CPU_Information[ 0 ].per_cpu.heir->cpu_time_used,
          _Per_CPU_Information[ 0 ].per_cpu.cpu_usage_timestamp;

  ensures the_period->postponed_jobs == 0;
  ensures the_period->state == RATE_MONOTONIC_INACTIVE;
  ensures the_period->Priority.priority ==
    \at( the_period->Priority.priority, Pre );
  ensures !priority_node_active{Post}( &the_period->Priority );
  ensures !priority_contributor_member{Post}(
    &owner->Scheduler.nodes->Wait.Priority,
    &the_period->Priority );
  ensures edf_ready_context_well_formed{Post}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
  ensures priority_aggregation_well_formed{Post}(
    &owner->Scheduler.nodes->Wait.Priority );
  ensures priority_aggregation_cached_minimum{Post}(
    &owner->Scheduler.nodes->Wait.Priority );
  ensures edf_scheduler_decision{Post}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    _Per_CPU_Information[ 0 ].per_cpu.executing,
    _Thread_Heir,
    _Thread_Heir->is_preemptible,
    _Thread_Dispatch_necessary_ghost );
  ensures edf_preemptible_heir_is_earliest_ready{Post}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    _Thread_Heir,
    _Thread_Heir->is_preemptible );
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
            priority_contributors_extract(
              priority_contributors{Pre}(
                &owner->Scheduler.nodes->Wait.Priority ),
              &the_period->Priority );

  behavior inactive:
    assumes !priority_node_active{Pre}( &the_period->Priority );
    ensures priority_contributors{Post}(
              &owner->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors{Pre}(
              &owner->Scheduler.nodes->Wait.Priority );

  complete behaviors active, inactive;
  disjoint behaviors active, inactive;
*/
void _Rate_monotonic_Cancel(
  Rate_monotonic_Control *the_period,
  Thread_Control         *owner,
  ISR_lock_Context       *lock_context
)
{
  Per_CPU_Control       *cpu_self;
  Thread_queue_Context  *queue_context;
#ifndef __FRAMAC__
  Thread_queue_Context   local_queue_context;
#endif

#ifdef __FRAMAC__
  queue_context = &_Rate_monotonic_Cancel_queue_context;
#else
  queue_context = &local_queue_context;
#endif

  _Rate_monotonic_Acquire_critical( the_period, lock_context );

  _Watchdog_Per_CPU_remove_ticks( &the_period->Timer );
  the_period->postponed_jobs = 0;
  the_period->state = RATE_MONOTONIC_INACTIVE;
  _Scheduler_Cancel_job(
    the_period->owner,
    &the_period->Priority,
    queue_context
  );
  /*@ assert edf_scheduler_decision{Here}(
        (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
        _Per_CPU_Information[ 0 ].per_cpu.executing,
        _Thread_Heir,
        _Thread_Heir->is_preemptible,
        _Thread_Dispatch_necessary_ghost ); */

  cpu_self = _Thread_Dispatch_disable_critical( lock_context );
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
  /*@ assert queue_context->Priority.update_count == 1 ==>
        queue_context->Priority.update[ 0 ] == owner; */
  /*@ assert queue_context->Priority.update_count == 1 ==>
        thread_priority_edf_update_ready_pre{Here}(
          (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
          queue_context->Priority.update[ 0 ] ); */
  /*@ assert queue_context->Priority.update_count == 1 &&
        queue_context->Priority.update[ 0 ]->current_state == STATES_READY ==>
          edf_ready_member{Here}(
            (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
            (Scheduler_EDF_Node *)
              queue_context->Priority.update[ 0 ]->Scheduler.nodes ); */
  /*@ assert queue_context->Priority.update_count == 1 &&
        queue_context->Priority.update[ 0 ]->current_state == STATES_READY ==>
          SCHEDULER_PRIORITY_PURIFY(
            queue_context->Priority.update[ 0 ]->Scheduler.nodes->Priority.value ) ==
            ((Scheduler_EDF_Node *)
              queue_context->Priority.update[ 0 ]->Scheduler.nodes)->
                Base.Wait.Priority.Node.priority; */
  _Thread_Priority_update( queue_context );
  _Thread_Dispatch_enable( cpu_self );
}

#ifdef __FRAMAC__
#undef _Rate_monotonic_Acquire_critical
#undef _Watchdog_Per_CPU_remove_ticks
#undef _Thread_Dispatch_disable_critical
#undef _Rate_monotonic_Release
#undef _Thread_Dispatch_enable
#endif

rtems_status_code rtems_rate_monotonic_cancel(
  rtems_id id
)
{
  Rate_monotonic_Control *the_period;
  ISR_lock_Context        lock_context;
  Thread_Control         *executing;

  the_period = _Rate_monotonic_Get( id, &lock_context );
  if ( the_period == NULL ) {
    return RTEMS_INVALID_ID;
  }

  executing = _Thread_Executing;
  if ( executing != the_period->owner ) {
    _ISR_lock_ISR_enable( &lock_context );
    return RTEMS_NOT_OWNER_OF_RESOURCE;
  }

  _Rate_monotonic_Cancel( the_period, executing, &lock_context );
  return RTEMS_SUCCESSFUL;
}
