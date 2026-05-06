/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSScorePriority
 *
 * @brief This header file provides interfaces of the
 *   @ref RTEMSScorePriority which are only used by the implementation.
 */

/*
 * Copyright (c) 2016 embedded brains GmbH & Co. KG
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

#ifndef _RTEMS_SCORE_PRIORITYIMPL_H
#define _RTEMS_SCORE_PRIORITYIMPL_H

#include <rtems/score/priority.h>
#include <rtems/score/scheduler.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @addtogroup RTEMSScorePriority
 *
 * @{
 */

/*@ ghost
    extern bool g_new_minimum;
    extern Priority_Node *g_min_priority_node;
    extern Scheduler_Node *g_scheduler_node_of_wait_priority_node;
*/

/*@
  requires \valid_read(tree);
  assigns \result \from tree, g_min_priority_node;
  ensures \result == g_min_priority_node;
*/
Priority_Node *_Helper_RBTree_Minimum( const RBTree_Control *tree );

typedef enum {
  PRIORITY_GROUP_FIRST = 0,
  PRIORITY_GROUP_LAST = 1
} Priority_Group_order;

/*
 * Forward declaration of _Thread_Priority_action_change.
 * Defined in threadchangepriority.c. Must be visible here for the
 * //@ calls annotations on function pointer callbacks.
 */
static void _Thread_Priority_action_change(
  Priority_Aggregation *priority_aggregation,
  Priority_Group_order  priority_group_order,
  Priority_Actions     *priority_actions,
  void                 *arg
);

/*@
  requires \valid(actions);
  assigns actions->actions;
  ensures actions->actions == NULL;
*/
static inline void _Priority_Actions_initialize_empty(
  Priority_Actions *actions
)
{
  actions->actions = NULL;
}

/*@
  requires \valid(actions) && \valid(aggregation) && \valid(node);
  requires \separated(actions, aggregation);

  assigns aggregation->Action.node;
  assigns aggregation->Action.type;
  assigns actions->actions;

  ensures aggregation->Action.type == type;
  ensures aggregation->Action.node == node;
  ensures actions->actions == aggregation;
*/
static inline void _Priority_Actions_initialize_one(
  Priority_Actions     *actions,
  Priority_Aggregation *aggregation,
  Priority_Node        *node,
  Priority_Action_type  type
)
{
#if defined(RTEMS_SMP)
  aggregation->Action.next = NULL;
#endif
  aggregation->Action.node = node;
  aggregation->Action.type = type;

  actions->actions = aggregation;
}

/*@
  requires \valid_read(actions);
  assigns \nothing;
  ensures \result == (actions->actions == NULL);
 */
static inline bool _Priority_Actions_is_empty(
  const Priority_Actions *actions
)
{
  return actions->actions == NULL;
}

/*@
 requires \valid(actions);
 assigns actions->actions;
 ensures \result == \old(actions->actions);
 ensures actions->actions == NULL;
 */
static inline Priority_Aggregation *_Priority_Actions_move(
  Priority_Actions *actions
)
{
  Priority_Aggregation *aggregation;

  aggregation = actions->actions;
  actions->actions = NULL;

  return aggregation;
}

/*@
 requires \valid(actions) && \valid(aggregation);
 assigns actions->actions;
 ensures actions->actions == \old(aggregation);
 */
static inline void _Priority_Actions_add(
  Priority_Actions     *actions,
  Priority_Aggregation *aggregation
)
{
#if defined(RTEMS_SMP)
  _Assert( aggregation->Action.next == NULL );
  aggregation->Action.next = actions->actions;
#endif
  actions->actions = aggregation;
}

static inline void _Priority_Node_initialize(
  Priority_Node    *node,
  Priority_Control  priority
)
{
  node->priority = priority;
  _RBTree_Initialize_node( &node->Node.RBTree );
}

/*@
  requires \valid(node);
  assigns node->priority;
  ensures node->priority == priority;
 */
static inline void _Priority_Node_set_priority(
  Priority_Node    *node,
  Priority_Control  priority
)
{
  node->priority = priority;
}

/*@
  requires \valid(node);
  assigns RB_COLOR( &node->Node.RBTree, Node );
  ensures RB_COLOR( &node->Node.RBTree, Node ) == -1;
 */
static inline void _Priority_Node_set_inactive(
  Priority_Node *node
)
{
  _RBTree_Set_off_tree( &node->Node.RBTree );
}

/*@
  requires \valid(node);
  assigns \nothing;
  ensures \result == (RB_COLOR( &node->Node.RBTree, Node ) != -1);
 */
static inline bool _Priority_Node_is_active(
  const Priority_Node *node
)
{
  return !_RBTree_Is_node_off_tree( &node->Node.RBTree );
}

