/**
 * @file
 *
 * @brief Changes the Priority of a Thread
 *
 * @ingroup RTEMSScoreThread
 */

/*
 *  COPYRIGHT (c) 1989-2014.
 *  On-Line Applications Research Corporation (OAR).
 *
 *  Copyright (c) 2013, 2016 embedded brains GmbH
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

#include <rtems/score/threadimpl.h>
#include <rtems/score/assert.h>
#include <rtems/score/schedulerimpl.h>

#ifdef __FRAMAC__
/*@
  requires \valid( priority_actions );
  terminates \true;
  exits \false;
  assigns priority_actions->actions \from \nothing;
  ensures priority_actions->actions == \null;
*/
void _Thread_queue_Do_nothing_priority_actions(
  Thread_queue_Queue *queue,
  Priority_Actions   *priority_actions
);
#endif

/*@
  requires \valid( priority_aggregation );
  requires \valid( _Priority_Verify_scheduler_node_of_aggregation(
    priority_aggregation ) );
  requires priority_aggregation ==
    &_Priority_Verify_scheduler_node_of_aggregation(
      priority_aggregation )->Wait.Priority;
  requires (uintptr_t) priority_aggregation >=
    _Priority_Verify_wait_priority_node_offset;
  requires (uintptr_t) priority_aggregation <= UINTPTR_MAX;
  requires \forall Priority_Node *node;
    node \in priority_contributors{Pre}( priority_aggregation ) ==>
      \separated(
        node + (..),
        &_Priority_Verify_scheduler_node_of_aggregation(
          priority_aggregation )->Priority.value
      );

  assigns _Priority_Verify_scheduler_node_of_aggregation(
    priority_aggregation )->Priority.value;

  // set scheduler node priority to priority aggregation minimum
  ensures _Priority_Verify_scheduler_node_of_aggregation(
    priority_aggregation )->Priority.value ==
      ( priority_aggregation->Node.priority |
        (Priority_Control) ( prepend_it ? 0 : SCHEDULER_PRIORITY_APPEND_FLAG ) );

  // preservation
  ensures priority_aggregation->Node.priority ==
    \at( priority_aggregation->Node.priority, Pre );
  ensures priority_contributors{Post}( priority_aggregation ) ==
    priority_contributors{Pre}( priority_aggregation );
  ensures priority_aggregation_well_formed{Pre}( priority_aggregation ) ==>
    priority_aggregation_well_formed{Post}( priority_aggregation );
  ensures priority_aggregation_cached_minimum{Pre}( priority_aggregation ) ==>
    priority_aggregation_cached_minimum{Post}( priority_aggregation );
*/
static void _Thread_Set_scheduler_node_priority(
  Priority_Aggregation *priority_aggregation,
  bool                  prepend_it
)
{
  _Scheduler_Node_set_priority(
    SCHEDULER_NODE_OF_WAIT_PRIORITY_NODE( priority_aggregation ),
    _Priority_Get_priority( priority_aggregation ),
    prepend_it
  );
}

#if defined(RTEMS_SMP)
static void _Thread_Priority_action_add(
  Priority_Aggregation *priority_aggregation,
  Priority_Actions     *priority_actions,
  void                 *arg
)
{
  Scheduler_Node *scheduler_node;
  Thread_Control *the_thread;

  scheduler_node = SCHEDULER_NODE_OF_WAIT_PRIORITY( priority_aggregation );
  the_thread = arg;

  _Thread_Scheduler_add_wait_node( the_thread, scheduler_node );
  _Thread_Set_scheduler_node_priority( priority_aggregation, false );
  _Priority_Set_action_type( priority_aggregation, PRIORITY_ACTION_ADD );
  _Priority_Actions_add( priority_actions, priority_aggregation );
}

static void _Thread_Priority_action_remove(
  Priority_Aggregation *priority_aggregation,
  Priority_Actions     *priority_actions,
  void                 *arg
)
{
  Scheduler_Node *scheduler_node;
  Thread_Control *the_thread;

  scheduler_node = SCHEDULER_NODE_OF_WAIT_PRIORITY( priority_aggregation );
  the_thread = arg;

  _Thread_Scheduler_remove_wait_node( the_thread, scheduler_node );
  _Thread_Set_scheduler_node_priority( priority_aggregation, true );
  _Priority_Set_action_type( priority_aggregation, PRIORITY_ACTION_REMOVE );
  _Priority_Actions_add( priority_actions, priority_aggregation );
}
#endif

