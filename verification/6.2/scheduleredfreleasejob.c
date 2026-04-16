/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSScoreSchedulerEDF
 *
 * @brief This source file contains the implementation of
 *   _Scheduler_EDF_Map_priority(), _Scheduler_EDF_Unmap_priority(),
 *   _Scheduler_EDF_Release_job(), and _Scheduler_EDF_Cancel_job().
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

#include <rtems/score/scheduleredfimpl.h>
/*@
  requires PRIORITY_MINIMUM < priority <= PRIORITY_DEFAULT_MAXIMUM;
  ensures \result == (SCHEDULER_EDF_PRIO_MSB | SCHEDULER_PRIORITY_MAP( priority ));
  ensures (\result & SCHEDULER_EDF_PRIO_MSB) == 0x8000000000000000;
*/
Priority_Control _Scheduler_EDF_Map_priority(
  const Scheduler_Control *scheduler,
  Priority_Control         priority
)
{
  return SCHEDULER_EDF_PRIO_MSB | SCHEDULER_PRIORITY_MAP( priority );
}

/*@
  ensures \result == SCHEDULER_PRIORITY_UNMAP( priority & ~SCHEDULER_EDF_PRIO_MSB );
  ensures (\result & SCHEDULER_EDF_PRIO_MSB) == 0x0000000000000000;
*/
Priority_Control _Scheduler_EDF_Unmap_priority(
  const Scheduler_Control *scheduler,
  Priority_Control         priority
)
{
  return SCHEDULER_PRIORITY_UNMAP( priority & ~SCHEDULER_EDF_PRIO_MSB );
}

/*@
  requires \valid(the_thread->Wait.queue);
  requires \valid(the_thread->Wait.operations);
  requires \valid(the_thread->Scheduler.nodes);
  requires \valid(the_thread);
  requires \valid(priority_node);
  requires \valid(queue_context);
  requires \valid(g_scheduler_node_of_wait_priority_node) && \valid(g_min_priority_node);
  requires \valid(queue_context->Priority.update[0]);
  requires queue_context->Priority.update_count == 0;
  requires the_thread->Wait.operations->priority_actions ==
    _Thread_queue_Do_nothing_priority_actions;

  requires deadline < 146235 && deadline > 0;

  requires \separated(
    queue_context,
    the_thread->Scheduler.nodes,
    the_thread->Wait.operations,
    the_thread->Wait.queue,
    the_thread
  );

  requires
    \separated(
      &the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
      &g_scheduler_node_of_wait_priority_node->Priority.value,
      &g_min_priority_node->priority,
      &priority_node->priority,
      &queue_context->Priority.update_count
    );

  behavior priority_node_already_active_new_min:
    assumes priority_node->Node.RBTree.Node.rbe_color != -1;
    assumes the_thread->Scheduler.nodes->Wait.Priority.Node.priority != g_min_priority_node->priority;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value == (g_min_priority_node->priority | 1);
    ensures \old(the_thread->Scheduler.nodes)->Wait.Priority.Node.priority == g_min_priority_node->priority;
    ensures queue_context->Priority.update[0] == the_thread;
    ensures queue_context->Priority.update_count == 1;

  behavior priority_node_already_active_no_new_min:
    assumes priority_node->Node.RBTree.Node.rbe_color != -1;
    assumes the_thread->Scheduler.nodes->Wait.Priority.Node.priority == g_min_priority_node->priority;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value == \old(g_scheduler_node_of_wait_priority_node->Priority.value);

  behavior priority_node_new_new_min:
    assumes priority_node->Node.RBTree.Node.rbe_color == -1;
    assumes g_new_minimum;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value == ( (deadline << 1) | 1 );
    ensures \old(the_thread->Scheduler.nodes)->Wait.Priority.Node.priority == (deadline << 1);
    ensures queue_context->Priority.update[0] == the_thread;
    ensures queue_context->Priority.update_count == 1;

  behavior priority_node_new_no_new_min:
    assumes priority_node->Node.RBTree.Node.rbe_color == -1;
    assumes !g_new_minimum;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value == \old(g_scheduler_node_of_wait_priority_node->Priority.value);

  disjoint behaviors;
  complete behaviors;
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

  _Priority_Node_set_priority(
    priority_node,
    SCHEDULER_PRIORITY_MAP( deadline )
  );
 //@assert priority_node->priority == SCHEDULER_PRIORITY_MAP( deadline);
  if ( _Priority_Node_is_active( priority_node ) ) {
     //@assert priority_node->priority == SCHEDULER_PRIORITY_MAP( deadline);
    _Thread_Priority_changed(
      the_thread,
      priority_node,
      PRIORITY_GROUP_LAST,
      queue_context
    );
  } else {
    _Thread_Priority_add( the_thread, priority_node, queue_context );
  }
  _Thread_Wait_release_critical( the_thread, queue_context );
}

/*@
  requires \valid(the_thread->Wait.queue);
  requires \valid(the_thread->Wait.operations);
  requires \valid(the_thread->Scheduler.nodes);
  requires \valid(the_thread);
  requires \valid(priority_node);
  requires \valid(queue_context);
  requires \valid(g_scheduler_node_of_wait_priority_node) && \valid(g_min_priority_node);
  requires \valid(queue_context->Priority.update[0]);
  requires queue_context->Priority.update_count == 0;
  requires the_thread->Wait.operations->priority_actions ==
    _Thread_queue_Do_nothing_priority_actions;

  requires \separated(
    queue_context,
    the_thread->Scheduler.nodes,
    the_thread->Wait.operations,
    the_thread->Wait.queue,
    the_thread
  );

  requires \separated(
    priority_node,
    &the_thread->Scheduler.nodes->Wait.Priority,
    g_scheduler_node_of_wait_priority_node,
    g_min_priority_node,
    &queue_context->Priority.update_count
  );

  behavior priority_node_new_new_min:
    assumes priority_node->Node.RBTree.Node.rbe_color != -1;
    assumes priority_node->priority < g_min_priority_node->priority;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value == (g_min_priority_node->priority | 0);
    ensures \old(the_thread->Scheduler.nodes)->Wait.Priority.Node.priority == g_min_priority_node->priority;
    ensures queue_context->Priority.update[0] == the_thread;
    ensures queue_context->Priority.update_count == 1;
    ensures priority_node->Node.RBTree.Node.rbe_color == -1;

  behavior priority_node_remove_no_new_min:
    assumes priority_node->Node.RBTree.Node.rbe_color != -1;
    assumes !(priority_node->priority < g_min_priority_node->priority);
    ensures g_scheduler_node_of_wait_priority_node->Priority.value == \old(g_scheduler_node_of_wait_priority_node->Priority.value);
    ensures priority_node->Node.RBTree.Node.rbe_color == -1;

  behavior priority_node_not_active:
    assumes priority_node->Node.RBTree.Node.rbe_color == -1;
    assigns \nothing;
    ensures the_thread->Scheduler.nodes->Priority.value == \old(the_thread->Scheduler.nodes->Priority.value);
    ensures priority_node->Node.RBTree.Node.rbe_color == -1;

  disjoint behaviors;
  complete behaviors;
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
}
