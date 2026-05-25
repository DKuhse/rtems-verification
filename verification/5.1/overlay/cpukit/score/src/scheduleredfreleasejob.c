/**
 * @file
 *
 * @ingroup RTEMSScoreScheduler
 *
 * @brief Scheduler EDF Release Job
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
  requires PRIORITY_MINIMUM < priority <= PRIORITY_DEFAULT_MAXIMUM;

  assigns \nothing;

  ensures \result == ( SCHEDULER_EDF_PRIO_MSB |
    SCHEDULER_PRIORITY_MAP( priority ) );
  ensures ( \result & SCHEDULER_EDF_PRIO_MSB ) == SCHEDULER_EDF_PRIO_MSB;
*/
Priority_Control _Scheduler_EDF_Map_priority(
  const Scheduler_Control *scheduler,
  Priority_Control         priority
)
{
  return SCHEDULER_EDF_PRIO_MSB | SCHEDULER_PRIORITY_MAP( priority );
}

/*@
  assigns \nothing;

  ensures \result == SCHEDULER_PRIORITY_UNMAP(
    priority & ~SCHEDULER_EDF_PRIO_MSB );
  ensures ( \result & SCHEDULER_EDF_PRIO_MSB ) == 0;
*/
Priority_Control _Scheduler_EDF_Unmap_priority(
  const Scheduler_Control *scheduler,
  Priority_Control         priority
)
{
  return SCHEDULER_PRIORITY_UNMAP( priority & ~SCHEDULER_EDF_PRIO_MSB );
}