/*@
  requires \valid( priority_aggregation );
  requires \valid( priority_actions );
  requires \valid( _Priority_Verify_scheduler_node_of_aggregation(
    priority_aggregation ) );
  requires priority_aggregation ==
    &_Priority_Verify_scheduler_node_of_aggregation(
      priority_aggregation )->Wait.Priority;
  requires (uintptr_t) priority_aggregation >=
    _Priority_Verify_wait_priority_node_offset;
  requires (uintptr_t) priority_aggregation <= UINTPTR_MAX;
  requires \forall Priority_Node *node;
    node \in priority_contributors{Pre}( priority_aggregation ) ==>
      \separated(
        node + (..),
        &_Priority_Verify_scheduler_node_of_aggregation(
          priority_aggregation )->Priority.value,
        &priority_actions->actions
      );
  requires \separated(
    &priority_actions->actions,
    &priority_aggregation->Contributors,
    &priority_aggregation->Node.priority,
    &_Priority_Verify_scheduler_node_of_aggregation(
      priority_aggregation )->Priority.value
  );
  requires \separated(
    priority_actions + (..),
    _Priority_Verify_scheduler_node_of_aggregation(
      priority_aggregation ) + (..)
  );

  assigns _Priority_Verify_scheduler_node_of_aggregation(
            priority_aggregation )->Priority.value,
          priority_actions->actions;

  ensures priority_actions->actions == priority_aggregation;
  ensures _Priority_Verify_scheduler_node_of_aggregation(
    priority_aggregation )->Priority.value ==
      ( priority_aggregation->Node.priority |
        (Priority_Control) ( prepend_it ? 0 : SCHEDULER_PRIORITY_APPEND_FLAG ) );
  ensures priority_aggregation->Node.priority ==
    \at( priority_aggregation->Node.priority, Pre );
  ensures priority_contributors{Post}( priority_aggregation ) ==
    priority_contributors{Pre}( priority_aggregation );
  ensures priority_aggregation_well_formed{Pre}( priority_aggregation ) ==>
    priority_aggregation_well_formed{Post}( priority_aggregation );
  ensures priority_aggregation_cached_minimum{Pre}( priority_aggregation ) ==>
    priority_aggregation_cached_minimum{Post}( priority_aggregation );
*/
static void _Thread_Priority_action_change(
  Priority_Aggregation *priority_aggregation,
  bool                  prepend_it,
  Priority_Actions     *priority_actions,
  void                 *arg
)
{
  _Thread_Set_scheduler_node_priority( priority_aggregation, prepend_it );
#if defined(RTEMS_SMP) || defined(RTEMS_DEBUG)
  _Priority_Set_action_type( priority_aggregation, PRIORITY_ACTION_CHANGE );
#endif
  _Priority_Actions_add( priority_actions, priority_aggregation );
}

