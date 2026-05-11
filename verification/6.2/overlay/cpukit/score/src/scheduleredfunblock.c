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

#include <rtems/score/scheduleredfimpl.h>
#include <rtems/score/schedulerimpl.h>
#include <rtems/score/thread.h>

/*@
  requires \valid_read( scheduler );
  requires \valid( the_thread );
  requires \valid( (Scheduler_EDF_Node *) node );
  requires &((Scheduler_EDF_Node *) node)->Base == node;
  requires \valid( (Scheduler_EDF_Context *) scheduler->context );
  requires edf_ready_context_well_formed{Pre}(
    (Scheduler_EDF_Context *) scheduler->context );
  requires !edf_ready_member{Pre}(
    (Scheduler_EDF_Context *) scheduler->context,
    (Scheduler_EDF_Node *) node );

  requires \valid_read( &_Thread_Heir->is_preemptible );
  requires \valid_read( &_Thread_Heir->Scheduler.nodes );
  requires \valid( _Thread_Heir->Scheduler.nodes );

  assigns ((Scheduler_EDF_Node *) node)->Base.Priority,
          ((Scheduler_EDF_Node *) node)->priority,
          ((Scheduler_EDF_Context *) scheduler->context)->Ready,
          { other->Node | Scheduler_EDF_Node *other; \valid( other ) },
          _Thread_Heir, _Thread_Dispatch_necessary;

  ensures ((Scheduler_EDF_Node *) node)->priority ==
    SCHEDULER_PRIORITY_PURIFY( \at( node->Priority.value, Pre ) );
  ensures edf_ready_set{Post}(
            (Scheduler_EDF_Context *) scheduler->context ) ==
          edf_ready_insert(
            edf_ready_set{Pre}(
              (Scheduler_EDF_Context *) scheduler->context ),
            (Scheduler_EDF_Node *) node );

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
  _Scheduler_EDF_Enqueue( context, the_node, insert_priority );
  _Scheduler_uniprocessor_Unblock( scheduler, the_thread, priority );
}
