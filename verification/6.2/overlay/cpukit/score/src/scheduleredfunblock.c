/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSScoreSchedulerEDF
 *
 * @brief This source file contains the implementation of
 *   _Scheduler_EDF_Unblock().
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
 * Frama-C 32 is strict about implicit function declarations. RTEMS'
 * timestampimpl.h calls sbttots/tstosbt/sbttotv without a visible
 * declaration in the headers we include here. Forward-declare them so
 * parsing succeeds; their bodies aren't part of the Unblock slice.
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
  requires \valid( the_thread );
  requires \valid( node );
  requires \valid( (Scheduler_EDF_Node *) node );
  requires \valid( &((Scheduler_EDF_Node *) node)->Base );
  requires &((Scheduler_EDF_Node *) node)->Base == node;
  requires \valid( (Scheduler_EDF_Context *) scheduler->context );
  requires edf_ready_context_well_formed{Pre}(
    (Scheduler_EDF_Context *) scheduler->context );
  requires edf_ready_context_cache_consistent{Pre}(
    (Scheduler_EDF_Context *) scheduler->context );
  requires !edf_ready_member{Pre}(
    (Scheduler_EDF_Context *) scheduler->context,
    (Scheduler_EDF_Node *) node );
  requires edf_ready_node_has_canonical_owner{Pre}(
    (Scheduler_EDF_Node *) node );
  requires SCHEDULER_PRIORITY_PURIFY( node->Priority.value ) ==
    ((Scheduler_EDF_Node *) node)->Base.Wait.Priority.Node.priority;

  // the_thread is not already represented in the ready set: required so
  // the post-state ready set remains owner-distinct (the well-formedness
  // invariant). Semantically, Unblock moves a thread from blocked to ready,
  // so the thread must not already be represented by some other ready node.
  requires \forall Scheduler_EDF_Node *m;
    m \in edf_ready_set{Pre}(
      (Scheduler_EDF_Context *) scheduler->context ) ==>
      m->Base.owner != the_thread;

  requires \valid( _Thread_Heir );
  requires \valid_read( &_Thread_Heir->is_preemptible );
  requires \valid_read( &_Thread_Heir->Scheduler.nodes );
  requires \valid( _Thread_Heir->Scheduler.nodes );
  requires \valid_read( (Scheduler_EDF_Node *) _Thread_Heir->Scheduler.nodes );
  requires &((Scheduler_EDF_Node *) _Thread_Heir->Scheduler.nodes)->Base ==
    _Thread_Heir->Scheduler.nodes;
  requires \valid( &_Thread_Heir->cpu_time_used );
  requires \valid(
    &_Per_CPU_Information[ 0 ].per_cpu.cpu_usage_timestamp );

  // P3 assumed at entry: heir is non-preemptible or owns the earliest-ready
  // scheduler node, and dispatch is set if heir differs from executing.
  // Proven again at exit (post-call heir).
  requires edf_scheduler_decision{Pre}(
    (Scheduler_EDF_Context *) scheduler->context,
    _Per_CPU_Information[ 0 ].per_cpu.executing,
    _Thread_Heir,
    _Thread_Heir->is_preemptible,
    _Thread_Dispatch_necessary_ghost );

  // The new node belongs to the_thread; needed to make the_thread the
  // owner of the earliest-ready node in the update_heir case.
  requires ((Scheduler_EDF_Node *) node)->Base.owner == the_thread;


  // Cache invariant for _Thread_Heir's home node. The context-level cache
  // invariant covers ready nodes; this explicit assumption also covers the
  // non-preemptible-heir case where the heir need not be represented in the
  // ready set.
  requires ((Scheduler_EDF_Node *) _Thread_Heir->Scheduler.nodes)->priority ==
    _Thread_Heir->Scheduler.nodes->Wait.Priority.Node.priority;


  requires \separated(
    _Thread_Heir->Scheduler.nodes,
    (Scheduler_EDF_Node *) node
  );
  requires \forall Scheduler_EDF_Node *m;
    m \in edf_ready_set{Pre}(
      (Scheduler_EDF_Context *) scheduler->context ) ==>
      \separated( (Scheduler_EDF_Node *) node + (..), m + (..) );
  requires \separated(
    (Scheduler_EDF_Node *) _Thread_Heir->Scheduler.nodes + (..),
    (Scheduler_EDF_Node *) node + (..),
    (Scheduler_EDF_Context *) scheduler->context + (..)
  );
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
  // _Thread_Heir points to a Thread_Control that is not inside Per_CPU
  // and is not the scheduler context. Needed so WP can derive that
  // _Thread_Heir->Scheduler.nodes is preserved across Enqueue's
  // `assigns context->Ready`.
  requires \separated(
    _Thread_Heir + (..),
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..),
    scheduler + (..),
    (Scheduler_EDF_Context *) scheduler->context + (..)
  );

  assigns ((Scheduler_EDF_Node *) node)->Base.Priority,
          ((Scheduler_EDF_Node *) node)->priority,
          ((Scheduler_EDF_Context *) scheduler->context)->Ready,
          _Per_CPU_Information[ 0 ].per_cpu.heir,
          _Per_CPU_Information[ 0 ].per_cpu.dispatch_necessary,
          _Thread_Dispatch_necessary_ghost,
          _Thread_Heir->cpu_time_used,
          _Per_CPU_Information[ 0 ].per_cpu.heir->cpu_time_used,
          _Per_CPU_Information[ 0 ].per_cpu.cpu_usage_timestamp;

  ensures ((Scheduler_EDF_Node *) node)->priority ==
    SCHEDULER_PRIORITY_PURIFY( \at( node->Priority.value, Pre ) );

  // thread is added to the ready set
  ensures edf_ready_set{Post}(
            (Scheduler_EDF_Context *) scheduler->context ) ==
          edf_ready_insert(
            edf_ready_set{Pre}(
              (Scheduler_EDF_Context *) scheduler->context ),
            (Scheduler_EDF_Node *) node );

  // Inductive invariant: the ready context remains well-formed at every
  // EDF API boundary.
  ensures edf_ready_context_well_formed{Post}(
    (Scheduler_EDF_Context *) scheduler->context );
  ensures edf_ready_context_cache_consistent{Post}(
    (Scheduler_EDF_Context *) scheduler->context );

  // P3 at exit: new heir is the earliest-ready thread (if preemptible),
  // dispatch is set if heir differs from executing.
  ensures edf_scheduler_decision{Post}(
    (Scheduler_EDF_Context *) scheduler->context,
    _Per_CPU_Information[ 0 ].per_cpu.executing,
    _Thread_Heir,
    _Thread_Heir->is_preemptible,
    _Thread_Dispatch_necessary_ghost );

  behavior keep_due_to_priority:
    assumes SCHEDULER_PRIORITY_PURIFY( node->Priority.value ) >=
      \at( _Thread_Heir, Pre )->Scheduler.nodes->Wait.Priority.Node.priority;
    ensures _Thread_Heir == \at( _Thread_Heir, Pre );

  behavior keep_due_to_nonpreemptible:
    assumes SCHEDULER_PRIORITY_PURIFY( node->Priority.value ) <
      \at( _Thread_Heir, Pre )->Scheduler.nodes->Wait.Priority.Node.priority;
    assumes !\at( _Thread_Heir, Pre )->is_preemptible;
    ensures _Thread_Heir == \at( _Thread_Heir, Pre );

  behavior update_heir:
    assumes SCHEDULER_PRIORITY_PURIFY( node->Priority.value ) <
      \at( _Thread_Heir, Pre )->Scheduler.nodes->Wait.Priority.Node.priority;
    assumes \at( _Thread_Heir, Pre )->is_preemptible;
    ensures _Thread_Heir == the_thread;

  complete behaviors;
  disjoint behaviors;