static inline void _Priority_Initialize_empty(
  Priority_Aggregation *aggregation
)
{
#if defined(RTEMS_DEBUG)
#if defined(RTEMS_SMP)
  aggregation->Action.next = NULL;
#endif
  aggregation->Action.node = NULL;
  aggregation->Action.type = PRIORITY_ACTION_INVALID;
#endif
  _RBTree_Initialize_node( &aggregation->Node.Node.RBTree );
  _RBTree_Initialize_empty( &aggregation->Contributors );
}

static inline void _Priority_Initialize_one(
  Priority_Aggregation *aggregation,
  Priority_Node        *node
)
{
#if defined(RTEMS_DEBUG)
#if defined(RTEMS_SMP)
  aggregation->Action.next = NULL;
#endif
  aggregation->Action.node = NULL;
  aggregation->Action.type = PRIORITY_ACTION_INVALID;
#endif
  _Priority_Node_initialize( &aggregation->Node, node->priority );
  _RBTree_Initialize_one( &aggregation->Contributors, &node->Node.RBTree );
}

static inline bool _Priority_Is_empty(
  const Priority_Aggregation *aggregation
)
{
  return _RBTree_Is_empty( &aggregation->Contributors );
}

/*@
  requires \valid(aggregation);
  assigns \nothing;
  ensures \result == aggregation->Node.priority;
*/
static inline Priority_Control _Priority_Get_priority(
  const Priority_Aggregation *aggregation
)
{
  return aggregation->Node.priority;
}

static inline const Scheduler_Control *_Priority_Get_scheduler(
  const Priority_Aggregation *aggregation
)
{
#if defined(RTEMS_SMP)
  return aggregation->scheduler;
#else
  return &_Scheduler_Table[ 0 ];
#endif
}

/*
 * VERIFICATION CHANGE: replaced _RBTree_Minimum with _Helper_RBTree_Minimum.
 * The original call traverses the red-black tree which Frama-C/WP cannot
 * reason about. The stub returns the ghost variable g_min_priority_node.
 */
/*@
  requires \valid(aggregation);
  assigns \nothing;
  ensures \result == g_min_priority_node;
*/
static inline Priority_Node *_Priority_Get_minimum_node(
  const Priority_Aggregation *aggregation
)
{
  return _Helper_RBTree_Minimum( &aggregation->Contributors );
}

/*@
  requires \valid(aggregation) && \valid(node);
  requires \separated(aggregation, node);
  assigns aggregation->Action.node;
  ensures aggregation->Action.node == node;
*/
static inline void _Priority_Set_action_node(
  Priority_Aggregation *aggregation,
  Priority_Node        *node
)
{
#if defined(RTEMS_SMP)
  _Assert( aggregation->Action.next == NULL );
#endif
  aggregation->Action.node = node;
}

/*@
  requires \valid(aggregation);
  assigns aggregation->Action.type;
  ensures aggregation->Action.type == type;
*/
static inline void _Priority_Set_action_type(
  Priority_Aggregation *aggregation,
  Priority_Action_type  type
)
{
#if defined(RTEMS_SMP)
  _Assert( aggregation->Action.next == NULL );
#endif
  aggregation->Action.type = type;
}

/*@
  requires \valid(aggregation) && \valid(node);
  requires \separated(aggregation, node);
  assigns aggregation->Action.node, aggregation->Action.type;
  ensures aggregation->Action.node == node;
  ensures aggregation->Action.type == type;
*/
static inline void _Priority_Set_action(
  Priority_Aggregation *aggregation,
  Priority_Node        *node,
  Priority_Action_type  type
)
{
#if defined(RTEMS_SMP)
  _Assert( aggregation->Action.next == NULL );
#endif
  aggregation->Action.node = node;
  aggregation->Action.type = type;
}

#if defined(RTEMS_SMP)
static inline Priority_Aggregation *_Priority_Get_next_action(
#if defined(RTEMS_DEBUG)
  Priority_Aggregation *aggregation
#else
  const Priority_Aggregation *aggregation
#endif
)
{
  Priority_Aggregation *next;

  next = aggregation->Action.next;
#if defined(RTEMS_DEBUG)
  aggregation->Action.next = NULL;
#endif

  return next;
}
#endif

static inline bool _Priority_Less(
  const void        *left,
  const RBTree_Node *right
)
{
  const Priority_Control *the_left;
  const Priority_Node    *the_right;

  the_left = (const Priority_Control *) left;
  the_right = RTEMS_CONTAINER_OF( right, Priority_Node, Node.RBTree );

  return *the_left < the_right->priority;
}

