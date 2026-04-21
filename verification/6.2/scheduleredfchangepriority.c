/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSScoreSchedulerEDF
 *
 * @brief This source file contains the implementation of
 *   _Scheduler_EDF_Update_priority().
 *
 * VERIFICATION CHANGES from original RTEMS 6.2:
 * - ACSL contract added to _Scheduler_EDF_Update_priority
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
  requires \valid_read(scheduler);
  requires \valid(the_thread);
  requires \valid(node);
  requires \valid(g_edf_sched_context);
  requires \valid(g_min_edf_node);
  requires \valid(g_min_edf_node->Base.owner) && \valid(_Thread_Heir) && \valid(_Per_CPU_Get());
  requires \separated(_Thread_Heir, _Per_CPU_Get());
  requires \separated(node + (..), (Per_CPU_Control_envelope *)_Per_CPU_Information + (..));
  requires \separated(scheduler + (..), (Per_CPU_Control_envelope *)_Per_CPU_Information + (..));
  requires \separated(the_thread + (..), (Per_CPU_Control_envelope *)_Per_CPU_Information + (..));
  requires \separated(g_min_edf_node->Base.owner + (..), (Per_CPU_Control_envelope *)_Per_CPU_Information + (..));

  behavior thread_not_ready:
    assumes the_thread->current_state != STATES_READY;
    assigns \nothing;

  behavior no_new_prio:
    assumes the_thread->current_state == STATES_READY;
    assumes SCHEDULER_PRIORITY_PURIFY(node->Priority.value) == ((Scheduler_EDF_Node *) node)->priority;
    assigns \nothing;

  behavior exec_update_new_h:
    assumes the_thread->current_state == STATES_READY;
    assumes SCHEDULER_PRIORITY_PURIFY(node->Priority.value) != ((Scheduler_EDF_Node *) node)->priority;
    assumes _Thread_Heir != g_min_edf_node->Base.owner && ( _Thread_Heir->is_preemptible);
    assigns _Thread_Heir, _Thread_Dispatch_necessary, ((Scheduler_EDF_Node *) node)->priority;
    ensures _Thread_Heir == g_min_edf_node->Base.owner;
    ensures ((Scheduler_EDF_Node *) node)->priority == SCHEDULER_PRIORITY_PURIFY(node->Priority.value);
    //ensures _Thread_Dispatch_necessary == true; // volatile — unprovable

  behavior exec_update_no_new_h:
    assumes the_thread->current_state == STATES_READY;
    assumes SCHEDULER_PRIORITY_PURIFY(node->Priority.value) != ((Scheduler_EDF_Node *) node)->priority;
    assumes !(_Thread_Heir != g_min_edf_node->Base.owner && ( _Thread_Heir->is_preemptible ));
    assigns ((Scheduler_EDF_Node *) node)->priority;
    ensures ((Scheduler_EDF_Node *) node)->priority == SCHEDULER_PRIORITY_PURIFY(node->Priority.value);

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
    return;
  }

  the_node = _Scheduler_EDF_Node_downcast( node );
  insert_priority = _Scheduler_Node_get_priority( &the_node->Base );
  priority = SCHEDULER_PRIORITY_PURIFY( insert_priority );

  if ( priority == the_node->priority ) {
    /* Nothing to do */
    return;
  }

  the_node->priority = priority;
  context = _Scheduler_EDF_Get_context( scheduler );

  _Scheduler_EDF_Extract( context, the_node );
  _Scheduler_EDF_Enqueue( context, the_node, insert_priority );
  //@ calls _Scheduler_EDF_Get_highest_ready;
  _Scheduler_uniprocessor_Schedule(
    scheduler,
    _Scheduler_EDF_Get_highest_ready
  );
}