*/
void _Scheduler_EDF_Unblock(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Scheduler_Node          *node
)
{
  Scheduler_EDF_Context *context;
  Scheduler_EDF_Node    *the_node;
  Priority_Control       priority;
  Priority_Control       insert_priority;

  context = _Scheduler_EDF_Get_context( scheduler );
  the_node = _Scheduler_EDF_Node_downcast( node );
  priority = _Scheduler_Node_get_priority( &the_node->Base );
  priority = SCHEDULER_PRIORITY_PURIFY( priority );
  insert_priority = SCHEDULER_PRIORITY_APPEND( priority );

  the_node->priority = priority;
  /*@ assert edf_ready_node_cache_consistent{Here}( the_node ); */
  /*@ assert edf_ready_context_cache_consistent{Here}( context ); */
  _Scheduler_EDF_Enqueue( context, the_node, insert_priority );

  /*@ assert \at( _Thread_Heir, Pre )->is_preemptible &&
    priority >=
      \at( _Thread_Heir, Pre )
        ->Scheduler.nodes->Wait.Priority.Node.priority ==>
    edf_thread_node_is_earliest_ready{Here}(
      context,
      _Thread_Heir,
      (Scheduler_EDF_Node *) _Thread_Heir->Scheduler.nodes
    ); */
  /*@ assert \at( _Thread_Heir, Pre )->is_preemptible &&
    priority <
      \at( _Thread_Heir, Pre )
        ->Scheduler.nodes->Wait.Priority.Node.priority ==>
    edf_thread_node_is_earliest_ready{Here}(
      context,
      the_thread,
      the_node
    ); */
  _Scheduler_uniprocessor_Unblock( scheduler, the_thread, priority );
}