/*@
  requires \valid(aggregation) && \valid(node);
  assigns \nothing;
  ensures \result == g_new_minimum;
*/
static inline bool _Priority_Plain_insert(
  Priority_Aggregation *aggregation,
  Priority_Node        *node,
  Priority_Control      priority
)
{
  return _RBTree_Insert_inline(
    &aggregation->Contributors,
    &node->Node.RBTree,
    &priority,
    _Priority_Less
  );
}

/*@
  assigns \nothing;
*/
static inline void _Priority_Plain_extract(
  Priority_Aggregation *aggregation,
  Priority_Node        *node
)
{
  _RBTree_Extract( &aggregation->Contributors, &node->Node.RBTree );
}

/*@
assigns \nothing;
*/
static inline void _Priority_Plain_changed(
  Priority_Aggregation *aggregation,
  Priority_Node        *node
)
{
  _Priority_Plain_extract( aggregation, node );
  _Priority_Plain_insert( aggregation, node, node->priority );
}

typedef void ( *Priority_Add_handler )(
  Priority_Aggregation *aggregation,
  Priority_Actions     *actions,
  void                 *arg
);

typedef void ( *Priority_Change_handler )(
  Priority_Aggregation *aggregation,
  Priority_Group_order  group_order,
  Priority_Actions     *actions,
  void                 *arg
);

typedef void ( *Priority_Remove_handler )(
  Priority_Aggregation *aggregation,
  Priority_Actions     *actions,
  void                 *arg
);

/*@
  assigns \nothing;
*/
static inline void _Priority_Change_nothing(
  Priority_Aggregation *aggregation,
  Priority_Group_order  group_order,
  Priority_Actions     *actions,
  void                 *arg
)
{
  (void) aggregation;
  (void) group_order;
  (void) actions;
  (void) arg;
}

/*@
  assigns \nothing;
*/
static inline void _Priority_Remove_nothing(
  Priority_Aggregation *aggregation,
  Priority_Actions     *actions,
  void                 *arg
)
{
  (void) aggregation;
  (void) actions;
  (void) arg;
}

/*@
  requires \valid(aggregation);
  requires \valid(node);
  requires \valid(actions);
  requires \valid(g_scheduler_node_of_wait_priority_node);
  requires \valid(g_min_priority_node);
  requires \separated(
    &node->priority,
    &aggregation->Node.priority,
    &g_scheduler_node_of_wait_priority_node->Priority.value
  );
  requires actions->actions == NULL;
  requires change == _Thread_Priority_action_change;

  behavior new_min:
    assumes g_new_minimum;
    assigns actions->actions;
    assigns aggregation->Node.priority;
    assigns g_scheduler_node_of_wait_priority_node->Priority.value;
    ensures aggregation->Node.priority == \old(node->priority);
    ensures actions->actions == aggregation;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value ==
      (\old(node->priority) | 1);

 behavior no_new_min:
    assumes !g_new_minimum;
    assigns \nothing;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value ==
      \old(g_scheduler_node_of_wait_priority_node->Priority.value );
    ensures actions->actions == NULL;

  disjoint behaviors;
  complete behaviors;
*/
static inline void _Priority_Non_empty_insert(
  Priority_Aggregation    *aggregation,
  Priority_Node           *node,
  Priority_Actions        *actions,
  Priority_Change_handler  change,
  void                    *arg
)
{
  bool is_new_minimum;

  _Assert( !_Priority_Is_empty( aggregation ) );
  is_new_minimum = _Priority_Plain_insert( aggregation, node, node->priority );

  if ( is_new_minimum ) {
    aggregation->Node.priority = node->priority;
    //@ calls _Thread_Priority_action_change;
    ( *change )( aggregation, PRIORITY_GROUP_LAST, actions, arg );
  }
}

static inline void _Priority_Insert(
  Priority_Aggregation    *aggregation,
  Priority_Node           *node,
  Priority_Actions        *actions,
  Priority_Add_handler     add,
  Priority_Change_handler  change,
  void                    *arg
)
{
  if ( _Priority_Is_empty( aggregation ) ) {
    _Priority_Initialize_one( aggregation, node );
    ( *add )( aggregation, actions, arg );
  } else {
    _Priority_Non_empty_insert( aggregation, node, actions, change, arg );
  }
}

static inline void _Priority_Extract(
  Priority_Aggregation    *aggregation,
  Priority_Node           *node,
  Priority_Actions        *actions,
  Priority_Remove_handler  remove,
  Priority_Change_handler  change,
  void                    *arg
)
{
  _Priority_Plain_extract( aggregation, node );

  if ( _Priority_Is_empty( aggregation ) ) {
    ( *remove )( aggregation, actions, arg );
  } else {
    Priority_Node *min;

    /* The aggregation is non-empty, so the minimum node exists. */
    min = _Priority_Get_minimum_node( aggregation );
    _Assert( min != NULL );

    if ( node->priority < min->priority ) {
      aggregation->Node.priority = min->priority;
      ( *change )( aggregation, PRIORITY_GROUP_FIRST, actions, arg );
    }
  }
}

