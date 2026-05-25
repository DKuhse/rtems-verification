/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSScoreThread
 *
 * @brief This source file contains the implementation of
 *   _Thread_Priority_add(), _Thread_Priority_and_sticky_update(),
 *   _Thread_Priority_changed(), _Thread_Priority_perform_actions(),
 *   _Thread_Priority_remove(), _Thread_Priority_replace(), and
 *   _Thread_Priority_update().
 */

/*
 *  COPYRIGHT (c) 1989-2014.
 *  On-Line Applications Research Corporation (OAR).
 *
 *  Copyright (C) 2013, 2016 embedded brains GmbH & Co. KG
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

#include <rtems/score/threadimpl.h>
#include <rtems/score/assert.h>
#include <rtems/score/schedulerimpl.h>

#ifdef __FRAMAC__
/*@
  axiomatic ThreadPriorityPurifyFacts {
    axiom thread_priority_purify_group_order:
      \forall Priority_Control priority, Priority_Group_order group_order;
        priority_is_pure( priority ) &&
        ( group_order == PRIORITY_GROUP_FIRST ||
          group_order == PRIORITY_GROUP_LAST ) ==>
        SCHEDULER_PRIORITY_PURIFY(
          priority | (Priority_Control) group_order ) == priority;
  }
*/

/*@
  lemma thread_priority_scheduler_node_purifies:
    \forall Scheduler_Node *node;
    \forall Priority_Aggregation *aggregation;
      \valid_read( node ) &&
      aggregation == &node->Wait.Priority &&
      _Priority_Verify_scheduler_node_of_aggregation( aggregation ) == node &&
      priority_purifies_to(
        _Priority_Verify_scheduler_node_of_aggregation(
          aggregation )->Priority.value,
        aggregation->Node.priority
      ) ==>
        priority_purifies_to(
          node->Priority.value,
          node->Wait.Priority.Node.priority
        );
*/

