/**
 *  @file
 *
 *  @brief Scheduler EDF Extract
 *  @ingroup RTEMSScoreScheduler
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

#include <rtems/score/scheduleredfimpl.h>

/*@
  requires \valid_read(scheduler);
  requires \valid(the_thread);
  requires \valid(node);
  requires \valid(g_edf_sched_context);
  requires \valid(g_min_edf_node->Base.owner) && \valid(_Thread_Heir) && \valid(_Per_CPU_Get());
  requires \separated(_Thread_Heir, _Per_CPU_Get());

  //requires \separated(&g_min_edf_node->Base, g_min_edf_node);
  
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
    ensures _Thread_Dispatch_necessary == true;
  
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
  _Scheduler_EDF_Schedule_body( scheduler, the_thread, false );
}