/*@
  requires \valid(aggregation) && \valid(node) && \valid(actions);
  requires \valid(g_scheduler_node_of_wait_priority_node);
  requires \separated(
    &aggregation->Node.priority,
    &g_scheduler_node_of_wait_priority_node->Priority.value
  );
  requires actions->actions == NULL;
  requires change == _Thread_Priority_action_change;

  behavior new_min:
    assumes node->priority < g_min_priority_node->priority;
    assigns actions->actions;
    assigns aggregation->Node.priority,
      g_scheduler_node_of_wait_priority_node->Priority.value;
    ensures aggregation->Node.priority == g_min_priority_node->priority;
    ensures actions->actions == aggregation;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value ==
      (aggregation->Node.priority | 0);

  behavior no_new_min:
    assumes !(node->priority < g_min_priority_node->priority);
    assigns \nothing;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value ==
      \old(g_scheduler_node_of_wait_priority_node->Priority.value );
    ensures actions->actions == NULL;

  disjoint behaviors;
  complete behaviors;
*/
static inline void _Priority_Extract_non_empty(
  Priority_Aggregation    *aggregation,
  Priority_Node           *node,
  Priority_Actions        *actions,
  Priority_Change_handler  change,
  void                    *arg
)
{
  Priority_Node *min;

  _Priority_Plain_extract( aggregation, node );
  _Assert( !_Priority_Is_empty( aggregation ) );

  min = _Priority_Get_minimum_node( aggregation );

  if ( node->priority < min->priority ) {
    aggregation->Node.priority = min->priority;
    //@ calls _Thread_Priority_action_change;
    ( *change )( aggregation, PRIORITY_GROUP_FIRST, actions, arg );
  }
}

/*@
  requires \valid(aggregation) && \valid(node) && \valid(actions);
  requires \valid(g_scheduler_node_of_wait_priority_node);
  requires actions->actions == NULL;
  requires \separated(
    &aggregation->Node.priority,
    &g_scheduler_node_of_wait_priority_node->Priority.value,
    &g_min_priority_node->priority
  );
  requires change == _Thread_Priority_action_change;

  behavior new_min_group_first:
    assumes aggregation->Node.priority != g_min_priority_node->priority;
    assumes group_order == PRIORITY_GROUP_FIRST;
    assigns actions->actions;
    assigns aggregation->Node.priority,
      g_scheduler_node_of_wait_priority_node->Priority.value;
    ensures aggregation->Node.priority == g_min_priority_node->priority;
    ensures actions->actions == aggregation;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value ==
      (aggregation->Node.priority | 0);

  behavior new_min_group_last:
    assumes aggregation->Node.priority != g_min_priority_node->priority;
    assumes group_order == PRIORITY_GROUP_LAST;
    assigns actions->actions;
    assigns aggregation->Node.priority,
      g_scheduler_node_of_wait_priority_node->Priority.value;
    ensures aggregation->Node.priority == g_min_priority_node->priority;
    ensures actions->actions == aggregation;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value ==
      (aggregation->Node.priority | 1);

  behavior no_new_min:
    assumes aggregation->Node.priority == g_min_priority_node->priority;
    assigns \nothing;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value ==
      \old(g_scheduler_node_of_wait_priority_node->Priority.value );
    ensures aggregation->Node.priority == g_min_priority_node->priority;
    ensures actions->actions == NULL;

  disjoint behaviors;
  complete behaviors;
*/
static inline void _Priority_Changed(
  Priority_Aggregation    *aggregation,
  Priority_Node           *node,
  Priority_Group_order     group_order,
  Priority_Actions        *actions,
  Priority_Change_handler  change,
  void                    *arg
)
{
  Priority_Node *min;

  _Priority_Plain_changed( aggregation, node );

  min = _Priority_Get_minimum_node( aggregation );
  _Assert( min != NULL );

  if ( min->priority != aggregation->Node.priority ) {
    aggregation->Node.priority = min->priority;
    //@ calls _Thread_Priority_action_change;
    ( *change )( aggregation, group_order, actions, arg );
  }
}

static inline void _Priority_Replace(
  Priority_Aggregation *aggregation,
  Priority_Node        *victim,
  Priority_Node        *replacement
)
{
  replacement->priority = victim->priority;
  _RBTree_Replace_node(
    &aggregation->Contributors,
    &victim->Node.RBTree,
    &replacement->Node.RBTree
  );
}

/** @} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _RTEMS_SCORE_PRIORITYIMPL_H */