/*@
  requires \valid_read( scheduler );
  requires deadline < SCHEDULER_EDF_PRIO_MSB;
  requires \valid( (Scheduler_EDF_Context *) scheduler->context );
  requires edf_ready_context_well_formed{Pre}(
    (Scheduler_EDF_Context *) scheduler->context );
  requires thread_priority_edf_heir_valid{Pre}( _Thread_Heir );
  requires edf_preemptible_heir_is_earliest_ready{Pre}(
    (Scheduler_EDF_Context *) scheduler->context,
    _Thread_Heir,
    _Thread_Heir->is_preemptible );
  requires \valid_read( &the_thread->Scheduler.nodes );
  requires \valid( priority_node );
  requires \valid( queue_context );
  requires \valid( the_thread->Scheduler.nodes );
  requires the_thread->current_state == STATES_READY ==>
    edf_ready_member{Pre}(
      (Scheduler_EDF_Context *) scheduler->context,
      (Scheduler_EDF_Node *) the_thread->Scheduler.nodes );
  requires the_thread->current_state == STATES_READY ==>
    SCHEDULER_PRIORITY_PURIFY( the_thread->Scheduler.nodes->Priority.value ) ==
      ((Scheduler_EDF_Node *)
        the_thread->Scheduler.nodes)->Base.Wait.Priority.Node.priority;
  requires priority_aggregation_well_formed{Pre}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  requires priority_aggregation_cached_minimum{Pre}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  requires priority_node_active_iff_contributor{Pre}(
    &the_thread->Scheduler.nodes->Wait.Priority,
    priority_node );
  requires \exists Priority_Node *node;
    node \in priority_contributors{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
  requires \separated(
    scheduler + (..),
    (Scheduler_EDF_Context *) scheduler->context + (..),
    the_thread + (..),
    the_thread->Scheduler.nodes + (..),
    priority_node + (..),
    queue_context + (..)
  );
  requires \forall Scheduler_EDF_Node *node;
    node \in edf_ready_set{Pre}(
      (Scheduler_EDF_Context *) scheduler->context ) ==>
      \separated( queue_context + (..), node + (..) );
  requires \forall Scheduler_EDF_Node *node;
    node \in edf_ready_set{Pre}(
      (Scheduler_EDF_Context *) scheduler->context ) ==>
      \separated( &node->priority, &priority_node->priority );
  requires \forall Scheduler_EDF_Node *node;
    node \in edf_ready_set{Pre}(
      (Scheduler_EDF_Context *) scheduler->context ) ==>
      \separated(
        &node->priority,
        &the_thread->Scheduler.nodes->Wait.Priority,
        &the_thread->Scheduler.nodes->Priority.value
      );

  assigns priority_node->priority,
          the_thread->Scheduler.nodes->Wait.Priority,
          the_thread->Scheduler.nodes->Priority.value,
          queue_context->Priority;

  ensures priority_node->priority == SCHEDULER_PRIORITY_MAP( deadline );
  ensures priority_aggregation_well_formed{Post}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  ensures priority_aggregation_cached_minimum{Post}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  ensures ((Scheduler_EDF_Context *) scheduler->context)->Ready.rbh_root ==
    \at( ((Scheduler_EDF_Context *) scheduler->context)->Ready.rbh_root, Pre );
  ensures edf_ready_set{Post}(
            (Scheduler_EDF_Context *) scheduler->context ) ==
          edf_ready_set{Pre}(
            (Scheduler_EDF_Context *) scheduler->context );
  ensures edf_ready_context_well_formed{Post}(
    (Scheduler_EDF_Context *) scheduler->context );
  ensures thread_priority_edf_heir_valid{Post}( _Thread_Heir );
  ensures edf_preemptible_heir_is_earliest_ready{Post}(
    (Scheduler_EDF_Context *) scheduler->context,
    _Thread_Heir,
    _Thread_Heir->is_preemptible );
  ensures _Thread_Heir == \at( _Thread_Heir, Pre );
  ensures _Per_CPU_Information[ 0 ].per_cpu.heir ==
    \at( _Per_CPU_Information[ 0 ].per_cpu.heir, Pre );
  ensures the_thread->Scheduler.nodes->Priority.value !=
            \at( the_thread->Scheduler.nodes->Priority.value, Pre ) ==>
          thread_priority_update_pending{Post}( queue_context, the_thread );
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 ==>
          queue_context->Priority.update_count <= 1;
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 0 ==>
          queue_context->Priority.update[ 0 ] ==
            \at( queue_context->Priority.update[ 0 ], Pre );
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 0 ==>
          queue_context->Priority.update[ 0 ]->Scheduler.nodes ==
            \at( queue_context->Priority.update[ 0 ]->Scheduler.nodes, Pre );
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 1 ==>
          queue_context->Priority.update[ 0 ] == the_thread;
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 1 &&
          the_thread->current_state == STATES_READY ==>
          edf_ready_member{Post}(
            (Scheduler_EDF_Context *) scheduler->context,
            (Scheduler_EDF_Node *) the_thread->Scheduler.nodes );
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 1 &&
          the_thread->current_state == STATES_READY ==>
          SCHEDULER_PRIORITY_PURIFY(
            the_thread->Scheduler.nodes->Priority.value ) ==
          ((Scheduler_EDF_Node *) the_thread->Scheduler.nodes)->
            Base.Wait.Priority.Node.priority;
  ensures queue_context->Priority.update_count == 0 &&
          the_thread->current_state == STATES_READY &&
          \at( edf_ready_node_cache_consistent(
            (Scheduler_EDF_Node *) the_thread->Scheduler.nodes ), Pre ) ==>
          edf_ready_node_cache_consistent{Post}(
            (Scheduler_EDF_Node *) the_thread->Scheduler.nodes );

  behavior active:
    assumes priority_node_active{Pre}( priority_node );
    ensures priority_contributors{Post}(
              &the_thread->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors{Pre}(
              &the_thread->Scheduler.nodes->Wait.Priority );

  behavior active_noop:
    assumes priority_node_active{Pre}( priority_node );
    assumes \valid_read( &the_thread->Wait.operations );
    assumes \valid( the_thread->Wait.operations );
    assumes the_thread->Wait.operations->priority_actions ==
      _Thread_queue_Do_nothing_priority_actions;
    assumes queue_context->Priority.update_count <= 1;
    assumes \valid( _Priority_Verify_scheduler_node_of_aggregation(
      &the_thread->Scheduler.nodes->Wait.Priority ) );
    assumes &the_thread->Scheduler.nodes->Wait.Priority ==
      &_Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority )->Wait.Priority;
    assumes (uintptr_t) &the_thread->Scheduler.nodes->Wait.Priority >=
      _Priority_Verify_wait_priority_node_offset;
    assumes (uintptr_t) &the_thread->Scheduler.nodes->Wait.Priority
      <= UINTPTR_MAX;
    assumes \separated(
      &queue_context->Priority.Actions,
      priority_node + (..),
      _Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority ) + (..)
    );
    assumes \separated(
      &queue_context->Priority.Actions.actions,
      &queue_context->Priority.update_count,
      queue_context->Priority.update + (0 .. 1),
      &priority_node->priority,
      &the_thread->Scheduler.nodes->Wait.Priority.Contributors,
      &the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
      &_Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
    );
    assumes \separated(
      the_thread->Wait.operations + (..),
      queue_context + (..),
      the_thread->Scheduler.nodes + (..),
      priority_node + (..)
    );
    assumes \forall Priority_Node *contributor;
      contributor \in priority_contributors{Pre}(
        &the_thread->Scheduler.nodes->Wait.Priority ) ==>
        \separated(
          contributor + (..),
          &queue_context->Priority.Actions.actions,
          &_Priority_Verify_scheduler_node_of_aggregation(
            &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
        );
    ensures priority_contributors{Post}(
              &the_thread->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors{Pre}(
              &the_thread->Scheduler.nodes->Wait.Priority );
    ensures queue_context->Priority.Actions.actions == \null;

  behavior inactive:
    assumes !priority_node_active{Pre}( priority_node );
    ensures priority_contributors{Post}(
              &the_thread->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors_insert(
              priority_contributors{Pre}(
                &the_thread->Scheduler.nodes->Wait.Priority ),
              priority_node );

  behavior inactive_noop:
    assumes !priority_node_active{Pre}( priority_node );
    assumes \valid_read( &the_thread->Wait.operations );
    assumes \valid( the_thread->Wait.operations );
    assumes the_thread->Wait.operations->priority_actions ==
      _Thread_queue_Do_nothing_priority_actions;
    assumes queue_context->Priority.update_count <= 1;
    assumes \valid( _Priority_Verify_scheduler_node_of_aggregation(
      &the_thread->Scheduler.nodes->Wait.Priority ) );
    assumes &the_thread->Scheduler.nodes->Wait.Priority ==
      &_Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority )->Wait.Priority;
    assumes (uintptr_t) &the_thread->Scheduler.nodes->Wait.Priority >=
      _Priority_Verify_wait_priority_node_offset;
    assumes (uintptr_t) &the_thread->Scheduler.nodes->Wait.Priority
      <= UINTPTR_MAX;
    assumes \separated(
      &queue_context->Priority.Actions,
      priority_node + (..),
      _Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority ) + (..)
    );
    assumes \separated(
      &queue_context->Priority.Actions.actions,
      &queue_context->Priority.update_count,
      queue_context->Priority.update + (0 .. 1),
      &priority_node->priority,
      &the_thread->Scheduler.nodes->Wait.Priority.Contributors,
      &the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
      &_Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
    );
    assumes \separated(
      the_thread->Wait.operations + (..),
      queue_context + (..),
      the_thread->Scheduler.nodes + (..),
      priority_node + (..)
    );
    assumes \forall Priority_Node *contributor;
      contributor \in priority_contributors{Pre}(
        &the_thread->Scheduler.nodes->Wait.Priority ) ==>
        \separated(
          contributor + (..),
          &queue_context->Priority.Actions.actions,
          &_Priority_Verify_scheduler_node_of_aggregation(
            &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
        );
    ensures priority_contributors{Post}(
              &the_thread->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors_insert(
              priority_contributors{Pre}(
                &the_thread->Scheduler.nodes->Wait.Priority ),
              priority_node );
    ensures priority_aggregation_well_formed{Post}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    ensures priority_aggregation_cached_minimum{Post}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    ensures queue_context->Priority.Actions.actions == \null;

  complete behaviors active, inactive;
  disjoint behaviors active, inactive;
*/
void _Scheduler_EDF_Release_job(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Priority_Node           *priority_node,
  uint64_t                 deadline,
  Thread_queue_Context    *queue_context
)
{
  (void) scheduler;

  _Thread_Wait_acquire_critical( the_thread, queue_context );

  /*
   * There is no integer overflow problem here due to the
   * SCHEDULER_PRIORITY_MAP().  The deadline is in clock ticks.  With the
   * minimum clock tick interval of 1us, the uptime is limited to about 146235
   * years.
   */
  _Priority_Node_set_priority(
    priority_node,
    SCHEDULER_PRIORITY_MAP( deadline )
  );

  if ( _Priority_Node_is_active( priority_node ) ) {
    _Thread_Priority_changed(
      the_thread,
      priority_node,
      false,
      queue_context
    );
  } else {
    _Thread_Priority_add( the_thread, priority_node, queue_context );
  }

  /*@ assert \forall Scheduler_Node *node;
        \valid_read( &node->owner ) ==>
          node->owner == \at( node->owner, Pre ); */
  /*@ assert \forall Thread_Control *thread;
        \valid_read( &thread->Scheduler.nodes ) ==>
          thread->Scheduler.nodes == \at( thread->Scheduler.nodes, Pre ); */
  /*@ assert edf_ready_owners_canonical{Pre}(
        edf_ready_set{Pre}(
          (Scheduler_EDF_Context *) scheduler->context ) ); */
  /*@ assert \forall Scheduler_EDF_Node *node;
        node \in edf_ready_set{Pre}(
          (Scheduler_EDF_Context *) scheduler->context ) ==>
          node->Base.owner == \at( node->Base.owner, Pre ); */
  /*@ assert \forall Scheduler_EDF_Node *node;
        node \in edf_ready_set{Pre}(
          (Scheduler_EDF_Context *) scheduler->context ) ==>
          node->Base.owner->Scheduler.nodes ==
            \at( node->Base.owner->Scheduler.nodes, Pre ); */
  /*@ assert edf_ready_set{Here}(
        (Scheduler_EDF_Context *) scheduler->context ) ==
        edf_ready_set{Pre}(
          (Scheduler_EDF_Context *) scheduler->context ); */
  /*@ assert edf_ready_valid_nodes{Here}(
        edf_ready_set{Here}(
          (Scheduler_EDF_Context *) scheduler->context ) ); */
  /*@ assert edf_ready_owners_distinct{Here}(
        edf_ready_set{Here}(
          (Scheduler_EDF_Context *) scheduler->context ) ); */
  /*@ assert \forall Scheduler_EDF_Node *node;
        node \in edf_ready_set{Here}(
          (Scheduler_EDF_Context *) scheduler->context ) ==>
        node \in edf_ready_set{Pre}(
          (Scheduler_EDF_Context *) scheduler->context ); */
  /*@ assert \forall Scheduler_EDF_Node *node;
        node \in edf_ready_set{Here}(
          (Scheduler_EDF_Context *) scheduler->context ) ==>
          node->Base.owner != \null; */
  /*@ assert \forall Scheduler_EDF_Node *node;
        node \in edf_ready_set{Here}(
          (Scheduler_EDF_Context *) scheduler->context ) ==>
          \valid_read( &node->Base.owner->Scheduler.nodes ); */
  /*@ assert \forall Scheduler_EDF_Node *node;
        node \in edf_ready_set{Here}(
          (Scheduler_EDF_Context *) scheduler->context ) ==>
          node->Base.owner->Scheduler.nodes == &node->Base; */
  /*@ assert \forall Scheduler_EDF_Node *node;
        node \in edf_ready_set{Here}(
          (Scheduler_EDF_Context *) scheduler->context ) ==>
          edf_ready_node_has_canonical_owner{Here}( node ); */
  /*@ assert edf_ready_owners_canonical{Here}(
        edf_ready_set{Here}(
          (Scheduler_EDF_Context *) scheduler->context ) ); */
  /*@ assert edf_ready_context_well_formed{Here}(
        (Scheduler_EDF_Context *) scheduler->context ); */
  /*@ assert \forall Scheduler_EDF_Node *node;
        node \in edf_ready_set{Pre}(
          (Scheduler_EDF_Context *) scheduler->context ) ==>
          \separated( &node->priority, &priority_node->priority ); */
  /*@ assert \forall Scheduler_EDF_Node *node;
        node \in edf_ready_set{Pre}(
          (Scheduler_EDF_Context *) scheduler->context ) ==>
          \separated(
            &node->priority,
            &the_thread->Scheduler.nodes->Wait.Priority
          ); */
  /*@ assert \forall Scheduler_EDF_Node *node;
        node \in edf_ready_set{Pre}(
          (Scheduler_EDF_Context *) scheduler->context ) ==>
          \separated(
            &node->priority,
            &the_thread->Scheduler.nodes->Priority.value
          ); */
  /*@ assert \forall Scheduler_EDF_Node *node;
        node \in edf_ready_set{Pre}(
          (Scheduler_EDF_Context *) scheduler->context ) ==>
          \separated( &node->priority, &queue_context->Priority ); */
  /*@ assert \forall Scheduler_EDF_Node *node;
        node \in edf_ready_set{Pre}(
          (Scheduler_EDF_Context *) scheduler->context ) ==>
          node->priority == \at( node->priority, Pre ); */
  /*@ assert \forall Scheduler_EDF_Node *node;
        node \in edf_ready_set{Pre}(
          (Scheduler_EDF_Context *) scheduler->context ) ==>
          \at( node->priority, Here ) == \at( node->priority, Pre ); */
  /*@ assert \forall Scheduler_EDF_Node *node;
        node \in edf_ready_set{Pre}(
          (Scheduler_EDF_Context *) scheduler->context ) ==>
          \at( node->Base.owner, Here ) == \at( node->Base.owner, Pre ); */
  /*@ assert _Thread_Heir == \at( _Thread_Heir, Pre ); */
  /*@ assert _Thread_Heir->is_preemptible ==
        \at( _Thread_Heir->is_preemptible, Pre ); */
  /*@ assert edf_preemptible_heir_is_earliest_ready{Pre}(
        (Scheduler_EDF_Context *) scheduler->context,
        _Thread_Heir,
        _Thread_Heir->is_preemptible ); */
  /*@ assert edf_preemptible_heir_is_earliest_ready{Here}(
        (Scheduler_EDF_Context *) scheduler->context,
        _Thread_Heir,
        _Thread_Heir->is_preemptible ); */
  _Thread_Wait_release_critical( the_thread, queue_context );
  /*@ assert queue_context->Priority.update_count == 0 &&
        the_thread->current_state == STATES_READY &&
        \at( edf_ready_node_cache_consistent(
          (Scheduler_EDF_Node *) the_thread->Scheduler.nodes ), Pre ) ==>
        edf_ready_node_cache_consistent{Here}(
          (Scheduler_EDF_Node *) the_thread->Scheduler.nodes ); */
}

