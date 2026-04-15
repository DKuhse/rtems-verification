/**
 * @file
 *
 * @ingroup RTEMSScoreSchedulerEDF
 *
 * @brief EDF Scheduler Implementation
 */

/*
 *  Copryight (c) 2011 Petr Benes.
 *  Copyright (C) 2011 On-Line Applications Research Corporation (OAR).
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.org/license/LICENSE.
 */

#ifndef _RTEMS_SCORE_SCHEDULEREDFIMPL_H
#define _RTEMS_SCORE_SCHEDULEREDFIMPL_H

#include <rtems/score/scheduleredf.h>
#include <rtems/score/schedulerimpl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup RTEMSScoreSchedulerEDF
 *
 * @{
 */

/**
 * This is just a most significant bit of Priority_Control type. It
 * distinguishes threads which are deadline driven (priority
 * represented by a lower number than @a SCHEDULER_EDF_PRIO_MSB) from those
 * ones who do not have any deadlines and thus are considered background
 * tasks.
 */
#define SCHEDULER_EDF_PRIO_MSB 0x8000000000000000

/**
 * @brief Gets the context of the scheduler.
 *
 * @param scheduler The scheduler instance.
 *
 * @return The scheduler context of @a scheduler.
 */

/*@ ghost Scheduler_EDF_Context *g_edf_sched_context; */
/*This typecast crashes when trying to verify in Frama-C 25
  requires \valid_read(scheduler);
  assigns \nothing;
  ensures \result == (Scheduler_EDF_Context *) scheduler->context;
  */
/*@
  requires \valid_read(scheduler);
  assigns \nothing;
  ensures \result == g_edf_sched_context;
*/
RTEMS_INLINE_ROUTINE Scheduler_EDF_Context *
  _Scheduler_EDF_Get_context( const Scheduler_Control *scheduler )
{
  return (Scheduler_EDF_Context *) _Scheduler_Get_context( scheduler );
}

/**
 * @brief Gets the scheduler EDF node of the thread.
 *
 * @param the_thread The thread to get the scheduler node of.
 *
 * @return The EDF scheduler node of @a the_thread.
 */
/*@
  requires \valid(the_thread);
  assigns \nothing;
  ensures \result == (Scheduler_EDF_Node *) the_thread->Scheduler.nodes; 
*/
RTEMS_INLINE_ROUTINE Scheduler_EDF_Node *_Scheduler_EDF_Thread_get_node(
  Thread_Control *the_thread
)
{
  return (Scheduler_EDF_Node *) _Thread_Scheduler_get_home_node( the_thread );
}

/**
 * @brief Returns the scheduler EDF node for the scheduler node.
 *
 * @param node The scheduler node of which the scheduler EDF node is returned.
 *
 * @return The corresponding scheduler EDF node.
 */
/*@
  requires \valid(node);
  assigns \nothing;
  ensures \result == (Scheduler_EDF_Node *) node;
*/
RTEMS_INLINE_ROUTINE Scheduler_EDF_Node * _Scheduler_EDF_Node_downcast(
  Scheduler_Node *node
)
{
  return (Scheduler_EDF_Node *) node;
}

/**
 * @brief Checks if @a left is less than the priority of the node @a right.
 *
 * @param left The priority on the left hand side of the comparison.
 * @param right The node of which the priority is compared to left.
 *
 * @retval true @a left is less than the priority of @a right.
 * @retval false @a left is greater or equal than the priority of @a right.
 */
RTEMS_INLINE_ROUTINE bool _Scheduler_EDF_Less(
  const void        *left,
  const RBTree_Node *right
)
{
  const Priority_Control   *the_left;
  const Scheduler_EDF_Node *the_right;
  Priority_Control          prio_left;
  Priority_Control          prio_right;

  the_left = left;
  the_right = RTEMS_CONTAINER_OF( right, Scheduler_EDF_Node, Node );

  prio_left = *the_left;
  prio_right = the_right->priority;

  return prio_left < prio_right;
}

/**
 * @brief Checks if @a left is less or equal than the priority of the node @a right.
 *
 * @param left The priority on the left hand side of the comparison.
 * @param right The node of which the priority is compared to left.
 *
 * @retval true @a left is less or equal than the priority of @a right.
 * @retval false @a left is greater than the priority of @a right.
 */
RTEMS_INLINE_ROUTINE bool _Scheduler_EDF_Priority_less_equal(
  const void        *left,
  const RBTree_Node *right
)
{
  const Priority_Control   *the_left;
  const Scheduler_EDF_Node *the_right;
  Priority_Control          prio_left;
  Priority_Control          prio_right;

  the_left = left;
  the_right = RTEMS_CONTAINER_OF( right, Scheduler_EDF_Node, Node );

  prio_left = *the_left;
  prio_right = the_right->priority;

  return prio_left <= prio_right;
}

/**
 * @brief Inserts the scheduler node with the given priority in the ready
 *      queue of the context.
 *
 * @param[in, out] context The context to insert the node in.
 * @param node The node to be inserted.
 * @param insert_priority The priority with which the node will be inserted.
 */
/*@
  assigns \nothing;
*/
RTEMS_INLINE_ROUTINE void _Scheduler_EDF_Enqueue(
  Scheduler_EDF_Context *context,
  Scheduler_EDF_Node    *node,
  Priority_Control       insert_priority
)
{
  _RBTree_Insert_inline(
    &context->Ready,
    &node->Node,
    &insert_priority,
    _Scheduler_EDF_Priority_less_equal
  );
}

/**
 * @brief Extracts the scheduler node from the ready queue of the context.
 *
 * @param[in, out] context The context to extract the node from.
 * @param[in, out] node The node to extract.
 */
/*@
  assigns \nothing;
 */
RTEMS_INLINE_ROUTINE void _Scheduler_EDF_Extract(
  Scheduler_EDF_Context *context,
  Scheduler_EDF_Node    *node
)
{
  _RBTree_Extract( &context->Ready, &node->Node );
}

/**
 * @brief Extracts the node from the context of the given scheduler.
 *
 * @param scheduler The scheduler instance.
 * @param the_thread The thread is not used in this method.
 * @param[in, out] node The node to be extracted.
 */
RTEMS_INLINE_ROUTINE void _Scheduler_EDF_Extract_body(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Scheduler_Node          *node
)
{
  Scheduler_EDF_Context *context;
  Scheduler_EDF_Node    *the_node;

  context = _Scheduler_EDF_Get_context( scheduler );
  the_node = _Scheduler_EDF_Node_downcast( node );

  _Scheduler_EDF_Extract( context, the_node );
}

/**
 * @brief Schedules the next ready thread as the heir.
 *
 * @param scheduler The scheduler instance to schedule the minimum of the context of.
 * @param the_thread This parameter is not used.
 * @param force_dispatch Indicates whether the current heir is blocked even if it is
 *      not set as preemptible.
 */
/*@ ghost Scheduler_EDF_Node *g_min_edf_node; */
/*@
  requires \valid_read(scheduler) && \valid(g_min_edf_node) && \valid_read( g_edf_sched_context);
  requires \valid(g_min_edf_node->Base.owner) && \valid(_Thread_Heir) && \valid(_Per_CPU_Get());
  requires \separated( 
      _Thread_Heir, 
      _Per_CPU_Get()
  );

  behavior new_h:
    assumes _Thread_Heir != g_min_edf_node->Base.owner && ( _Thread_Heir->is_preemptible || force_dispatch );
    assigns _Thread_Heir, _Thread_Dispatch_necessary;
    ensures _Thread_Heir == g_min_edf_node->Base.owner;
    //ensures _Thread_Dispatch_necessary == true;
  behavior no_new_h:
    assumes !(_Thread_Heir != g_min_edf_node->Base.owner && ( _Thread_Heir->is_preemptible || force_dispatch ));
    assigns \nothing;
  complete behaviors;
  disjoint behaviors; 
 */
RTEMS_INLINE_ROUTINE void _Scheduler_EDF_Schedule_body(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  bool                     force_dispatch
)
{
  Scheduler_EDF_Context *context;
  RBTree_Node           *first;
  Scheduler_EDF_Node    *node;

  (void) the_thread;

  context = _Scheduler_EDF_Get_context( scheduler ); //Typecast crashes Frama-C 25
  //@assert context == g_edf_sched_context;
  //first = _RBTree_Minimum( &context->Ready );
  //node = RTEMS_CONTAINER_OF( first, Scheduler_EDF_Node, Node );
  node = _Helper_RBTree_EDF_Minimum( &context->Ready ); //Macro + bypassed tree function
  //@ assert node == g_min_edf_node;
  _Scheduler_Update_heir( node->Base.owner, force_dispatch );
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif
/* end of include file */
