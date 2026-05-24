/**
 * @file
 *
 * @ingroup RTEMSScoreScheduler
 *
 * @brief Scheduler EDF Unblock
 */

/*
 *  Copyright (C) 2011 Petr Benes.
 *  Copyright (C) 2011 On-Line Applications Research Corporation (OAR).
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

  // Carve-out vs 6.2: 5.1's body has a pseudo-ISR force-dispatch escape
  // hatch (`_Scheduler_Update_heir(the_thread, priority == PSEUDO_ISR)`)
  // that 6.2 does not. When a non-preemptible heir is force-dispatched to a
  // pseudo-ISR-priority the_thread, the_thread is not generally earliest in
  // the ready set (deadline threads with smaller priority values may be
  // present), so P3.a cannot hold for a preemptible pseudo-ISR new heir.
  // We exclude this branch via precondition; the contract then matches 6.2.
  // RTEMS callers (MPCI receive server, timer server, ratemon) honor this
  // naturally: ratemon tasks have deadline priorities; system pseudo-ISR
  // tasks are configured non-preemptible by convention.
  requires SCHEDULER_PRIORITY_PURIFY( node->Priority.value ) !=
    ( SCHEDULER_EDF_PRIO_MSB | PRIORITY_PSEUDO_ISR );

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
    // No call to _Scheduler_Update_heir in this branch; dispatch is fully
    // preserved.
    ensures _Thread_Dispatch_necessary_ghost ==
              \at( _Thread_Dispatch_necessary_ghost, Pre );

  behavior keep_due_to_nonpreemptible:
    assumes SCHEDULER_PRIORITY_PURIFY( node->Priority.value ) <
      \at( _Thread_Heir, Pre )->Scheduler.nodes->Wait.Priority.Node.priority;
    assumes !\at( _Thread_Heir, Pre )->is_preemptible;
    ensures _Thread_Heir == \at( _Thread_Heir, Pre );
    // _Scheduler_Update_heir's keep behavior preserves dispatch (assigns
    // \nothing); the dispatch-monotone top-level ensures also conveys it.
    ensures \at( _Thread_Dispatch_necessary_ghost, Pre ) ==>
              _Thread_Dispatch_necessary_ghost;

  behavior update_heir:
    assumes SCHEDULER_PRIORITY_PURIFY( node->Priority.value ) <
      \at( _Thread_Heir, Pre )->Scheduler.nodes->Wait.Priority.Node.priority;
    assumes \at( _Thread_Heir, Pre )->is_preemptible;
    ensures _Thread_Heir == the_thread;
    // _Scheduler_Update_heir's update behavior forces dispatch true.
    ensures _Thread_Dispatch_necessary_ghost;

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

  /*
   *  If the thread that was unblocked is more important than the heir,
   *  then we have a new heir.  This may or may not result in a
   *  context switch.
   *
   *  Normal case:
   *    If the current thread is preemptible, then we need to do
   *    a context switch.
   *  Pseudo-ISR case:
   *    Even if the thread isn't preemptible, if the new heir is
   *    a pseudo-ISR system task, we need to do a context switch.
   *    (Carved out by precondition; see contract.)
   */
  if ( priority < _Thread_Get_priority( _Thread_Heir ) ) {
    _Scheduler_Update_heir(
      the_thread,
      priority == ( SCHEDULER_EDF_PRIO_MSB | PRIORITY_PSEUDO_ISR )
    );
  }

  // P3.b: the if-true branch inherits it from _Scheduler_Update_heir's
  // top-level ensures; the if-false branch carries it from P3.b{Pre}
  // unchanged. Inline assertion so Alt-Ergo handles the case-split.
  /*@ assert
    _Per_CPU_Information[ 0 ].per_cpu.executing == _Thread_Heir ||
    _Thread_Dispatch_necessary_ghost; */
}