/*@
  requires \valid_read( scheduler );
  requires \valid( (Scheduler_EDF_Context *) scheduler->context );
  requires edf_ready_context_well_formed{Pre}(
    (Scheduler_EDF_Context *) scheduler->context );
  requires thread_priority_edf_heir_valid{Pre}( _Thread_Heir );
  requires edf_preemptible_heir_is_earliest_ready{Pre}(
    (Scheduler_EDF_Context *) scheduler->context,
    _Thread_Heir,
    _Thread_Heir->is_preemptible );
  requires \valid_read( &the_thread->Scheduler.nodes );
  requires \valid( priority_node );
  requires \valid( queue_context );
  requires \valid( the_thread->Scheduler.nodes );
  requires the_thread->current_state == STATES_READY ==>
    edf_ready_member{Pre}(
      (Scheduler_EDF_Context *) scheduler->context,
      (Scheduler_EDF_Node *) the_thread->Scheduler.nodes );
  requires the_thread->current_state == STATES_READY ==>
    SCHEDULER_PRIORITY_PURIFY( the_thread->Scheduler.nodes->Priority.value ) ==
      ((Scheduler_EDF_Node *)
        the_thread->Scheduler.nodes)->Base.Wait.Priority.Node.priority;
  requires priority_aggregation_well_formed{Pre}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  requires priority_aggregation_cached_minimum{Pre}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  requires priority_node_active_iff_contributor{Pre}(
    &the_thread->Scheduler.nodes->Wait.Priority,
    priority_node );
  requires priority_node_active{Pre}( priority_node ) ==>
    ( \exists Priority_Node *other;
        other != priority_node &&
        other \in priority_contributors{Pre}(
          &the_thread->Scheduler.nodes->Wait.Priority ) );
  requires \separated(
    scheduler + (..),
    (Scheduler_EDF_Context *) scheduler->context + (..),
    the_thread + (..),
    the_thread->Scheduler.nodes + (..),
    priority_node + (..),
    queue_context + (..)
  );
  requires \forall Scheduler_EDF_Node *node;
    node \in edf_ready_set{Pre}(
      (Scheduler_EDF_Context *) scheduler->context ) ==>
      \separated( queue_context + (..), node + (..) );

  assigns priority_node->Node.RBTree.Node.rbe_color,
          the_thread->Scheduler.nodes->Wait.Priority,
          the_thread->Scheduler.nodes->Priority.value,
          queue_context->Priority;

  ensures !priority_node_active{Post}( priority_node );
  ensures priority_aggregation_well_formed{Post}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  ensures priority_aggregation_cached_minimum{Post}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  ensures ((Scheduler_EDF_Context *) scheduler->context)->Ready.rbh_root ==
    \at( ((Scheduler_EDF_Context *) scheduler->context)->Ready.rbh_root, Pre );
  ensures edf_ready_set{Post}(
            (Scheduler_EDF_Context *) scheduler->context ) ==
          edf_ready_set{Pre}(
            (Scheduler_EDF_Context *) scheduler->context );
  ensures edf_ready_context_well_formed{Post}(
    (Scheduler_EDF_Context *) scheduler->context );
  ensures thread_priority_edf_heir_valid{Post}( _Thread_Heir );
  ensures edf_preemptible_heir_is_earliest_ready{Post}(
    (Scheduler_EDF_Context *) scheduler->context,
    _Thread_Heir,
    _Thread_Heir->is_preemptible );
  ensures the_thread->Scheduler.nodes->Priority.value !=
            \at( the_thread->Scheduler.nodes->Priority.value, Pre ) ==>
          thread_priority_update_pending{Post}( queue_context, the_thread );
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 ==>
          queue_context->Priority.update_count <= 1;
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 0 ==>
          queue_context->Priority.update[ 0 ] ==
            \at( queue_context->Priority.update[ 0 ], Pre );
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 0 ==>
          queue_context->Priority.update[ 0 ]->Scheduler.nodes ==
            \at( queue_context->Priority.update[ 0 ]->Scheduler.nodes, Pre );
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 1 ==>
          queue_context->Priority.update[ 0 ] == the_thread;
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 1 &&
          the_thread->current_state == STATES_READY ==>
          edf_ready_member{Post}(
            (Scheduler_EDF_Context *) scheduler->context,
            (Scheduler_EDF_Node *) the_thread->Scheduler.nodes );
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 1 &&
          the_thread->current_state == STATES_READY ==>
          SCHEDULER_PRIORITY_PURIFY(
            the_thread->Scheduler.nodes->Priority.value ) ==
          ((Scheduler_EDF_Node *) the_thread->Scheduler.nodes)->
            Base.Wait.Priority.Node.priority;
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 0 &&
          the_thread->current_state == STATES_READY &&
          \at( edf_ready_node_cache_consistent(
            (Scheduler_EDF_Node *) the_thread->Scheduler.nodes ), Pre ) ==>
          edf_ready_node_cache_consistent{Post}(
            (Scheduler_EDF_Node *) the_thread->Scheduler.nodes );

  behavior active:
    assumes priority_node_active{Pre}( priority_node );
    ensures priority_contributors{Post}(
              &the_thread->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors_extract(
              priority_contributors{Pre}(
                &the_thread->Scheduler.nodes->Wait.Priority ),
              priority_node );

  behavior active_noop:
    assumes priority_node_active{Pre}( priority_node );
    assumes \valid_read( &the_thread->Wait.operations );
    assumes \valid( the_thread->Wait.operations );
    assumes the_thread->Wait.operations->priority_actions ==
      _Thread_queue_Do_nothing_priority_actions;
    assumes queue_context->Priority.update_count <= 1;
    assumes \valid( _Priority_Verify_scheduler_node_of_aggregation(
      &the_thread->Scheduler.nodes->Wait.Priority ) );
    assumes &the_thread->Scheduler.nodes->Wait.Priority ==
      &_Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority )->Wait.Priority;
    assumes (uintptr_t) &the_thread->Scheduler.nodes->Wait.Priority >=
      _Priority_Verify_wait_priority_node_offset;
    assumes (uintptr_t) &the_thread->Scheduler.nodes->Wait.Priority
      <= UINTPTR_MAX;
    assumes \separated(
      &queue_context->Priority.Actions,
      priority_node + (..),
      _Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority ) + (..)
    );
    assumes \separated(
      &queue_context->Priority.Actions.actions,
      &queue_context->Priority.update_count,
      queue_context->Priority.update + (0 .. 1),
      &priority_node->priority,
      &the_thread->Scheduler.nodes->Wait.Priority.Contributors,
      &the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
      &_Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
    );
    assumes \separated(
      the_thread->Wait.operations + (..),
      queue_context + (..),
      the_thread->Scheduler.nodes + (..),
      priority_node + (..)
    );
    assumes \forall Priority_Node *contributor;
      contributor \in priority_contributors{Pre}(
        &the_thread->Scheduler.nodes->Wait.Priority ) ==>
        \separated(
          contributor + (..),
          &queue_context->Priority.Actions.actions,
          &_Priority_Verify_scheduler_node_of_aggregation(
            &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
        );
    ensures priority_contributors{Post}(
              &the_thread->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors_extract(
              priority_contributors{Pre}(
                &the_thread->Scheduler.nodes->Wait.Priority ),
              priority_node );
    ensures queue_context->Priority.Actions.actions == \null;

  behavior inactive:
    assumes !priority_node_active{Pre}( priority_node );
    ensures priority_contributors{Post}(
              &the_thread->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors{Pre}(
              &the_thread->Scheduler.nodes->Wait.Priority );

  complete behaviors active, inactive;
  disjoint behaviors active, inactive;
*/
void _Scheduler_EDF_Cancel_job(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Priority_Node           *priority_node,
  Thread_queue_Context    *queue_context
)
{
  (void) scheduler;

  _Thread_Wait_acquire_critical( the_thread, queue_context );

  if ( _Priority_Node_is_active( priority_node ) ) {
    _Thread_Priority_remove( the_thread, priority_node, queue_context );
    _Priority_Node_set_inactive( priority_node );
  }

  _Thread_Wait_release_critical( the_thread, queue_context );
  /*@ assert queue_context->Priority.update_count == 0 &&
        the_thread->current_state == STATES_READY &&
        \at( edf_ready_node_cache_consistent(
          (Scheduler_EDF_Node *) the_thread->Scheduler.nodes ), Pre ) ==>
        edf_ready_node_cache_consistent{Here}(
          (Scheduler_EDF_Node *) the_thread->Scheduler.nodes ); */
}