/*@
  requires \valid( priority_actions );
  terminates \true;
  exits \false;
  assigns priority_actions->actions \from \nothing;
  ensures priority_actions->actions == \null;
  ensures \forall Priority_Aggregation *aggregation;
    \valid_read( aggregation ) &&
    \separated( &priority_actions->actions, aggregation + (..) ) ==>
      priority_contributors{Post}( aggregation ) ==
        priority_contributors{Pre}( aggregation );
  ensures \forall Priority_Aggregation *aggregation;
    \valid_read( aggregation ) &&
    \separated(
      &priority_actions->actions,
      &aggregation->Contributors,
      &aggregation->Node.priority
    ) &&
    ( \forall Priority_Node *node;
        node \in priority_contributors{Pre}( aggregation ) ==>
          \separated( &priority_actions->actions, node + (..) ) ) ==>
      priority_contributors{Post}( aggregation ) ==
        priority_contributors{Pre}( aggregation );
  ensures \forall Priority_Aggregation *aggregation;
    \separated( &priority_actions->actions, aggregation + (..) ) &&
    priority_aggregation_well_formed{Pre}( aggregation ) ==>
      priority_aggregation_well_formed{Post}( aggregation );
  ensures \forall Priority_Aggregation *aggregation;
    \separated(
      &priority_actions->actions,
      &aggregation->Contributors,
      &aggregation->Node.priority
    ) &&
    ( \forall Priority_Node *node;
        node \in priority_contributors{Pre}( aggregation ) ==>
          \separated( &priority_actions->actions, node + (..) ) ) &&
    priority_aggregation_well_formed{Pre}( aggregation ) ==>
      priority_aggregation_well_formed{Post}( aggregation );
  ensures \forall Priority_Aggregation *aggregation;
    \separated( &priority_actions->actions, aggregation + (..) ) &&
    priority_aggregation_cached_minimum{Pre}( aggregation ) ==>
      priority_aggregation_cached_minimum{Post}( aggregation );
  ensures \forall Priority_Aggregation *aggregation;
    \separated(
      &priority_actions->actions,
      &aggregation->Contributors,
      &aggregation->Node.priority
    ) &&
    ( \forall Priority_Node *node;
        node \in priority_contributors{Pre}( aggregation ) ==>
          \separated( &priority_actions->actions, node + (..) ) ) &&
    priority_aggregation_cached_minimum{Pre}( aggregation ) ==>
      priority_aggregation_cached_minimum{Post}( aggregation );
  ensures \forall Scheduler_Node *node;
    \valid_read( node ) &&
    \separated( &priority_actions->actions, node + (..) ) ==>
      node->Priority.value == \at( node->Priority.value, Pre );
  ensures \forall Scheduler_Node *node;
    \valid_read( &node->Priority.value ) &&
    \separated( &priority_actions->actions, &node->Priority.value ) ==>
      node->Priority.value == \at( node->Priority.value, Pre );
  ensures \forall Priority_Aggregation *aggregation;
    \valid_read( &aggregation->Node.priority ) &&
    \separated( &priority_actions->actions, &aggregation->Node.priority ) ==>
      aggregation->Node.priority ==
        \at( aggregation->Node.priority, Pre );
  ensures \forall Thread_Control *thread;
    \valid_read( &thread->Scheduler.nodes ) &&
    \separated( &priority_actions->actions, &thread->Scheduler.nodes ) ==>
      thread->Scheduler.nodes == \at( thread->Scheduler.nodes, Pre );
  ensures \forall Scheduler_Node *node;
    \valid_read( &node->Priority.value ) &&
    \valid_read( &node->Wait.Priority.Node.priority ) &&
    \separated(
      &priority_actions->actions,
      &node->Priority.value,
      &node->Wait.Priority.Node.priority
    ) &&
    priority_purifies_to(
      \at( node->Priority.value, Pre ),
      \at( node->Wait.Priority.Node.priority, Pre )
    ) ==>
      priority_purifies_to(
        node->Priority.value,
        node->Wait.Priority.Node.priority
      );
  ensures \forall Scheduler_Node *node;
    \valid_read( &node->Priority.value ) &&
    \valid_read( &node->Wait.Priority.Node.priority ) &&
    \separated(
      &priority_actions->actions,
      &node->Priority.value,
      &node->Wait.Priority.Node.priority
    ) &&
    SCHEDULER_PRIORITY_PURIFY(
      \at( node->Priority.value, Pre )
    ) == \at( node->Wait.Priority.Node.priority, Pre ) ==>
      SCHEDULER_PRIORITY_PURIFY( node->Priority.value ) ==
        node->Wait.Priority.Node.priority;
*/
void _Thread_queue_Do_nothing_priority_actions(
  Thread_queue_Queue *queue,
  Priority_Actions   *priority_actions
)
{
  (void) queue;
  priority_actions->actions = NULL;
}
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
  requires priority_is_pure( priority_aggregation->Node.priority );
  requires priority_group_order == PRIORITY_GROUP_FIRST ||
           priority_group_order == PRIORITY_GROUP_LAST;

  assigns _Priority_Verify_scheduler_node_of_aggregation(
    priority_aggregation )->Priority.value;

  // set scheduler node priority to priority aggregation minimum
  ensures _Priority_Verify_scheduler_node_of_aggregation(
    priority_aggregation )->Priority.value ==
      ( priority_aggregation->Node.priority |
        (Priority_Control) priority_group_order );
  ensures priority_purifies_to(
    _Priority_Verify_scheduler_node_of_aggregation(
      priority_aggregation )->Priority.value,
    priority_aggregation->Node.priority
  );
  ensures SCHEDULER_PRIORITY_PURIFY(
    _Priority_Verify_scheduler_node_of_aggregation(
      priority_aggregation )->Priority.value ) ==
    priority_aggregation->Node.priority;

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
  Priority_Group_order  priority_group_order
)
{
  _Scheduler_Node_set_priority(
    SCHEDULER_NODE_OF_WAIT_PRIORITY_NODE( priority_aggregation ),
    _Priority_Get_priority( priority_aggregation ),
    priority_group_order
  );
  /*@ assert _Priority_Verify_scheduler_node_of_aggregation(
        priority_aggregation )->Priority.value ==
      ( priority_aggregation->Node.priority |
        (Priority_Control) priority_group_order ); */
  /*@ assert SCHEDULER_PRIORITY_PURIFY(
        _Priority_Verify_scheduler_node_of_aggregation(
          priority_aggregation )->Priority.value ) ==
        priority_aggregation->Node.priority; */
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
  _Thread_Set_scheduler_node_priority(
    priority_aggregation,
    PRIORITY_GROUP_LAST
  );
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
  _Thread_Set_scheduler_node_priority(
    priority_aggregation,
    PRIORITY_GROUP_FIRST
  );
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
  requires priority_is_pure( priority_aggregation->Node.priority );
  requires priority_group_order == PRIORITY_GROUP_FIRST ||
           priority_group_order == PRIORITY_GROUP_LAST;
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
        (Priority_Control) priority_group_order );
  ensures priority_purifies_to(
    _Priority_Verify_scheduler_node_of_aggregation(
      priority_aggregation )->Priority.value,
    priority_aggregation->Node.priority
  );
  ensures SCHEDULER_PRIORITY_PURIFY(
    _Priority_Verify_scheduler_node_of_aggregation(
      priority_aggregation )->Priority.value ) ==
    priority_aggregation->Node.priority;
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
  Priority_Group_order  priority_group_order,
  Priority_Actions     *priority_actions,
  void                 *arg
)
{
  (void) arg;
  _Thread_Set_scheduler_node_priority(
    priority_aggregation,
    priority_group_order
  );
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
  requires priority_group_order == PRIORITY_GROUP_FIRST ||
           priority_group_order == PRIORITY_GROUP_LAST;
  requires queue_context->Priority.update_count <= 1;
  requires queue_context->Priority.Actions.actions ==
    &the_thread->Scheduler.nodes->Wait.Priority;
  requires _Priority_Verify_scheduler_node_of_aggregation(
    queue_context->Priority.Actions.actions ) == the_thread->Scheduler.nodes;
  requires \valid( queue_context->Priority.Actions.actions );
  requires \valid( queue_context->Priority.Actions.actions->Action.node );
  requires queue_context->Priority.Actions.actions->Action.type ==
             PRIORITY_ACTION_ADD ||
           queue_context->Priority.Actions.actions->Action.type ==
             PRIORITY_ACTION_REMOVE ||
           queue_context->Priority.Actions.actions->Action.type ==
             PRIORITY_ACTION_CHANGE;
  requires priority_is_pure(
    queue_context->Priority.Actions.actions->Action.node->priority );
  requires \forall Priority_Node *contributor;
    contributor \in priority_contributors{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority ) ==>
      priority_is_pure( contributor->priority );
  requires \valid( _Priority_Verify_scheduler_node_of_aggregation(
    &the_thread->Scheduler.nodes->Wait.Priority ) );
  requires _Priority_Verify_scheduler_node_of_aggregation(
    &the_thread->Scheduler.nodes->Wait.Priority ) ==
      the_thread->Scheduler.nodes;
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
  requires \separated(
    &the_thread->Scheduler.nodes,
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
        &queue_context->Priority.update_count,
        queue_context->Priority.update + (0 .. 1),
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
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 0 ==>
          queue_context->Priority.update[ 0 ] ==
            \at( queue_context->Priority.update[ 0 ], Pre );
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 0 &&
          \at( priority_purifies_to(
            the_thread->Scheduler.nodes->Priority.value,
            the_thread->Scheduler.nodes->Wait.Priority.Node.priority
          ), Pre ) ==>
          priority_purifies_to(
            the_thread->Scheduler.nodes->Priority.value,
            the_thread->Scheduler.nodes->Wait.Priority.Node.priority
          );
  ensures the_thread->Scheduler.nodes ==
    \at( the_thread->Scheduler.nodes, Pre );
  ensures queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) + 1 ==>
          priority_purifies_to(
            the_thread->Scheduler.nodes->Priority.value,
            the_thread->Scheduler.nodes->Wait.Priority.Node.priority
          );
  ensures queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) + 1 ==>
          SCHEDULER_PRIORITY_PURIFY(
            the_thread->Scheduler.nodes->Priority.value ) ==
            the_thread->Scheduler.nodes->Wait.Priority.Node.priority;
  ensures the_thread->Scheduler.nodes->Priority.value !=
            \at( the_thread->Scheduler.nodes->Priority.value, Pre ) ==>
          queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) + 1;

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
    requires priority_is_pure(
      the_thread->Scheduler.nodes->Wait.Priority.Node.priority );
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
    requires priority_is_pure(
      the_thread->Scheduler.nodes->Wait.Priority.Node.priority );
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
    ensures priority_aggregation_well_formed{Post}(
      \at( queue_context->Priority.Actions.actions, Pre ) );
    ensures priority_aggregation_cached_minimum{Post}(
      \at( queue_context->Priority.Actions.actions, Pre ) );

  behavior change:
    assumes queue_context->Priority.Actions.actions->Action.type ==
      PRIORITY_ACTION_CHANGE;
    requires priority_aggregation_well_formed{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    requires priority_contributor_member{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority,
      queue_context->Priority.Actions.actions->Action.node );
    requires priority_is_pure(
      the_thread->Scheduler.nodes->Wait.Priority.Node.priority );
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
  Priority_Group_order           priority_group_order,
  Thread_queue_Context          *queue_context
)
{
  Priority_Aggregation *priority_aggregation;

  _Assert( !_Priority_Actions_is_empty( &queue_context->Priority.Actions ) );
  priority_aggregation = _Priority_Actions_move( &queue_context->Priority.Actions );
  /*@ assert priority_aggregation ==
        \at( queue_context->Priority.Actions.actions, Pre ); */
  /*@ assert priority_aggregation ==
        &the_thread->Scheduler.nodes->Wait.Priority; */
  /*@ assert _Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority ) ==
      the_thread->Scheduler.nodes; */
  /*@ assert priority_contributors{Here}( priority_aggregation ) ==
        priority_contributors{Pre}( priority_aggregation ); */
  /*@ assert operations->priority_actions ==
        _Thread_queue_Do_nothing_priority_actions; */

  do {
#if defined(RTEMS_SMP)
    Priority_Aggregation *next_aggregation;
#endif
    Priority_Node        *priority_action_node;
    Priority_Action_type  priority_action_type;

#if defined(RTEMS_SMP)
    next_aggregation = _Priority_Get_next_action( priority_aggregation );
#endif

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
        /*@ assert priority_contributors{Here}( priority_aggregation ) ==
              priority_contributors_insert(
                priority_contributors{Pre}( priority_aggregation ),
                priority_action_node
              ); */
        /*@ assert priority_aggregation_well_formed{Here}(
              priority_aggregation ); */
        /*@ assert priority_aggregation_cached_minimum{Here}(
              priority_aggregation ); */
        /*@ assert operations->priority_actions ==
              _Thread_queue_Do_nothing_priority_actions; */
        /*@ assert _Priority_Verify_scheduler_node_of_aggregation(
              priority_aggregation ) == the_thread->Scheduler.nodes; */
        /*@ assert queue_context->Priority.Actions.actions == priority_aggregation ==>
            priority_purifies_to(
              _Priority_Verify_scheduler_node_of_aggregation(
                priority_aggregation )->Priority.value,
              priority_aggregation->Node.priority
            ); */
        /*@ assert queue_context->Priority.Actions.actions == priority_aggregation ==>
              priority_purifies_to(
                the_thread->Scheduler.nodes->Priority.value,
                the_thread->Scheduler.nodes->Wait.Priority.Node.priority
              ); */
        /*@ assert queue_context->Priority.Actions.actions == priority_aggregation ==>
              SCHEDULER_PRIORITY_PURIFY(
                the_thread->Scheduler.nodes->Priority.value ) ==
                the_thread->Scheduler.nodes->Wait.Priority.Node.priority; */
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
        /*@ assert priority_contributors{Here}( priority_aggregation ) ==
              priority_contributors{Pre}( priority_aggregation ); */
        /*@ assert \exists Priority_Node *other;
              other != priority_action_node &&
              other \in priority_contributors{Here}( priority_aggregation ); */
        /*@ assert \exists Priority_Node *remaining;
              remaining \in priority_contributors_extract(
                priority_contributors{Here}( priority_aggregation ),
                priority_action_node
              ); */
        _Priority_Extract_non_empty(
          priority_aggregation,
          priority_action_node,
          &queue_context->Priority.Actions,
          _Thread_Priority_action_change,
          NULL
        );
#endif
        /*@ assert priority_contributors{Here}( priority_aggregation ) ==
              priority_contributors_extract(
                priority_contributors{Pre}( priority_aggregation ),
                priority_action_node
              ); */
        /*@ assert priority_aggregation_well_formed{Here}(
              priority_aggregation ); */
        /*@ assert priority_aggregation_cached_minimum{Here}(
              priority_aggregation ); */
        /*@ assert operations->priority_actions ==
              _Thread_queue_Do_nothing_priority_actions; */
        /*@ assert _Priority_Verify_scheduler_node_of_aggregation(
              priority_aggregation ) == the_thread->Scheduler.nodes; */
        /*@ assert queue_context->Priority.Actions.actions == priority_aggregation ==>
            priority_purifies_to(
              _Priority_Verify_scheduler_node_of_aggregation(
                priority_aggregation )->Priority.value,
              priority_aggregation->Node.priority
            ); */
        /*@ assert queue_context->Priority.Actions.actions == priority_aggregation ==>
              priority_purifies_to(
                the_thread->Scheduler.nodes->Priority.value,
                the_thread->Scheduler.nodes->Wait.Priority.Node.priority
              ); */
        /*@ assert queue_context->Priority.Actions.actions == priority_aggregation ==>
              SCHEDULER_PRIORITY_PURIFY(
                the_thread->Scheduler.nodes->Priority.value ) ==
                the_thread->Scheduler.nodes->Wait.Priority.Node.priority; */
        break;
      default:
        _Assert( priority_action_type == PRIORITY_ACTION_CHANGE );
        _Priority_Changed(
          priority_aggregation,
          priority_action_node,
          priority_group_order,
          &queue_context->Priority.Actions,
          _Thread_Priority_action_change,
          NULL
        );
        /*@ assert priority_contributors{Here}( priority_aggregation ) ==
              priority_contributors{Pre}( priority_aggregation ); */
        /*@ assert priority_aggregation_well_formed{Here}(
              priority_aggregation ); */
        /*@ assert priority_aggregation_cached_minimum{Here}(
              priority_aggregation ); */
        /*@ assert operations->priority_actions ==
              _Thread_queue_Do_nothing_priority_actions; */
        /*@ assert _Priority_Verify_scheduler_node_of_aggregation(
              priority_aggregation ) == the_thread->Scheduler.nodes; */
        /*@ assert queue_context->Priority.Actions.actions == priority_aggregation ==>
            priority_purifies_to(
              _Priority_Verify_scheduler_node_of_aggregation(
                priority_aggregation )->Priority.value,
              priority_aggregation->Node.priority
            ); */
        /*@ assert queue_context->Priority.Actions.actions == priority_aggregation ==>
              priority_purifies_to(
                the_thread->Scheduler.nodes->Priority.value,
                the_thread->Scheduler.nodes->Wait.Priority.Node.priority
              ); */
        /*@ assert queue_context->Priority.Actions.actions == priority_aggregation ==>
              SCHEDULER_PRIORITY_PURIFY(
                the_thread->Scheduler.nodes->Priority.value ) ==
                the_thread->Scheduler.nodes->Wait.Priority.Node.priority; */
        break;
    }

#if defined(RTEMS_SMP)
    priority_aggregation = next_aggregation;
  } while ( priority_aggregation != NULL );
