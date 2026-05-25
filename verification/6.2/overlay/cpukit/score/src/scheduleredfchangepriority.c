/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSScoreSchedulerEDF
 *
 * @brief This source file contains the implementation of
 *   _Scheduler_EDF_Update_priority().
 */

/*
 *  Copyright (C) 2011 Petr Benes.
 *  Copyright (C) 2011 On-Line Applications Research Corporation (OAR).
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
#include <rtems/score/schedulerimpl.h>
#include <rtems/score/thread.h>

/*@
  requires \valid_read( scheduler );
  requires \valid( node );
  requires \valid( (Scheduler_EDF_Node *) node );
  requires \valid( &((Scheduler_EDF_Node *) node)->Base );
  requires &((Scheduler_EDF_Node *) node)->Base == node;
  requires \valid( (Scheduler_EDF_Context *) scheduler->context );
  requires edf_ready_context_well_formed{Pre}(
    (Scheduler_EDF_Context *) scheduler->context );

  requires \valid_read( &the_thread->current_state );
  requires \valid_read( &the_thread->Scheduler.nodes );

  requires \valid_read( &_Thread_Heir->is_preemptible );
  requires \valid_read( &_Thread_Heir->Scheduler.nodes );
  requires \valid( _Thread_Heir->Scheduler.nodes );
  requires \valid_read( (Scheduler_EDF_Node *) _Thread_Heir->Scheduler.nodes );
  requires &((Scheduler_EDF_Node *) _Thread_Heir->Scheduler.nodes)->Base ==
    _Thread_Heir->Scheduler.nodes;
  requires \valid( &_Thread_Heir->cpu_time_used );
  requires \valid(
    &_Per_CPU_Information[ 0 ].per_cpu.cpu_usage_timestamp );

  // P3 at entry, re-established at exit.
  requires edf_scheduler_decision{Pre}(
    (Scheduler_EDF_Context *) scheduler->context,
    _Per_CPU_Information[ 0 ].per_cpu.executing,
    _Thread_Heir,
    _Thread_Heir->is_preemptible,
    _Thread_Dispatch_necessary_ghost );

  // The node belongs to the_thread.
  requires ((Scheduler_EDF_Node *) node)->Base.owner == the_thread;
  requires \separated(
    the_thread + (..),
    node + (..)
  );
  requires \separated(
    &node->Wait.Priority.Node.priority,
    &node->Priority.value,
    &((Scheduler_EDF_Node *) node)->priority
  );
  requires \forall Priority_Node *contributor;
    contributor \in priority_contributors{Pre}( &node->Wait.Priority ) ==>
      \separated(
        contributor + (..),
        &node->Priority.value,
        &((Scheduler_EDF_Node *) node)->priority
      );

  // If the_thread is ready, the_node is already in the ready set
  // required so Extract's precondition holds in the extract_enqueue path.
  requires the_thread->current_state == STATES_READY ==>
    edf_ready_member{Pre}(
      (Scheduler_EDF_Context *) scheduler->context,
      (Scheduler_EDF_Node *) node );

  // Update_priority repairs cache consistency for the node it is called on.
  // Several priority updates can be queued, so others may still be stale.
  requires the_thread->current_state == STATES_READY ==>
    priority_purifies_to(
      node->Priority.value,
      node->Wait.Priority.Node.priority );
  requires the_thread->current_state == STATES_READY ==>
    SCHEDULER_PRIORITY_PURIFY( node->Priority.value ) ==
      node->Wait.Priority.Node.priority;
  requires \separated(
    (Scheduler_EDF_Node *) node + (..),
    (Scheduler_EDF_Context *) scheduler->context + (..)
  );
  requires the_thread->current_state == STATES_READY ==>
    \forall Scheduler_EDF_Node *m;
      m \in edf_ready_set{Pre}(
        (Scheduler_EDF_Context *) scheduler->context ) &&
      m != (Scheduler_EDF_Node *) node ==>
        \separated( (Scheduler_EDF_Node *) node + (..), m + (..) );
  requires \separated(
    node + (..),
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..)
  );
  requires \separated(
    the_thread + (..),
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..)
  );
  requires \separated(
    scheduler + (..),
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..)
  );
  requires \separated(
    _Thread_Heir + (..),
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..),
    scheduler + (..),
    (Scheduler_EDF_Context *) scheduler->context + (..)
  );

  assigns ((Scheduler_EDF_Node *) node)->priority,
          ((Scheduler_EDF_Node *) node)->Base.Priority,
          ((Scheduler_EDF_Context *) scheduler->context)->Ready,
          _Per_CPU_Information[ 0 ].per_cpu.heir,
          _Per_CPU_Information[ 0 ].per_cpu.dispatch_necessary,
          _Thread_Dispatch_necessary_ghost,
          _Thread_Heir->cpu_time_used,
          _Per_CPU_Information[ 0 ].per_cpu.heir->cpu_time_used,
          _Per_CPU_Information[ 0 ].per_cpu.cpu_usage_timestamp;

  // P3 at exit.
  ensures edf_scheduler_decision{Post}(
    (Scheduler_EDF_Context *) scheduler->context,
    _Per_CPU_Information[ 0 ].per_cpu.executing,
    _Thread_Heir,
    _Thread_Heir->is_preemptible,
    _Thread_Dispatch_necessary_ghost );

  // Inductive invariant: the ready context remains well-formed at every
  // EDF API boundary.
  ensures edf_ready_context_well_formed{Post}(
    (Scheduler_EDF_Context *) scheduler->context );
  ensures edf_ready_set{Post}(
            (Scheduler_EDF_Context *) scheduler->context ) ==
          edf_ready_set{Pre}(
            (Scheduler_EDF_Context *) scheduler->context );
  ensures edf_priority_cache_consistency_preserved{Pre,Post}(
    edf_ready_set{Pre}( (Scheduler_EDF_Context *) scheduler->context ) );
  ensures the_thread->current_state == STATES_READY ==>
    edf_ready_node_cache_consistent{Post}( (Scheduler_EDF_Node *) node );
  ensures the_thread->Scheduler.nodes ==
          \at( the_thread->Scheduler.nodes, Pre );
  ensures priority_contributors{Post}( &node->Wait.Priority ) ==
          priority_contributors{Pre}( &node->Wait.Priority );
  ensures \at(
    priority_aggregation_well_formed( &node->Wait.Priority ),
    Pre
  ) ==>
    priority_aggregation_well_formed{Post}( &node->Wait.Priority );
  ensures \at(
    priority_aggregation_cached_minimum( &node->Wait.Priority ),
    Pre
  ) ==>
    priority_aggregation_cached_minimum{Post}( &node->Wait.Priority );
  ensures \forall Priority_Node *priority_node;
    \valid_read( priority_node ) &&
    \separated(
      &priority_node->priority,
      &((Scheduler_EDF_Node *) node)->priority
    ) ==>
      priority_node->priority == \at( priority_node->priority, Pre );

  behavior not_ready:
    assumes the_thread->current_state != STATES_READY;
    assigns \nothing;
    ensures _Thread_Heir == \at( _Thread_Heir, Pre );

  behavior priority_unchanged:
    assumes the_thread->current_state == STATES_READY;
    assumes SCHEDULER_PRIORITY_PURIFY( node->Priority.value ) ==
      ((Scheduler_EDF_Node *) node)->priority;
    assigns ((Scheduler_EDF_Node *) node)->Base.Priority;
    ensures _Thread_Heir == \at( _Thread_Heir, Pre );

  behavior extract_enqueue:
    assumes the_thread->current_state == STATES_READY;
    assumes SCHEDULER_PRIORITY_PURIFY( node->Priority.value ) !=
      ((Scheduler_EDF_Node *) node)->priority;
    ensures ((Scheduler_EDF_Node *) node)->priority ==
      SCHEDULER_PRIORITY_PURIFY( \at( node->Priority.value, Pre ) );

  complete behaviors;
  disjoint behaviors;
*/
void _Scheduler_EDF_Update_priority(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Scheduler_Node          *node
)
{
  Scheduler_EDF_Context *context;
  Scheduler_EDF_Node    *the_node;
  Priority_Control       priority;
  Priority_Control       insert_priority;

  if ( !_Thread_Is_ready( the_thread ) ) {
    /* Nothing to do */
    /*@ assert edf_dispatch_set_if_heir_differs(
          _Per_CPU_Information[ 0 ].per_cpu.executing,
          _Thread_Heir,
          _Thread_Dispatch_necessary_ghost ); */
    /*@ assert priority_contributors{Here}( &node->Wait.Priority ) ==
          priority_contributors{Pre}( &node->Wait.Priority ); */
    /*@ assert \at(
          priority_aggregation_well_formed( &node->Wait.Priority ),
          Pre
        ) ==>
          priority_aggregation_well_formed{Here}( &node->Wait.Priority ); */
    /*@ assert \at(
          priority_aggregation_cached_minimum( &node->Wait.Priority ),
          Pre
        ) ==>
          priority_aggregation_cached_minimum{Here}( &node->Wait.Priority ); */
    return;
  }

  the_node = _Scheduler_EDF_Node_downcast( node );
  insert_priority = _Scheduler_Node_get_priority( &the_node->Base );
  priority = SCHEDULER_PRIORITY_PURIFY( insert_priority );
  /*@ assert insert_priority == node->Priority.value; */
  /*@ assert priority_purifies_to(
        insert_priority,
        node->Wait.Priority.Node.priority ); */
  /*@ assert SCHEDULER_PRIORITY_PURIFY( insert_priority ) ==
        node->Wait.Priority.Node.priority; */
  /*@ assert priority ==
        (Priority_Control) SCHEDULER_PRIORITY_PURIFY( insert_priority ); */
  /*@ assert priority == node->Wait.Priority.Node.priority; */

  if ( priority == the_node->priority ) {
    /* Nothing to do */
    /*@ assert edf_dispatch_set_if_heir_differs(
          _Per_CPU_Information[ 0 ].per_cpu.executing,
          _Thread_Heir,
          _Thread_Dispatch_necessary_ghost ); */
    /*@ assert priority_contributors{Here}( &node->Wait.Priority ) ==
          priority_contributors{Pre}( &node->Wait.Priority ); */
    /*@ assert \at(
          priority_aggregation_well_formed( &node->Wait.Priority ),
          Pre
        ) ==>
          priority_aggregation_well_formed{Here}( &node->Wait.Priority ); */
    /*@ assert \at(
          priority_aggregation_cached_minimum( &node->Wait.Priority ),
          Pre
        ) ==>
          priority_aggregation_cached_minimum{Here}( &node->Wait.Priority ); */
    return;
  }

  the_node->priority = priority;
  /*@ assert \forall Priority_Node *priority_node;
        \valid_read( priority_node ) &&
        \separated( &priority_node->priority, &the_node->priority ) ==>
          priority_node->priority == \at( priority_node->priority, Pre ); */
  context = _Scheduler_EDF_Get_context( scheduler );
  /*@ assert edf_ready_node_cache_consistent{Here}( the_node ); */

  _Scheduler_EDF_Extract( context, the_node );
  /*@ assert edf_ready_node_cache_consistent{Here}( the_node ); */
  _Scheduler_EDF_Enqueue( context, the_node, insert_priority );

  // Pin a witness for Get_highest_ready precondition
  /*@ assert
        the_node \in edf_ready_set{Here}(
          (Scheduler_EDF_Context *) scheduler->context ); */

  _Scheduler_uniprocessor_Schedule(
    scheduler,
    _Scheduler_EDF_Get_highest_ready
  );
  /*@ assert edf_dispatch_set_if_heir_differs(
        _Per_CPU_Information[ 0 ].per_cpu.executing,
        _Thread_Heir,
        _Thread_Dispatch_necessary_ghost ); */
  /*@ assert priority_contributors{Here}( &node->Wait.Priority ) ==
        priority_contributors{Pre}( &node->Wait.Priority ); */
  /*@ assert \at(
        priority_aggregation_well_formed( &node->Wait.Priority ),
        Pre
      ) ==>
        priority_aggregation_well_formed{Here}( &node->Wait.Priority ); */
  /*@ assert node->Wait.Priority.Node.priority ==
        \at( node->Wait.Priority.Node.priority, Pre ); */
  /*@ assert \forall Priority_Node *contributor;
        contributor \in priority_contributors{Pre}( &node->Wait.Priority ) ==>
          contributor->priority == \at( contributor->priority, Pre ); */
  /*@ assert \at(
        priority_aggregation_cached_minimum( &node->Wait.Priority ),
        Pre
      ) ==>
        priority_aggregation_cached_minimum{Here}( &node->Wait.Priority ); */

  // Pin the witness for the post-condition existentials
  /*@ assert \at( _Thread_Heir, Pre )->is_preemptible ==>
        edf_thread_node_is_earliest_ready{Here}(
          context,
          _Thread_Heir,
          (Scheduler_EDF_Node *) _Thread_Heir->Scheduler.nodes
        ); */
}