#if !defined(RTEMS_SMP)
/*@
  requires \valid_read( &the_thread->Scheduler.nodes );
  requires \valid( the_thread->Scheduler.nodes );
  requires \valid( queue_context );
  requires \valid( operations );
  requires operations->priority_actions ==
    _Thread_queue_Do_nothing_priority_actions;
  requires queue_context->Priority.update_count <= 1;
  requires queue_context->Priority.Actions.actions ==
    &the_thread->Scheduler.nodes->Wait.Priority;
  requires \valid( queue_context->Priority.Actions.actions );
  requires \valid( queue_context->Priority.Actions.actions->Action.node );
  requires queue_context->Priority.Actions.actions->Action.type ==
             PRIORITY_ACTION_ADD ||
           queue_context->Priority.Actions.actions->Action.type ==
             PRIORITY_ACTION_REMOVE ||
           queue_context->Priority.Actions.actions->Action.type ==
             PRIORITY_ACTION_CHANGE;
  requires \valid( _Priority_Verify_scheduler_node_of_aggregation(
    &the_thread->Scheduler.nodes->Wait.Priority ) );
  requires &the_thread->Scheduler.nodes->Wait.Priority ==
    &_Priority_Verify_scheduler_node_of_aggregation(
      &the_thread->Scheduler.nodes->Wait.Priority )->Wait.Priority;
  requires (uintptr_t) &the_thread->Scheduler.nodes->Wait.Priority >=
    _Priority_Verify_wait_priority_node_offset;
  requires (uintptr_t) &the_thread->Scheduler.nodes->Wait.Priority
    <= UINTPTR_MAX;
  requires \separated(
    &queue_context->Priority.Actions,
    queue_context->Priority.Actions.actions->Action.node + (..),
    _Priority_Verify_scheduler_node_of_aggregation(
      &the_thread->Scheduler.nodes->Wait.Priority ) + (..)
  );
  requires \separated(
    &queue_context->Priority.Actions.actions,
    &queue_context->Priority.update_count,
    queue_context->Priority.update + (0 .. 1),
    &queue_context->Priority.Actions.actions->Action.node->priority,
    &the_thread->Scheduler.nodes->Wait.Priority.Contributors,
    &the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
    &_Priority_Verify_scheduler_node_of_aggregation(
      &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
  );
  requires \separated(
    operations + (..),
    queue_context + (..),
    the_thread->Scheduler.nodes + (..),
    queue_context->Priority.Actions.actions->Action.node + (..)
  );
  requires \forall Priority_Node *contributor;
    contributor \in priority_contributors{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority ) ==>
      \separated(
        contributor + (..),
        &queue_context->Priority.Actions.actions,
        &_Priority_Verify_scheduler_node_of_aggregation(
          &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
      );

  assigns the_thread->Scheduler.nodes->Wait.Priority,
          the_thread->Scheduler.nodes->Priority.value,
          queue_context->Priority;
  allocates \nothing;
  frees \nothing;

  ensures queue_context->Priority.Actions.actions == \null;
  ensures queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) ||
          queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) + 1;
  ensures queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) + 1 ==>
          queue_context->Priority.update[
            \at( queue_context->Priority.update_count, Pre )
          ] == the_thread;
  ensures queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) + 1 ==>
          SCHEDULER_PRIORITY_PURIFY(
            the_thread->Scheduler.nodes->Priority.value ) ==
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority;

  behavior add:
    assumes queue_context->Priority.Actions.actions->Action.type ==
      PRIORITY_ACTION_ADD;
    requires priority_aggregation_well_formed{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    requires priority_aggregation_cached_minimum{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    requires !priority_contributor_member{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority,
      queue_context->Priority.Actions.actions->Action.node );
    requires \exists Priority_Node *node;
      node \in priority_contributors{Pre}(
        &the_thread->Scheduler.nodes->Wait.Priority );
    ensures priority_contributors{Post}(
              \at( queue_context->Priority.Actions.actions, Pre ) ) ==
            priority_contributors_insert(
              priority_contributors{Pre}(
                \at( queue_context->Priority.Actions.actions, Pre ) ),
              \at( queue_context->Priority.Actions.actions->Action.node, Pre )
            );
    ensures priority_aggregation_well_formed{Post}(
      \at( queue_context->Priority.Actions.actions, Pre ) );
    ensures priority_aggregation_cached_minimum{Post}(
      \at( queue_context->Priority.Actions.actions, Pre ) );

  behavior remove:
    assumes queue_context->Priority.Actions.actions->Action.type ==
      PRIORITY_ACTION_REMOVE;
    requires priority_aggregation_well_formed{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    requires priority_aggregation_cached_minimum{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    requires priority_contributor_member{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority,
      queue_context->Priority.Actions.actions->Action.node );
    requires \exists Priority_Node *other;
      other != queue_context->Priority.Actions.actions->Action.node &&
      other \in priority_contributors{Pre}(
        &the_thread->Scheduler.nodes->Wait.Priority );
    requires \exists Priority_Node *remaining;
      remaining \in priority_contributors_extract(
        priority_contributors{Pre}(
          &the_thread->Scheduler.nodes->Wait.Priority ),
        queue_context->Priority.Actions.actions->Action.node
      );
    ensures priority_contributors{Post}(
              \at( queue_context->Priority.Actions.actions, Pre ) ) ==
            priority_contributors_extract(
              priority_contributors{Pre}(
                \at( queue_context->Priority.Actions.actions, Pre ) ),
              \at( queue_context->Priority.Actions.actions->Action.node, Pre )
            );

  behavior change:
    assumes queue_context->Priority.Actions.actions->Action.type ==
      PRIORITY_ACTION_CHANGE;
    requires priority_aggregation_well_formed{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    requires priority_contributor_member{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority,
      queue_context->Priority.Actions.actions->Action.node );
    ensures priority_contributors{Post}(
              \at( queue_context->Priority.Actions.actions, Pre ) ) ==
            priority_contributors{Pre}(
              \at( queue_context->Priority.Actions.actions, Pre ) );
    ensures priority_aggregation_well_formed{Post}(
      \at( queue_context->Priority.Actions.actions, Pre ) );
    ensures priority_aggregation_cached_minimum{Post}(
      \at( queue_context->Priority.Actions.actions, Pre ) );

  complete behaviors;
  disjoint behaviors;
*/
#endif
static void _Thread_Priority_do_perform_actions(
  Thread_Control                *the_thread,
  Thread_queue_Queue            *queue,
  const Thread_queue_Operations *operations,
  bool                           prepend_it,
  Thread_queue_Context          *queue_context
)
{
  Priority_Aggregation *priority_aggregation;

  _Assert( !_Priority_Actions_is_empty( &queue_context->Priority.Actions ) );
  priority_aggregation = _Priority_Actions_move( &queue_context->Priority.Actions );
  /*@ ghost Priority_Aggregation *original_aggregation = priority_aggregation; */
  /*@ assert original_aggregation ==
        \at( queue_context->Priority.Actions.actions, Pre ); */
  /*@ assert original_aggregation ==
        &the_thread->Scheduler.nodes->Wait.Priority; */
  /*@ assert priority_contributors{Here}( original_aggregation ) ==
        priority_contributors{Pre}( original_aggregation ); */
  /*@ assert operations->priority_actions ==
        _Thread_queue_Do_nothing_priority_actions; */

  do {
    Priority_Aggregation *next_aggregation;
    Priority_Node        *priority_action_node;
    Priority_Action_type  priority_action_type;

    next_aggregation = _Priority_Get_next_action( priority_aggregation );

    priority_action_node = priority_aggregation->Action.node;
    /*@ assert priority_action_node ==
          \at( queue_context->Priority.Actions.actions->Action.node, Pre ); */
    priority_action_type = priority_aggregation->Action.type;

    switch ( priority_action_type ) {
      case PRIORITY_ACTION_ADD:
#if defined(RTEMS_SMP)
        _Priority_Insert(
          priority_aggregation,
          priority_action_node,
          &queue_context->Priority.Actions,
          _Thread_Priority_action_add,
          _Thread_Priority_action_change,
          the_thread
        );
#else
        _Priority_Non_empty_insert(
          priority_aggregation,
          priority_action_node,
          &queue_context->Priority.Actions,
          _Thread_Priority_action_change,
          NULL
        );
#endif
        /*@ assert priority_contributors{Here}( original_aggregation ) ==
              priority_contributors_insert(
                priority_contributors{Pre}( original_aggregation ),
                priority_action_node
              ); */
        /*@ assert priority_aggregation_well_formed{Here}(
              original_aggregation ); */
        /*@ assert priority_aggregation_cached_minimum{Here}(
              original_aggregation ); */
        /*@ assert operations->priority_actions ==
              _Thread_queue_Do_nothing_priority_actions; */
        break;
      case PRIORITY_ACTION_REMOVE:
#if defined(RTEMS_SMP)
        _Priority_Extract(
          priority_aggregation,
          priority_action_node,
          &queue_context->Priority.Actions,
          _Thread_Priority_action_remove,
          _Thread_Priority_action_change,
          the_thread
        );
#else
        _Priority_Extract_non_empty(
          priority_aggregation,
          priority_action_node,
          &queue_context->Priority.Actions,
          _Thread_Priority_action_change,
          NULL
        );
#endif
        /*@ assert priority_contributors{Here}( original_aggregation ) ==
              priority_contributors_extract(
                priority_contributors{Pre}( original_aggregation ),
                priority_action_node
              ); */
        /*@ assert priority_aggregation_well_formed{Here}(
              original_aggregation ); */
        /*@ assert priority_aggregation_cached_minimum{Here}(
              original_aggregation ); */
        /*@ assert operations->priority_actions ==
              _Thread_queue_Do_nothing_priority_actions; */
        break;
      default:
        _Assert( priority_action_type == PRIORITY_ACTION_CHANGE );
        _Priority_Changed(
          priority_aggregation,
          priority_action_node,
          prepend_it,
          &queue_context->Priority.Actions,
          _Thread_Priority_action_change,
          NULL
        );
        /*@ assert priority_contributors{Here}( original_aggregation ) ==
              priority_contributors{Pre}( original_aggregation ); */
        /*@ assert priority_aggregation_well_formed{Here}(
              original_aggregation ); */
        /*@ assert priority_aggregation_cached_minimum{Here}(
              original_aggregation ); */
        /*@ assert operations->priority_actions ==
              _Thread_queue_Do_nothing_priority_actions; */
        break;
    }

    priority_aggregation = next_aggregation;
  } while ( _Priority_Actions_is_valid( priority_aggregation ) );

  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_ADD ==>
        priority_contributors{Here}( original_aggregation ) ==
          priority_contributors_insert(
            priority_contributors{Pre}( original_aggregation ),
            \at( queue_context->Priority.Actions.actions->Action.node, Pre )
          ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_ADD ==>
        priority_aggregation_well_formed{Here}( original_aggregation ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_ADD ==>
        priority_aggregation_cached_minimum{Here}( original_aggregation ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_REMOVE ==>
        priority_contributors{Here}( original_aggregation ) ==
          priority_contributors_extract(
            priority_contributors{Pre}( original_aggregation ),
            \at( queue_context->Priority.Actions.actions->Action.node, Pre )
          ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_CHANGE ==>
        priority_contributors{Here}( original_aggregation ) ==
          priority_contributors{Pre}( original_aggregation ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_CHANGE ==>
        priority_aggregation_well_formed{Here}( original_aggregation ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_CHANGE ==>
        priority_aggregation_cached_minimum{Here}( original_aggregation ); */

  if ( !_Priority_Actions_is_empty( &queue_context->Priority.Actions ) ) {
    _Thread_queue_Context_add_priority_update( queue_context, the_thread );
    /*@ assert operations->priority_actions ==
          _Thread_queue_Do_nothing_priority_actions; */

    /*@ calls _Thread_queue_Do_nothing_priority_actions; */
    ( *operations->priority_actions )(
      queue,
      &queue_context->Priority.Actions
    );
  }
}

void _Thread_Priority_perform_actions(
  Thread_Control       *start_of_path,
  Thread_queue_Context *queue_context
)
{
  Thread_Control *the_thread;
  size_t          update_count;

  _Assert( start_of_path != NULL );

  /*
   * This function is tricky on SMP configurations.  Please note that we do not
   * use the thread queue path available via the thread queue context.  Instead
   * we directly use the thread wait information to traverse the thread queue
   * path.  Thus, we do not necessarily acquire all thread queue locks on our
   * own.  In case of a deadlock, we use locks acquired by other processors
   * along the path.
   */

  the_thread = start_of_path;
  update_count = _Thread_queue_Context_save_priority_updates( queue_context );

  while ( true ) {
    Thread_queue_Queue *queue;

    queue = the_thread->Wait.queue;

    _Thread_Priority_do_perform_actions(
      the_thread,
      queue,
      the_thread->Wait.operations,
      false,
      queue_context
    );

    if ( _Priority_Actions_is_empty( &queue_context->Priority.Actions ) ) {
      return;
    }

    _Assert( queue != NULL );
    the_thread = queue->owner;
    _Assert( the_thread != NULL );

    /*
     * In case the priority action list is non-empty, then the current thread
     * is enqueued on a thread queue.  There is no need to notify the scheduler
     * about a priority change, since it will pick up the new priority once it
     * is unblocked.  Restore the previous set of threads bound to update the
     * priority.
     */
    _Thread_queue_Context_restore_priority_updates(
      queue_context,
      update_count
    );
  }
}

/*@
  requires \valid_read( &the_thread->Scheduler.nodes );
  requires \valid( priority_action_node );
  requires \valid( queue_context );
  requires \valid( the_thread->Scheduler.nodes );
  requires priority_action_type == PRIORITY_ACTION_ADD ||
           priority_action_type == PRIORITY_ACTION_REMOVE ||
           priority_action_type == PRIORITY_ACTION_CHANGE;

  assigns the_thread->Scheduler.nodes->Wait.Priority,
          the_thread->Scheduler.nodes->Priority.value,
          queue_context->Priority;
  allocates \nothing;
  frees \nothing;

  ensures priority_aggregation_well_formed{Post}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  ensures priority_aggregation_cached_minimum{Post}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  ensures priority_action_node->priority ==
    \at( priority_action_node->priority, Pre );
  ensures the_thread->Scheduler.nodes->owner ==
    \at( the_thread->Scheduler.nodes->owner, Pre );
  ensures \forall Scheduler_Node *node;
    \valid_read( &node->owner ) ==>
      node->owner == \at( node->owner, Pre );
  ensures \forall Thread_Control *thread;
    \valid_read( &thread->Scheduler.nodes ) ==>
      thread->Scheduler.nodes == \at( thread->Scheduler.nodes, Pre );
  ensures queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) ||
          queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) + 1;
  ensures queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) + 1 ==>
          queue_context->Priority.update[
            \at( queue_context->Priority.update_count, Pre )
          ] == the_thread;
  ensures queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) + 1 ==>
          SCHEDULER_PRIORITY_PURIFY(
            the_thread->Scheduler.nodes->Priority.value ) ==
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority;
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 1 ==>
          SCHEDULER_PRIORITY_PURIFY(
            the_thread->Scheduler.nodes->Priority.value ) ==
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority;
  ensures the_thread->Scheduler.nodes->Priority.value !=
            \at( the_thread->Scheduler.nodes->Priority.value, Pre ) ==>
          thread_priority_update_pending{Post}( queue_context, the_thread );
  ensures queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) &&
          \at(
            SCHEDULER_PRIORITY_PURIFY(
              the_thread->Scheduler.nodes->Priority.value ) ==
            the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
            Pre
          ) ==>
          SCHEDULER_PRIORITY_PURIFY(
            the_thread->Scheduler.nodes->Priority.value ) ==
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority;

  behavior add:
    assumes priority_action_type == PRIORITY_ACTION_ADD;
    requires priority_aggregation_well_formed{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    requires priority_aggregation_cached_minimum{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    requires !priority_contributor_member{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority,
      priority_action_node );
    requires \exists Priority_Node *node;
      node \in priority_contributors{Pre}(
        &the_thread->Scheduler.nodes->Wait.Priority );
    ensures priority_contributors{Post}(
              &the_thread->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors_insert(
              priority_contributors{Pre}(
                &the_thread->Scheduler.nodes->Wait.Priority ),
              priority_action_node );

  behavior add_noop:
    assumes priority_action_type == PRIORITY_ACTION_ADD;
    assumes \valid_read( &the_thread->Wait.operations );
    assumes \valid( the_thread->Wait.operations );
    assumes the_thread->Wait.operations->priority_actions ==
      _Thread_queue_Do_nothing_priority_actions;
    assumes queue_context->Priority.update_count <= 1;
    assumes priority_aggregation_well_formed{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    assumes priority_aggregation_cached_minimum{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    assumes !priority_contributor_member{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority,
      priority_action_node );
    assumes \exists Priority_Node *node;
      node \in priority_contributors{Pre}(
        &the_thread->Scheduler.nodes->Wait.Priority );
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
      priority_action_node + (..),
      _Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority ) + (..)
    );
    assumes \separated(
      &queue_context->Priority.Actions.actions,
      &queue_context->Priority.update_count,
      queue_context->Priority.update + (0 .. 1),
      &priority_action_node->priority,
      &the_thread->Scheduler.nodes->Wait.Priority.Contributors,
      &the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
      &_Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
    );
    assumes \separated(
      the_thread->Wait.operations + (..),
      queue_context + (..),
      the_thread->Scheduler.nodes + (..),
      priority_action_node + (..)
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
              priority_action_node );
    ensures priority_aggregation_well_formed{Post}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    ensures priority_aggregation_cached_minimum{Post}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    ensures queue_context->Priority.Actions.actions == \null;

  behavior remove:
    assumes priority_action_type == PRIORITY_ACTION_REMOVE;
    requires priority_aggregation_well_formed{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    requires priority_aggregation_cached_minimum{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    requires priority_contributor_member{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority,
      priority_action_node );
    requires \exists Priority_Node *other;
      other != priority_action_node &&
      other \in priority_contributors{Pre}(
        &the_thread->Scheduler.nodes->Wait.Priority );
    ensures priority_contributors{Post}(
              &the_thread->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors_extract(
              priority_contributors{Pre}(
                &the_thread->Scheduler.nodes->Wait.Priority ),
              priority_action_node );

  behavior remove_noop:
    assumes priority_action_type == PRIORITY_ACTION_REMOVE;
    assumes \valid_read( &the_thread->Wait.operations );
    assumes \valid( the_thread->Wait.operations );
    assumes the_thread->Wait.operations->priority_actions ==
      _Thread_queue_Do_nothing_priority_actions;
    assumes queue_context->Priority.update_count <= 1;
    assumes priority_aggregation_well_formed{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    assumes priority_aggregation_cached_minimum{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    assumes priority_contributor_member{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority,
      priority_action_node );
    assumes \exists Priority_Node *other;
      other != priority_action_node &&
      other \in priority_contributors{Pre}(
        &the_thread->Scheduler.nodes->Wait.Priority );
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
      priority_action_node + (..),
      _Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority ) + (..)
    );
    assumes \separated(
      &queue_context->Priority.Actions.actions,
      &queue_context->Priority.update_count,
      queue_context->Priority.update + (0 .. 1),
      &priority_action_node->priority,
      &the_thread->Scheduler.nodes->Wait.Priority.Contributors,
      &the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
      &_Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
    );
    assumes \separated(
      the_thread->Wait.operations + (..),
      queue_context + (..),
      the_thread->Scheduler.nodes + (..),
      priority_action_node + (..)
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
              priority_action_node );
    ensures queue_context->Priority.Actions.actions == \null;

  behavior change:
    assumes priority_action_type == PRIORITY_ACTION_CHANGE;
    requires priority_aggregation_well_formed{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    requires priority_contributor_member{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority,
      priority_action_node );
    ensures priority_contributors{Post}(
              &the_thread->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors{Pre}(
              &the_thread->Scheduler.nodes->Wait.Priority );

  behavior change_noop:
    assumes priority_action_type == PRIORITY_ACTION_CHANGE;
    assumes \valid_read( &the_thread->Wait.operations );
    assumes \valid( the_thread->Wait.operations );
    assumes the_thread->Wait.operations->priority_actions ==
      _Thread_queue_Do_nothing_priority_actions;
    assumes queue_context->Priority.update_count <= 1;
    assumes priority_aggregation_well_formed{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    assumes priority_contributor_member{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority,
      priority_action_node );
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
      priority_action_node + (..),
      _Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority ) + (..)
    );
    assumes \separated(
      &queue_context->Priority.Actions.actions,
      &queue_context->Priority.update_count,
      queue_context->Priority.update + (0 .. 1),
      &priority_action_node->priority,
      &the_thread->Scheduler.nodes->Wait.Priority.Contributors,
      &the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
      &_Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
    );
    assumes \separated(
      the_thread->Wait.operations + (..),
      queue_context + (..),
      the_thread->Scheduler.nodes + (..),
      priority_action_node + (..)
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
    ensures priority_aggregation_well_formed{Post}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    ensures priority_aggregation_cached_minimum{Post}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    ensures queue_context->Priority.Actions.actions == \null;

  complete behaviors add, remove, change;
  disjoint behaviors add, remove, change;
*/
static void _Thread_Priority_apply(
  Thread_Control       *the_thread,
  Priority_Node        *priority_action_node,
  Thread_queue_Context *queue_context,
  bool                  prepend_it,
  Priority_Action_type  priority_action_type
)
{
  Scheduler_Node     *scheduler_node;
  Thread_queue_Queue *queue;

  scheduler_node = _Thread_Scheduler_get_home_node( the_thread );
  /*@ assert scheduler_node == the_thread->Scheduler.nodes; */
  _Priority_Actions_initialize_one(
    &queue_context->Priority.Actions,
    &scheduler_node->Wait.Priority,
    priority_action_node,
    priority_action_type
  );
  /*@ assert queue_context->Priority.Actions.actions ==
        &scheduler_node->Wait.Priority; */
  /*@ assert queue_context->Priority.Actions.actions ==
        &the_thread->Scheduler.nodes->Wait.Priority; */
  /*@ assert priority_contributors{Here}( &scheduler_node->Wait.Priority ) ==
        priority_contributors{Pre}( &scheduler_node->Wait.Priority ); */
  queue = the_thread->Wait.queue;

#ifdef __FRAMAC__
Before_Do_Perform:
#endif
  _Thread_Priority_do_perform_actions(
    the_thread,
    queue,
    the_thread->Wait.operations,
    prepend_it,
    queue_context
  );
  /*@ assert priority_action_type == PRIORITY_ACTION_ADD ==>
        priority_contributors{Here}(
          \at( queue_context->Priority.Actions.actions, Before_Do_Perform )
        ) == priority_contributors_insert(
          priority_contributors{Before_Do_Perform}(
            \at( queue_context->Priority.Actions.actions, Before_Do_Perform )
          ),
          priority_action_node
        ); */
  /*@ assert priority_action_type == PRIORITY_ACTION_ADD ==>
        \at( queue_context->Priority.Actions.actions, Before_Do_Perform ) ==
          &scheduler_node->Wait.Priority; */
  /*@ assert priority_action_type == PRIORITY_ACTION_ADD ==>
        priority_contributors{Before_Do_Perform}(
          \at( queue_context->Priority.Actions.actions, Before_Do_Perform )
        ) == priority_contributors{Pre}(
          \at( queue_context->Priority.Actions.actions, Before_Do_Perform )
        ); */
  /*@ assert priority_action_type == PRIORITY_ACTION_REMOVE ==>
        priority_contributors{Here}(
          \at( queue_context->Priority.Actions.actions, Before_Do_Perform )
        ) == priority_contributors_extract(
          priority_contributors{Before_Do_Perform}(
            \at( queue_context->Priority.Actions.actions, Before_Do_Perform )
          ),
          priority_action_node
        ); */
  /*@ assert priority_action_type == PRIORITY_ACTION_REMOVE ==>
        \at( queue_context->Priority.Actions.actions, Before_Do_Perform ) ==
          &scheduler_node->Wait.Priority; */
  /*@ assert priority_action_type == PRIORITY_ACTION_REMOVE ==>
        priority_contributors{Before_Do_Perform}(
          \at( queue_context->Priority.Actions.actions, Before_Do_Perform )
        ) == priority_contributors{Pre}(
          \at( queue_context->Priority.Actions.actions, Before_Do_Perform )
        ); */
  /*@ assert priority_action_type == PRIORITY_ACTION_CHANGE ==>
        priority_contributors{Here}(
          \at( queue_context->Priority.Actions.actions, Before_Do_Perform )
        ) == priority_contributors{Before_Do_Perform}(
          \at( queue_context->Priority.Actions.actions, Before_Do_Perform )
        ); */
  /*@ assert priority_action_type == PRIORITY_ACTION_CHANGE ==>
        \at( queue_context->Priority.Actions.actions, Before_Do_Perform ) ==
          &scheduler_node->Wait.Priority; */
  /*@ assert scheduler_node == the_thread->Scheduler.nodes; */
  /*@ assert priority_action_type == PRIORITY_ACTION_ADD ==>
        priority_contributors{Here}(
          &the_thread->Scheduler.nodes->Wait.Priority
        ) == priority_contributors_insert(
          priority_contributors{Pre}(
            &the_thread->Scheduler.nodes->Wait.Priority
          ),
          priority_action_node
        ); */
  /*@ assert priority_action_type == PRIORITY_ACTION_ADD ==>
        priority_aggregation_well_formed{Here}(
          &the_thread->Scheduler.nodes->Wait.Priority
        ); */
  /*@ assert priority_action_type == PRIORITY_ACTION_ADD ==>
        priority_aggregation_cached_minimum{Here}(
          &the_thread->Scheduler.nodes->Wait.Priority
        ); */
  /*@ assert priority_action_type == PRIORITY_ACTION_REMOVE ==>
        priority_contributors{Here}(
          &the_thread->Scheduler.nodes->Wait.Priority
        ) == priority_contributors_extract(
          priority_contributors{Pre}(
            &the_thread->Scheduler.nodes->Wait.Priority
          ),
          priority_action_node
        ); */
  /*@ assert priority_action_type == PRIORITY_ACTION_CHANGE ==>
        priority_contributors{Here}(
          &the_thread->Scheduler.nodes->Wait.Priority
        ) == priority_contributors{Pre}(
          &the_thread->Scheduler.nodes->Wait.Priority
        ); */
  /*@ assert priority_action_type == PRIORITY_ACTION_CHANGE ==>
        priority_aggregation_well_formed{Here}(
          &the_thread->Scheduler.nodes->Wait.Priority
        ); */
  /*@ assert priority_action_type == PRIORITY_ACTION_CHANGE ==>
        priority_aggregation_cached_minimum{Here}(
          &the_thread->Scheduler.nodes->Wait.Priority
        ); */

  if ( !_Priority_Actions_is_empty( &queue_context->Priority.Actions ) ) {
#if defined(RTEMS_SMP)
    _Thread_queue_Path_acquire_critical( queue, the_thread, queue_context );
#endif
    _Thread_Priority_perform_actions( queue->owner, queue_context );
#if defined(RTEMS_SMP)
    _Thread_queue_Path_release_critical( queue_context );
#endif
  }
}

void _Thread_Priority_add(
  Thread_Control       *the_thread,
  Priority_Node        *priority_node,
  Thread_queue_Context *queue_context
)
{
  _Thread_Priority_apply(
    the_thread,
    priority_node,
    queue_context,
    false,
    PRIORITY_ACTION_ADD
  );
}

void _Thread_Priority_remove(
  Thread_Control       *the_thread,
  Priority_Node        *priority_node,
  Thread_queue_Context *queue_context
)
{
  _Thread_Priority_apply(
    the_thread,
    priority_node,
    queue_context,
    true,
    PRIORITY_ACTION_REMOVE
  );
}

void _Thread_Priority_changed(
  Thread_Control       *the_thread,
  Priority_Node        *priority_node,
  bool                  prepend_it,
  Thread_queue_Context *queue_context
)
{
  _Thread_Priority_apply(
    the_thread,
    priority_node,
    queue_context,
    prepend_it,
    PRIORITY_ACTION_CHANGE
  );
}

void _Thread_Priority_replace(
  Thread_Control *the_thread,
  Priority_Node  *victim_node,
  Priority_Node  *replacement_node
)
{
  Scheduler_Node *scheduler_node;

  scheduler_node = _Thread_Scheduler_get_home_node( the_thread );
  _Priority_Replace(
    &scheduler_node->Wait.Priority,
    victim_node,
    replacement_node
  );
}

void _Thread_Priority_update( Thread_queue_Context *queue_context )
{
  size_t i;
  size_t n;

  n = queue_context->Priority.update_count;

  /*
   * Update the priority of all threads of the set.  Do not care to clear the
   * set, since the thread queue context will soon get destroyed anyway.
   */
  for ( i = 0; i < n ; ++i ) {
    Thread_Control   *the_thread;
    ISR_lock_Context  lock_context;

    the_thread = queue_context->Priority.update[ i ];
    _Thread_State_acquire( the_thread, &lock_context );
    _Scheduler_Update_priority( the_thread );
    _Thread_State_release( the_thread, &lock_context );
  }
}

#if defined(RTEMS_SMP)
void _Thread_Priority_and_sticky_update(
  Thread_Control *the_thread,
  int             sticky_level_change
)
{
  ISR_lock_Context lock_context;

  _Thread_State_acquire( the_thread, &lock_context );
  _Scheduler_Priority_and_sticky_update(
    the_thread,
    sticky_level_change
  );
  _Thread_State_release( the_thread, &lock_context );
}
#endif
