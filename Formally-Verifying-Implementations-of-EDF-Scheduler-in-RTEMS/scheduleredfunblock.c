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

#include <rtems/score/scheduleredfimpl.h>
#include <rtems/score/schedulerimpl.h>
#include <rtems/score/thread.h>

/*@
  requires \valid_read(scheduler) && \valid(the_thread) && \valid(node) && \valid((Scheduler_EDF_Node *) node) && \valid(node);
  requires \valid(the_thread) && \valid(_Thread_Heir) && \valid(_Per_CPU_Get());
  requires \valid(g_edf_sched_context);
  requires \valid(_Thread_Heir->Scheduler.nodes);
  requires \separated(
        _Thread_Heir->Scheduler.nodes,
    ((Scheduler_EDF_Node *) node)
  );
  requires &((Scheduler_EDF_Node *) node)->Base == node;
  requires \separated(
    the_thread, 
    _Per_CPU_Get()
    );

  behavior exec_update_new_h:
    assumes SCHEDULER_PRIORITY_PURIFY(((Scheduler_EDF_Node *) node)->Base.Priority.value)  < _Thread_Heir->Scheduler.nodes->Wait.Priority.Node.priority;
    assumes (_Thread_Heir != the_thread && ( _Thread_Heir->is_preemptible || (SCHEDULER_PRIORITY_PURIFY(node->Priority.value) == ( SCHEDULER_EDF_PRIO_MSB | PRIORITY_PSEUDO_ISR ))));
    assigns _Thread_Heir, _Thread_Dispatch_necessary, ((Scheduler_EDF_Node *) node)->priority;
    ensures _Thread_Heir == the_thread;
    ensures ((Scheduler_EDF_Node *) node)->priority == SCHEDULER_PRIORITY_PURIFY(node->Priority.value);
    //ensures _Thread_Dispatch_necessary == true;
  
  behavior exec_update_no_new_h:
    assumes SCHEDULER_PRIORITY_PURIFY(((Scheduler_EDF_Node *) node)->Base.Priority.value)  < _Thread_Heir->Scheduler.nodes->Wait.Priority.Node.priority;
    assumes !(_Thread_Heir != the_thread && ( _Thread_Heir->is_preemptible || (SCHEDULER_PRIORITY_PURIFY(node->Priority.value) == ( SCHEDULER_EDF_PRIO_MSB | PRIORITY_PSEUDO_ISR ))));
    assigns ((Scheduler_EDF_Node *) node)->priority;
    ensures ((Scheduler_EDF_Node *) node)->priority == SCHEDULER_PRIORITY_PURIFY(node->Priority.value);

  behavior priority_no_new_min:
    assumes SCHEDULER_PRIORITY_PURIFY(((Scheduler_EDF_Node *) node)->Base.Priority.value) >= _Thread_Heir->Scheduler.nodes->Wait.Priority.Node.priority; 
    assigns ((Scheduler_EDF_Node *) node)->priority;
    ensures ((Scheduler_EDF_Node *) node)->priority == SCHEDULER_PRIORITY_PURIFY(node->Priority.value);

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
   */
  if ( priority < _Thread_Get_priority( _Thread_Heir ) ) {
    //@ assert _Thread_Heir == \at(_Thread_Heir, Pre);
    _Scheduler_Update_heir(
      the_thread,
      priority == ( SCHEDULER_EDF_PRIO_MSB | PRIORITY_PSEUDO_ISR )
    );
  }
}