#else
  } while ( false );
#endif

  /*@ assert priority_aggregation ==
        \at( queue_context->Priority.Actions.actions, Pre ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_ADD ==>
        priority_contributors{Here}( priority_aggregation ) ==
          priority_contributors_insert(
            priority_contributors{Pre}( priority_aggregation ),
            \at( queue_context->Priority.Actions.actions->Action.node, Pre )
          ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_ADD ==>
        priority_aggregation_well_formed{Here}( priority_aggregation ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_ADD ==>
        priority_aggregation_cached_minimum{Here}( priority_aggregation ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_REMOVE ==>
        priority_contributors{Here}( priority_aggregation ) ==
          priority_contributors_extract(
            priority_contributors{Pre}( priority_aggregation ),
            \at( queue_context->Priority.Actions.actions->Action.node, Pre )
          ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_REMOVE ==>
        priority_contributors{Here}(
          \at( queue_context->Priority.Actions.actions, Pre )
        ) == priority_contributors_extract(
          priority_contributors{Pre}(
            \at( queue_context->Priority.Actions.actions, Pre )
          ),
          \at( queue_context->Priority.Actions.actions->Action.node, Pre )
        ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_REMOVE ==>
        priority_aggregation_well_formed{Here}( priority_aggregation ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_REMOVE ==>
        priority_aggregation_cached_minimum{Here}( priority_aggregation ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_REMOVE ==>
        priority_aggregation_cached_minimum{Here}(
          \at( queue_context->Priority.Actions.actions, Pre ) ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_CHANGE ==>
        priority_contributors{Here}( priority_aggregation ) ==
          priority_contributors{Pre}( priority_aggregation ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_CHANGE ==>
        priority_aggregation_well_formed{Here}( priority_aggregation ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_CHANGE ==>
        priority_aggregation_cached_minimum{Here}( priority_aggregation ); */
  /*@ assert queue_context->Priority.Actions.actions == priority_aggregation ==>
        priority_purifies_to(
          the_thread->Scheduler.nodes->Priority.value,
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority
        ); */
  /*@ assert queue_context->Priority.Actions.actions == priority_aggregation ==>
        SCHEDULER_PRIORITY_PURIFY(
          the_thread->Scheduler.nodes->Priority.value ) ==
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority; */

  if ( !_Priority_Actions_is_empty( &queue_context->Priority.Actions ) ) {
    /*@ assert queue_context->Priority.Actions.actions == priority_aggregation; */
    /*@ assert queue_context->Priority.Actions.actions == priority_aggregation ==>
          priority_purifies_to(
            the_thread->Scheduler.nodes->Priority.value,
            the_thread->Scheduler.nodes->Wait.Priority.Node.priority
          ); */
    /*@ assert priority_purifies_to(
          the_thread->Scheduler.nodes->Priority.value,
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority
        ); */
    /*@ assert SCHEDULER_PRIORITY_PURIFY(
          the_thread->Scheduler.nodes->Priority.value ) ==
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority; */
#ifdef __FRAMAC__
Before_Add_Priority_Update:
#endif
    _Thread_queue_Context_add_priority_update( queue_context, the_thread );
    /*@ assert operations->priority_actions ==
          _Thread_queue_Do_nothing_priority_actions; */
    /*@ assert \separated(
          &queue_context->Priority.Actions.actions,
          &priority_aggregation->Contributors,
          &priority_aggregation->Node.priority
        ); */
    /*@ assert \separated(
          &queue_context->Priority.Actions.actions,
          &the_thread->Scheduler.nodes->Priority.value,
          &the_thread->Scheduler.nodes->Wait.Priority.Node.priority
        ); */
    /*@ assert \separated(
          &queue_context->Priority.Actions.actions,
          &the_thread->Scheduler.nodes
        ); */
    /*@ assert \forall Priority_Node *contributor;
          contributor \in priority_contributors{Here}(
            priority_aggregation ) ==>
            \separated(
              &queue_context->Priority.Actions.actions,
              contributor + (..)
            ); */
    /*@ assert priority_contributors{Here}( priority_aggregation ) ==
          priority_contributors{Before_Add_Priority_Update}(
            priority_aggregation ); */
    /*@ assert priority_aggregation_well_formed{
          Before_Add_Priority_Update}( priority_aggregation ) ==>
        priority_aggregation_well_formed{Here}( priority_aggregation ); */
    /*@ assert priority_aggregation_cached_minimum{
          Before_Add_Priority_Update}( priority_aggregation ) ==>
        priority_aggregation_cached_minimum{Here}( priority_aggregation ); */
    /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
          == PRIORITY_ACTION_REMOVE ==>
          priority_aggregation_well_formed{Here}( priority_aggregation ); */
    /*@ assert SCHEDULER_PRIORITY_PURIFY(
          the_thread->Scheduler.nodes->Priority.value ) ==
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority; */

#ifdef __FRAMAC__
Before_Priority_Actions_Callback:
#endif
    /*@ calls _Thread_queue_Do_nothing_priority_actions; */
    ( *operations->priority_actions )(
      queue,
      &queue_context->Priority.Actions
    );
    /*@ assert priority_contributors{Here}( priority_aggregation ) ==
          priority_contributors{Before_Priority_Actions_Callback}(
            priority_aggregation ); */
    /*@ assert priority_aggregation_well_formed{
          Before_Priority_Actions_Callback}( priority_aggregation ) ==>
        priority_aggregation_well_formed{Here}( priority_aggregation ); */
    /*@ assert priority_aggregation_cached_minimum{
          Before_Priority_Actions_Callback}( priority_aggregation ) ==>
        priority_aggregation_cached_minimum{Here}( priority_aggregation ); */
    /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
          == PRIORITY_ACTION_REMOVE ==>
          priority_aggregation_cached_minimum{Here}( priority_aggregation ); */
    /*@ assert the_thread->Scheduler.nodes ==
          \at( the_thread->Scheduler.nodes, Before_Priority_Actions_Callback ); */
    /*@ assert priority_purifies_to(
          the_thread->Scheduler.nodes->Priority.value,
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority
        ); */
    /*@ assert SCHEDULER_PRIORITY_PURIFY(
          the_thread->Scheduler.nodes->Priority.value ) ==
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority; */
  }
  /*@ assert queue_context->Priority.update_count ==
          \at( queue_context->Priority.update_count, Pre ) + 1 ==>
        SCHEDULER_PRIORITY_PURIFY(
          the_thread->Scheduler.nodes->Priority.value ) ==
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority; */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_REMOVE ==>
        priority_aggregation_cached_minimum{Here}( priority_aggregation ); */
  /*@ assert \at( queue_context->Priority.Actions.actions->Action.type, Pre )
        == PRIORITY_ACTION_REMOVE ==>
        priority_aggregation_cached_minimum{Here}(
          \at( queue_context->Priority.Actions.actions, Pre ) ); */
  /*@ assert the_thread->Scheduler.nodes ==
        \at( the_thread->Scheduler.nodes, Pre ); */
  /*@ assert \at( queue_context->Priority.update_count, Pre ) == 0 &&
        queue_context->Priority.update_count == 0 ==>
        queue_context->Priority.update[ 0 ] ==
          \at( queue_context->Priority.update[ 0 ], Pre ); */
  /*@ assert \at( queue_context->Priority.update_count, Pre ) == 0 &&
        queue_context->Priority.update_count == 0 ==>
        queue_context->Priority.Actions.actions == \null; */
  /*@ assert \at( queue_context->Priority.update_count, Pre ) == 0 &&
        queue_context->Priority.update_count == 0 ==>
        the_thread->Scheduler.nodes->Priority.value ==
          \at( the_thread->Scheduler.nodes->Priority.value, Pre ); */
  /*@ assert \at( queue_context->Priority.update_count, Pre ) == 0 &&
        queue_context->Priority.update_count == 0 ==>
        the_thread->Scheduler.nodes->Wait.Priority.Node.priority ==
          \at( the_thread->Scheduler.nodes->Wait.Priority.Node.priority, Pre ); */
  /*@ assert \at( queue_context->Priority.update_count, Pre ) == 0 &&
        queue_context->Priority.update_count == 0 &&
        \at( priority_purifies_to(
          the_thread->Scheduler.nodes->Priority.value,
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority
        ), Pre ) ==>
        priority_purifies_to(
          the_thread->Scheduler.nodes->Priority.value,
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority
        ); */
  /*@ assert \valid( the_thread->Scheduler.nodes ); */
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
  update_count = _Thread_queue_Context_get_priority_updates( queue_context );

  while ( true ) {
    Thread_queue_Queue *queue;

    queue = the_thread->Wait.queue;

    _Thread_Priority_do_perform_actions(
      the_thread,
      queue,
      the_thread->Wait.operations,
      PRIORITY_GROUP_LAST,
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
  requires priority_group_order == PRIORITY_GROUP_FIRST ||
           priority_group_order == PRIORITY_GROUP_LAST;
  requires priority_action_type == PRIORITY_ACTION_ADD ||
           priority_action_type == PRIORITY_ACTION_REMOVE ||
           priority_action_type == PRIORITY_ACTION_CHANGE;
  requires priority_is_pure( priority_action_node->priority );
  requires priority_is_pure(
    the_thread->Scheduler.nodes->Wait.Priority.Node.priority );
  requires \forall Priority_Node *contributor;
    contributor \in priority_contributors{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority ) ==>
      priority_is_pure( contributor->priority );

  assigns the_thread->Scheduler.nodes->Wait.Priority,
          the_thread->Scheduler.nodes->Priority.value,
          queue_context->Priority;
  allocates \nothing;
  frees \nothing;

  ensures priority_aggregation_well_formed{Post}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  ensures priority_action_node->priority ==
    \at( priority_action_node->priority, Pre );
  ensures the_thread->Scheduler.nodes ==
    \at( the_thread->Scheduler.nodes, Pre );
  ensures the_thread->Scheduler.nodes->owner ==
    \at( the_thread->Scheduler.nodes->owner, Pre );
  ensures queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) ||
          queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) + 1;
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 0 ==>
          queue_context->Priority.update[ 0 ] ==
            \at( queue_context->Priority.update[ 0 ], Pre );
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 0 &&
          \at( priority_purifies_to(
            the_thread->Scheduler.nodes->Priority.value,
            the_thread->Scheduler.nodes->Wait.Priority.Node.priority
          ), Pre ) ==>
          priority_purifies_to(
            the_thread->Scheduler.nodes->Priority.value,
            the_thread->Scheduler.nodes->Wait.Priority.Node.priority
          );
  ensures queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) + 1 ==>
          queue_context->Priority.update[
            \at( queue_context->Priority.update_count, Pre )
          ] == the_thread;
  ensures queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) + 1 ==>
          priority_purifies_to(
            the_thread->Scheduler.nodes->Priority.value,
            the_thread->Scheduler.nodes->Wait.Priority.Node.priority
          );
  ensures queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) + 1 ==>
          SCHEDULER_PRIORITY_PURIFY(
            the_thread->Scheduler.nodes->Priority.value ) ==
            the_thread->Scheduler.nodes->Wait.Priority.Node.priority;
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 1 ==>
          priority_purifies_to(
            the_thread->Scheduler.nodes->Priority.value,
            the_thread->Scheduler.nodes->Wait.Priority.Node.priority
          );
  ensures \at( queue_context->Priority.update_count, Pre ) == 0 &&
          queue_context->Priority.update_count == 1 ==>
          SCHEDULER_PRIORITY_PURIFY(
            the_thread->Scheduler.nodes->Priority.value ) ==
            the_thread->Scheduler.nodes->Wait.Priority.Node.priority;
  ensures the_thread->Scheduler.nodes->Priority.value !=
            \at( the_thread->Scheduler.nodes->Priority.value, Pre ) ==>
          queue_context->Priority.update_count ==
            \at( queue_context->Priority.update_count, Pre ) + 1;
  behavior add:
    assumes priority_action_type == PRIORITY_ACTION_ADD;
    requires \valid_read( &the_thread->Wait.operations );
    requires \valid( the_thread->Wait.operations );
    requires the_thread->Wait.operations->priority_actions ==
      _Thread_queue_Do_nothing_priority_actions;
    requires queue_context->Priority.update_count <= 1;
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
      priority_action_node + (..),
      _Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority ) + (..)
    );
    requires \separated(
      &queue_context->Priority.Actions.actions,
      &queue_context->Priority.update_count,
      queue_context->Priority.update + (0 .. 1),
      &priority_action_node->priority,
      &the_thread->Scheduler.nodes->Wait.Priority.Contributors,
      &the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
      &_Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
    );
    requires \separated(
      the_thread->Wait.operations + (..),
      queue_context + (..),
      the_thread->Scheduler.nodes + (..),
      priority_action_node + (..)
    );
    requires \separated(
      &the_thread->Scheduler.nodes,
      the_thread->Wait.operations + (..),
      queue_context + (..),
      the_thread->Scheduler.nodes + (..),
      priority_action_node + (..)
    );
    requires \forall Priority_Node *contributor;
      contributor \in priority_contributors{Pre}(
        &the_thread->Scheduler.nodes->Wait.Priority ) ==>
        \separated(
          contributor + (..),
          &queue_context->Priority.Actions.actions,
          &queue_context->Priority.update_count,
          queue_context->Priority.update + (0 .. 1),
          &_Priority_Verify_scheduler_node_of_aggregation(
            &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
        );
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
    assumes \separated(
      &the_thread->Scheduler.nodes,
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
    requires \valid_read( &the_thread->Wait.operations );
    requires \valid( the_thread->Wait.operations );
    requires the_thread->Wait.operations->priority_actions ==
      _Thread_queue_Do_nothing_priority_actions;
    requires queue_context->Priority.update_count <= 1;
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
    requires \exists Priority_Node *remaining;
      remaining \in priority_contributors_extract(
        priority_contributors{Pre}(
          &the_thread->Scheduler.nodes->Wait.Priority ),
        priority_action_node
      );
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
      priority_action_node + (..),
      _Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority ) + (..)
    );
    requires \separated(
      &queue_context->Priority.Actions.actions,
      &queue_context->Priority.update_count,
      queue_context->Priority.update + (0 .. 1),
      &priority_action_node->priority,
      &the_thread->Scheduler.nodes->Wait.Priority.Contributors,
      &the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
      &_Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
    );
    requires \separated(
      the_thread->Wait.operations + (..),
      queue_context + (..),
      the_thread->Scheduler.nodes + (..),
      priority_action_node + (..)
    );
    requires \separated(
      &the_thread->Scheduler.nodes,
      the_thread->Wait.operations + (..),
      queue_context + (..),
      the_thread->Scheduler.nodes + (..),
      priority_action_node + (..)
    );
    requires \forall Priority_Node *contributor;
      contributor \in priority_contributors{Pre}(
        &the_thread->Scheduler.nodes->Wait.Priority ) ==>
        \separated(
          contributor + (..),
          &queue_context->Priority.Actions.actions,
          &queue_context->Priority.update_count,
          queue_context->Priority.update + (0 .. 1),
          &_Priority_Verify_scheduler_node_of_aggregation(
            &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
        );
    ensures priority_contributors{Post}(
              &the_thread->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors_extract(
              priority_contributors{Pre}(
                &the_thread->Scheduler.nodes->Wait.Priority ),
              priority_action_node );
    ensures priority_aggregation_well_formed{Post}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    ensures priority_aggregation_cached_minimum{Post}(
      &the_thread->Scheduler.nodes->Wait.Priority );

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
    assumes \separated(
      &the_thread->Scheduler.nodes,
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
    requires \valid_read( &the_thread->Wait.operations );
    requires \valid( the_thread->Wait.operations );
    requires the_thread->Wait.operations->priority_actions ==
      _Thread_queue_Do_nothing_priority_actions;
    requires queue_context->Priority.update_count <= 1;
    requires priority_aggregation_well_formed{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    requires priority_contributor_member{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority,
      priority_action_node );
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
      priority_action_node + (..),
      _Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority ) + (..)
    );
    requires \separated(
      &queue_context->Priority.Actions.actions,
      &queue_context->Priority.update_count,
      queue_context->Priority.update + (0 .. 1),
      &priority_action_node->priority,
      &the_thread->Scheduler.nodes->Wait.Priority.Contributors,
      &the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
      &_Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
    );
    requires \separated(
      the_thread->Wait.operations + (..),
      queue_context + (..),
      the_thread->Scheduler.nodes + (..),
      priority_action_node + (..)
    );
    requires \separated(
      &the_thread->Scheduler.nodes,
      the_thread->Wait.operations + (..),
      queue_context + (..),
      the_thread->Scheduler.nodes + (..),
      priority_action_node + (..)
    );
    requires \forall Priority_Node *contributor;
      contributor \in priority_contributors{Pre}(
        &the_thread->Scheduler.nodes->Wait.Priority ) ==>
        \separated(
          contributor + (..),
          &queue_context->Priority.Actions.actions,
          &queue_context->Priority.update_count,
          queue_context->Priority.update + (0 .. 1),
          &_Priority_Verify_scheduler_node_of_aggregation(
            &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
        );
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
    assumes \separated(
      &the_thread->Scheduler.nodes,
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
  Priority_Group_order  priority_group_order,
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
  /*@ assert _Priority_Verify_scheduler_node_of_aggregation(
        &scheduler_node->Wait.Priority ) == scheduler_node; */
  /*@ assert _Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority ) ==
      the_thread->Scheduler.nodes; */
  /*@ assert _Priority_Verify_scheduler_node_of_aggregation(
        queue_context->Priority.Actions.actions ) ==
      the_thread->Scheduler.nodes; */
  /*@ assert priority_contributors{Here}( &scheduler_node->Wait.Priority ) ==
        priority_contributors{Pre}( &scheduler_node->Wait.Priority ); */
  queue = the_thread->Wait.queue;

#if defined(RTEMS_SMP)
  if ( queue != NULL ) {
    /*
     * If the thread waits on a thread queue, then we have to acquire the path
     * before the priority action prepared above is carried out.  For example,
     * when a priority inheritance queue is involved, the priority action may
     * produce more actions using scheduler nodes of other threads.  Using
     * these nodes is only safe once the thread queue path is acquired.
     */
    (void) _Thread_queue_Path_acquire( queue, the_thread, queue_context );
  }
#endif

#ifdef __FRAMAC__
Before_Do_Perform:
#endif
  _Thread_Priority_do_perform_actions(
    the_thread,
    queue,
    the_thread->Wait.operations,
    priority_group_order,
    queue_context
  );
  /*@ assert \at( queue_context->Priority.update_count, Before_Do_Perform ) ==
        \at( queue_context->Priority.update_count, Pre ); */
  /*@ assert \at( priority_purifies_to(
        the_thread->Scheduler.nodes->Priority.value,
        the_thread->Scheduler.nodes->Wait.Priority.Node.priority
      ), Pre ) ==>
        \at( priority_purifies_to(
          the_thread->Scheduler.nodes->Priority.value,
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority
        ), Before_Do_Perform ); */
  /*@ assert \at( queue_context->Priority.update_count, Pre ) == 0 &&
        queue_context->Priority.update_count == 0 &&
        \at( priority_purifies_to(
          the_thread->Scheduler.nodes->Priority.value,
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority
        ), Pre ) ==>
        priority_purifies_to(
          the_thread->Scheduler.nodes->Priority.value,
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority
        ); */
  /*@ assert queue_context->Priority.update_count ==
          \at( queue_context->Priority.update_count, Pre ) + 1 ==>
        SCHEDULER_PRIORITY_PURIFY(
          the_thread->Scheduler.nodes->Priority.value ) ==
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority; */
  /*@ assert \at( queue_context->Priority.update_count, Pre ) == 0 &&
        queue_context->Priority.update_count == 1 ==>
        SCHEDULER_PRIORITY_PURIFY(
          the_thread->Scheduler.nodes->Priority.value ) ==
          the_thread->Scheduler.nodes->Wait.Priority.Node.priority; */
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
  /*@ assert priority_action_type == PRIORITY_ACTION_REMOVE ==>
        priority_aggregation_well_formed{Here}(
          &the_thread->Scheduler.nodes->Wait.Priority
        ); */
  /*@ assert priority_action_type == PRIORITY_ACTION_REMOVE ==>
        priority_aggregation_cached_minimum{Here}(
          &the_thread->Scheduler.nodes->Wait.Priority
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
    _Thread_Priority_perform_actions( queue->owner, queue_context );
  }

#if defined(RTEMS_SMP)
  if (queue != NULL ) {
    _Thread_queue_Path_release( queue_context );
  }
#endif
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
    PRIORITY_GROUP_LAST,
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
    PRIORITY_GROUP_FIRST,
    PRIORITY_ACTION_REMOVE
  );
}

void _Thread_Priority_changed(
  Thread_Control       *the_thread,
  Priority_Node        *priority_node,
  Priority_Group_order  priority_group_order,
  Thread_queue_Context *queue_context
)
{
  _Thread_Priority_apply(
    the_thread,
    priority_node,
    queue_context,
    priority_group_order,
    PRIORITY_ACTION_CHANGE
  );
}

#if defined(RTEMS_SMP)
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
#endif

void _Thread_Priority_update( Thread_queue_Context *queue_context )
{
  size_t i;
  size_t n;

  n = queue_context->Priority.update_count;

  /*
   * Update the priority of all threads of the set.  Do not care to clear the
   * set, since the thread queue context will soon get destroyed anyway.
   */
  if ( n == 0 ) {
    return;
  }

  /*@ assert n == 1; */
  /*@ assert queue_context->Priority.update_count == 1; */
  /*@ assert queue_context->Priority.update_count == 1 &&
        queue_context->Priority.update[ 0 ]->current_state == STATES_READY ==>
          priority_purifies_to(
            queue_context->Priority.update[ 0 ]->Scheduler.nodes->
              Priority.value,
            queue_context->Priority.update[ 0 ]->Scheduler.nodes->
              Wait.Priority.Node.priority ); */
  /*@ assert queue_context->Priority.update_count == 1 &&
        queue_context->Priority.update[ 0 ]->current_state == STATES_READY ==>
          SCHEDULER_PRIORITY_PURIFY(
            queue_context->Priority.update[ 0 ]->Scheduler.nodes->
              Priority.value ) ==
            queue_context->Priority.update[ 0 ]->Scheduler.nodes->
              Wait.Priority.Node.priority; */
  /*@
    loop invariant 0 <= i <= n;
    loop invariant n == 1;
    loop invariant n == queue_context->Priority.update_count;
    loop invariant n == \at( queue_context->Priority.update_count, Pre );
    loop invariant queue_context->Priority.update[ 0 ] ==
      \at( queue_context->Priority.update[ 0 ], Pre );
    loop invariant queue_context->Priority.update[ 0 ]->Scheduler.nodes ==
      \at( queue_context->Priority.update[ 0 ]->Scheduler.nodes, Pre );
    loop invariant \at(
      priority_aggregation_well_formed(
        &queue_context->Priority.update[ 0 ]->Scheduler.nodes->Wait.Priority ),
      Pre
    ) ==>
      priority_aggregation_well_formed{Here}(
        &queue_context->Priority.update[ 0 ]->Scheduler.nodes->Wait.Priority );
    loop invariant \at(
      priority_aggregation_cached_minimum(
        &queue_context->Priority.update[ 0 ]->Scheduler.nodes->Wait.Priority ),
      Pre
    ) ==>
      priority_aggregation_cached_minimum{Here}(
        &queue_context->Priority.update[ 0 ]->Scheduler.nodes->Wait.Priority );
    loop invariant priority_contributors{Here}(
      &queue_context->Priority.update[ 0 ]->Scheduler.nodes->Wait.Priority ) ==
      priority_contributors{Pre}(
        &queue_context->Priority.update[ 0 ]->Scheduler.nodes->Wait.Priority );
    loop invariant i == 0 ==> _Thread_Heir == \at( _Thread_Heir, Pre );
    loop invariant i == 0 ==>
      _Per_CPU_Information[ 0 ].per_cpu.heir ==
        \at( _Per_CPU_Information[ 0 ].per_cpu.heir, Pre );
    loop invariant \forall Priority_Node *node;
      \valid_read( node ) &&
      \separated(
        &node->priority,
        &((Scheduler_EDF_Node *)
          \at( queue_context->Priority.update[ 0 ]->Scheduler.nodes,
               Pre ))->priority
      ) ==>
        node->priority == \at( node->priority, Pre );
    loop invariant edf_ready_context_well_formed{Here}(
      (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
    loop invariant edf_ready_set{Here}(
              (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context ) ==
            edf_ready_set{Pre}(
              (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
    loop invariant edf_priority_cache_consistency_preserved{Pre,Here}(
      edf_ready_set{Pre}(
        (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context ) );
    loop invariant edf_scheduler_decision{Here}(
      (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
      _Per_CPU_Information[ 0 ].per_cpu.executing,
      _Thread_Heir,
      _Thread_Heir->is_preemptible,
      _Thread_Dispatch_necessary_ghost );
    loop invariant edf_preemptible_heir_is_earliest_ready{Here}(
      (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
      _Thread_Heir,
      _Thread_Heir->is_preemptible );
    loop invariant i == 0 ==>
      thread_priority_edf_heir_valid{Here}( _Thread_Heir );
    loop invariant i == 0 &&
      queue_context->Priority.update[ 0 ]->current_state == STATES_READY ==>
        edf_ready_member{Here}(
          (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
          (Scheduler_EDF_Node *)
            queue_context->Priority.update[ 0 ]->Scheduler.nodes );
    loop invariant i == 0 &&
      queue_context->Priority.update[ 0 ]->current_state == STATES_READY ==>
        priority_purifies_to(
          queue_context->Priority.update[ 0 ]->Scheduler.nodes->Priority.value,
          queue_context->Priority.update[ 0 ]->Scheduler.nodes->
            Wait.Priority.Node.priority );
    loop invariant i == 0 &&
      queue_context->Priority.update[ 0 ]->current_state == STATES_READY ==>
        SCHEDULER_PRIORITY_PURIFY(
          queue_context->Priority.update[ 0 ]->Scheduler.nodes->Priority.value
        ) ==
          queue_context->Priority.update[ 0 ]->Scheduler.nodes->
            Wait.Priority.Node.priority;
    loop invariant i == 0 ==>
      thread_priority_edf_update_separated{Here}(
        (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
        queue_context->Priority.update[ 0 ] );
    loop invariant i == 1 &&
      queue_context->Priority.update[ 0 ]->current_state == STATES_READY ==>
        edf_ready_node_cache_consistent{Here}(
          (Scheduler_EDF_Node *)
            queue_context->Priority.update[ 0 ]->Scheduler.nodes );
    loop assigns i,
      ((Scheduler_EDF_Node *)
        \at( queue_context->Priority.update[ 0 ]->Scheduler.nodes,
             Pre ))->priority,
      ((Scheduler_EDF_Node *)
        \at( queue_context->Priority.update[ 0 ]->Scheduler.nodes,
             Pre ))->Base.Priority,
      ((Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context)->Ready,
      _Per_CPU_Information[ 0 ].per_cpu.heir,
      _Per_CPU_Information[ 0 ].per_cpu.dispatch_necessary,
      _Thread_Dispatch_necessary_ghost,
      ((Thread_Control *) \at( _Thread_Heir, Pre ))->cpu_time_used,
      ((Thread_Control *)
        \at( _Per_CPU_Information[ 0 ].per_cpu.heir, Pre ))->cpu_time_used,
      _Per_CPU_Information[ 0 ].per_cpu.cpu_usage_timestamp;
    loop variant n - i;
  */
  for ( i = 0; i < n ; ++i ) {
    Thread_Control   *the_thread;
    ISR_lock_Context  lock_context;

    the_thread = queue_context->Priority.update[ i ];
    /*@ assert i == 0; */
    /*@ assert the_thread == queue_context->Priority.update[ 0 ]; */
    /*@ assert the_thread->Scheduler.nodes ==
          \at( queue_context->Priority.update[ 0 ]->Scheduler.nodes, Pre ); */
    _Thread_State_acquire( the_thread, &lock_context );
    /*@ assert _Thread_Heir == \at( _Thread_Heir, Pre ); */
    /*@ assert _Per_CPU_Information[ 0 ].per_cpu.heir ==
          \at( _Per_CPU_Information[ 0 ].per_cpu.heir, Pre ); */
    /*@ assert the_thread->Scheduler.nodes ==
          \at( queue_context->Priority.update[ 0 ]->Scheduler.nodes, Pre ); */
    /*@ assert &((Scheduler_EDF_Node *) the_thread->Scheduler.nodes)->priority ==
          &((Scheduler_EDF_Node *)
            \at( queue_context->Priority.update[ 0 ]->Scheduler.nodes,
                 Pre ))->priority; */
    /*@ assert &((Scheduler_EDF_Node *) the_thread->Scheduler.nodes)->
            Base.Priority ==
          &((Scheduler_EDF_Node *)
            \at( queue_context->Priority.update[ 0 ]->Scheduler.nodes,
                 Pre ))->Base.Priority; */
    /*@ assert &_Thread_Heir->cpu_time_used ==
          &((Thread_Control *) \at( _Thread_Heir, Pre ))->cpu_time_used; */
    /*@ assert &_Per_CPU_Information[ 0 ].per_cpu.heir->cpu_time_used ==
          &((Thread_Control *)
            \at( _Per_CPU_Information[ 0 ].per_cpu.heir, Pre ))->
              cpu_time_used; */
    /*@ assert thread_priority_edf_heir_valid{Here}( _Thread_Heir ); */
    /*@ assert edf_scheduler_decision{Here}(
          (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
          _Per_CPU_Information[ 0 ].per_cpu.executing,
          _Thread_Heir,
          _Thread_Heir->is_preemptible,
          _Thread_Dispatch_necessary_ghost ); */
    /*@ assert the_thread->current_state == STATES_READY ==>
          edf_ready_member{Here}(
            (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
            (Scheduler_EDF_Node *) the_thread->Scheduler.nodes ); */
    /*@ assert the_thread->current_state == STATES_READY ==>
          priority_purifies_to(
            the_thread->Scheduler.nodes->Priority.value,
            the_thread->Scheduler.nodes->Wait.Priority.Node.priority ); */
    /*@ assert the_thread->current_state == STATES_READY ==>
          SCHEDULER_PRIORITY_PURIFY(
            the_thread->Scheduler.nodes->Priority.value ) ==
            the_thread->Scheduler.nodes->Wait.Priority.Node.priority; */
    /*@ assert thread_priority_edf_update_separated{Here}(
          (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
          the_thread ); */
    _Scheduler_Update_priority( the_thread );
    /*@ assert queue_context->Priority.update[ 0 ]->Scheduler.nodes ==
          \at( queue_context->Priority.update[ 0 ]->Scheduler.nodes, Pre ); */
    /*@ assert &((Scheduler_EDF_Node *)
            queue_context->Priority.update[ 0 ]->Scheduler.nodes)->priority ==
          &((Scheduler_EDF_Node *)
            \at( queue_context->Priority.update[ 0 ]->Scheduler.nodes,
                 Pre ))->priority; */
    /*@ assert &((Scheduler_EDF_Node *)
            queue_context->Priority.update[ 0 ]->Scheduler.nodes)->
              Base.Priority ==
          &((Scheduler_EDF_Node *)
            \at( queue_context->Priority.update[ 0 ]->Scheduler.nodes,
                 Pre ))->Base.Priority; */
    /*@ assert edf_scheduler_decision{Here}(
          (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
          _Per_CPU_Information[ 0 ].per_cpu.executing,
          _Thread_Heir,
          _Thread_Heir->is_preemptible,
          _Thread_Dispatch_necessary_ghost ); */
    _Thread_State_release( the_thread, &lock_context );
  }
}

#if defined(RTEMS_SMP)
static void _Thread_Priority_update_helping(
  Thread_Control *the_thread,
  Chain_Node     *first_node
)
{
  const Chain_Node *tail;
  Chain_Node       *node;

  tail = _Chain_Immutable_tail( &the_thread->Scheduler.Scheduler_nodes );
  node = _Chain_Next( first_node );

  while ( node != tail ) {
    Scheduler_Node          *scheduler_node;
    const Scheduler_Control *scheduler;
    ISR_lock_Context         lock_context;

    scheduler_node = SCHEDULER_NODE_OF_THREAD_SCHEDULER_NODE( node );
    scheduler = _Scheduler_Node_get_scheduler( scheduler_node );

    _Scheduler_Acquire_critical( scheduler, &lock_context );
    ( *scheduler->Operations.update_priority )(
      scheduler,
      the_thread,
      scheduler_node
    );
    _Scheduler_Release_critical( scheduler, &lock_context );

    node = _Chain_Next( node );
  }
}

void _Thread_Priority_update_and_make_sticky( Thread_Control *the_thread )
{
  ISR_lock_Context         lock_context;
  ISR_lock_Context         lock_context_2;
  Chain_Node              *node;
  Scheduler_Node          *scheduler_node;
  const Scheduler_Control *scheduler;
  int                      new_sticky_level;
  int                      make_sticky_level;

  _Thread_State_acquire( the_thread, &lock_context );
  _Thread_Scheduler_process_requests( the_thread );

  node = _Chain_First( &the_thread->Scheduler.Scheduler_nodes );
  scheduler_node = SCHEDULER_NODE_OF_THREAD_SCHEDULER_NODE( node );
  scheduler = _Scheduler_Node_get_scheduler( scheduler_node );

  _Scheduler_Acquire_critical( scheduler, &lock_context_2 );

  new_sticky_level = scheduler_node->sticky_level + 1;
  scheduler_node->sticky_level = new_sticky_level;
  _Assert( new_sticky_level >= 1 );

  /*
   * The sticky level is incremented by the scheduler block operation, so for a
   * ready thread, the change to sticky happens at a level of two.
   */
  make_sticky_level = 1 + (int) _Thread_Is_ready( the_thread );

  if ( new_sticky_level == make_sticky_level ) {
    ( *scheduler->Operations.make_sticky )(
      scheduler,
      the_thread,
      scheduler_node
    );
  }

  ( *scheduler->Operations.update_priority )(
    scheduler,
    the_thread,
    scheduler_node
  );

  _Scheduler_Release_critical( scheduler, &lock_context_2 );
  _Thread_Priority_update_helping( the_thread, node );
  _Thread_State_release( the_thread, &lock_context );
}

void _Thread_Priority_update_and_clean_sticky( Thread_Control *the_thread )
{
  ISR_lock_Context         lock_context;
  ISR_lock_Context         lock_context_2;
  Chain_Node              *node;
  Scheduler_Node          *scheduler_node;
  const Scheduler_Control *scheduler;
  int                      new_sticky_level;
  int                      clean_sticky_level;

  _Thread_State_acquire( the_thread, &lock_context );
  _Thread_Scheduler_process_requests( the_thread );

  node = _Chain_First( &the_thread->Scheduler.Scheduler_nodes );
  scheduler_node = SCHEDULER_NODE_OF_THREAD_SCHEDULER_NODE( node );
  scheduler = _Scheduler_Node_get_scheduler( scheduler_node );

  _Scheduler_Acquire_critical( scheduler, &lock_context_2 );

  new_sticky_level = scheduler_node->sticky_level - 1;
  scheduler_node->sticky_level = new_sticky_level;
  _Assert( new_sticky_level >= 0 );

  /*
   * The sticky level is incremented by the scheduler block operation, so for a
   * ready thread, the change to sticky happens at a level of one.
   */
  clean_sticky_level = (int) _Thread_Is_ready( the_thread );

  if ( new_sticky_level == clean_sticky_level ) {
    ( *scheduler->Operations.clean_sticky )(
      scheduler,
      the_thread,
      scheduler_node
    );
  }

  ( *scheduler->Operations.update_priority )(
    scheduler,
    the_thread,
    scheduler_node
  );

  _Scheduler_Release_critical( scheduler, &lock_context_2 );
  _Thread_Priority_update_helping( the_thread, node );
  _Thread_State_release( the_thread, &lock_context );
}

void _Thread_Priority_update_ignore_sticky( Thread_Control *the_thread )
{
  ISR_lock_Context lock_context;

  _Thread_State_acquire( the_thread, &lock_context );
  _Thread_Scheduler_process_requests( the_thread );
  _Scheduler_Update_priority( the_thread );
  _Thread_State_release( the_thread, &lock_context );
}
#endif
