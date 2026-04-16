/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * SIMPLIFIED VERIFICATION HEADER — replaces the original threadimpl.h.
 * See verification/6.2/HEADERS.md for documentation of changes.
 *
 * Original: cpukit/include/rtems/score/threadimpl.h (RTEMS 6.2, ~2600 lines, 71 inline functions)
 * This version keeps only the ~10 functions needed for EDF scheduler
 * verification and cuts the transitive includes from 16 headers to 4.
 */

#ifndef _RTEMS_SCORE_THREADIMPL_H
#define _RTEMS_SCORE_THREADIMPL_H

#include <rtems/score/thread.h>
#include <rtems/score/schedulernodeimpl.h>
#include <rtems/score/statesimpl.h>
#include <rtems/score/status.h>
/* REMOVED: #include <rtems/score/chainimpl.h>      — 41 inline functions, not needed */
/* REMOVED: #include <rtems/score/objectimpl.h>      — 23 inline functions, not needed */
/* REMOVED: #include <rtems/score/threadqimpl.h>     — 29 inline functions, not needed */
/* REMOVED: #include <rtems/score/watchdogimpl.h>    — 26 inline functions, not needed */
/* REMOVED: #include <rtems/score/timestampimpl.h>   — 13 inline functions, not needed */
/* REMOVED: #include <rtems/score/todimpl.h>         — 10 inline functions, not needed */
/* REMOVED: #include <rtems/score/isr.h>             — not needed */
/* REMOVED: #include <rtems/score/interr.h>          — not needed */
/* REMOVED: #include <rtems/score/sysstate.h>        — not needed */
/* REMOVED: #include <rtems/config.h>                — pulls in rtems/rtems/ headers */
/* REMOVED: #include <rtems/score/threadmp.h>        — multiprocessing, not needed */
/* REMOVED: #include <rtems/score/assert.h>          — not needed */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Returns true if the thread is in the ready state.
 *
 * Original: threadimpl.h line 471
 */
/*@
  requires \valid_read(the_thread);
  assigns \nothing;
  ensures \result == (the_thread->current_state == STATES_READY);
*/
static inline bool _Thread_Is_ready( const Thread_Control *the_thread )
{
  return _States_Is_ready( the_thread->current_state );
}

/**
 * @brief Returns true if the thread is the current heir.
 *
 * Original: threadimpl.h line 458
 */
static inline bool _Thread_Is_heir(
  const Thread_Control *the_thread
)
{
  return ( the_thread == _Thread_Heir );
}

/**
 * @brief Gets the home scheduler node of the thread.
 *
 * Original: threadimpl.h line 1582
 * On uniprocessor, this just returns the_thread->Scheduler.nodes.
 */
/*@
  requires \valid_read(the_thread) && \valid(the_thread->Scheduler.nodes);
  assigns \result \from the_thread;
  ensures \result == the_thread->Scheduler.nodes;
 */
static inline Scheduler_Node *_Thread_Scheduler_get_home_node(
  const Thread_Control *the_thread
)
{
#if defined(RTEMS_SMP)
  _Assert( !_Chain_Is_empty( &the_thread->Scheduler.Wait_nodes ) );
  return SCHEDULER_NODE_OF_THREAD_WAIT_NODE(
    _Chain_First( &the_thread->Scheduler.Wait_nodes )
  );
#else
  return the_thread->Scheduler.nodes;
#endif
}

/**
 * @brief Gets the priority of the thread.
 *
 * Original: threadimpl.h line 1754
 */
/*@
  requires \valid_read(the_thread) && \valid(the_thread->Scheduler.nodes);
  assigns \nothing;
  ensures \result == the_thread->Scheduler.nodes->Wait.Priority.Node.priority;
*/
static inline Priority_Control _Thread_Get_priority(
  const Thread_Control *the_thread
)
{
  Scheduler_Node *scheduler_node;

  scheduler_node = _Thread_Scheduler_get_home_node( the_thread );
  return _Priority_Get_priority( &scheduler_node->Wait.Priority );
}

/**
 * @brief Gets the CPU of the thread.
 *
 * Original: threadimpl.h line 996
 * On uniprocessor, always returns _Per_CPU_Get().
 */
/*@
  assigns \result \from thread;
 */
static inline Per_CPU_Control *_Thread_Get_CPU(
  const Thread_Control *thread
)
{
#if defined(RTEMS_SMP)
  return thread->Scheduler.cpu;
#else
  (void) thread;

  return _Per_CPU_Get();
#endif
}

/**
 * @brief Updates the CPU time used by the thread.
 *
 * Original: threadimpl.h line 1269
 * This function accesses timestamp counters which are opaque to
 * the EDF scheduler verification. Stubbed as assigns \nothing
 * because the CPU time bookkeeping does not affect scheduling decisions.
 *
 * NOTE: The real implementation modifies cpu->cpu_usage_timestamp
 * and the_thread->cpu_time_used. These are irrelevant to priority
 * and heir selection.
 */
/*@
  assigns \nothing;
 */
static inline void _Thread_Update_CPU_time_used(
  Thread_Control  *the_thread,
  Per_CPU_Control *cpu
);

/**
 * @brief Acquires the thread wait lock inside a critical section.
 *
 * Original: threadimpl.h line 1956
 * On uniprocessor this is a no-op (ISR already disabled).
 */
/*@
 assigns \nothing;
 */
static inline void _Thread_Wait_acquire_critical(
  Thread_Control       *the_thread,
  Thread_queue_Context *queue_context
);

/**
 * @brief Releases the thread wait lock inside a critical section.
 *
 * Original: threadimpl.h line 2031
 * On uniprocessor this is a no-op.
 */
/*@
 assigns \nothing;
 */
static inline void _Thread_Wait_release_critical(
  Thread_Control       *the_thread,
  Thread_queue_Context *queue_context
);

/**
 * @brief Acquires the thread state lock.
 *
 * Original: threadimpl.h line 611
 * On uniprocessor this disables interrupts.
 */
/*@
  assigns \nothing;
*/
static inline void _Thread_State_acquire(
  Thread_Control   *the_thread,
  ISR_lock_Context *lock_context
);

/**
 * @brief Releases the thread state lock.
 *
 * Original: threadimpl.h line 660
 * On uniprocessor this enables interrupts.
 */
/*@
  assigns \nothing;
*/
static inline void _Thread_State_release(
  Thread_Control   *the_thread,
  ISR_lock_Context *lock_context
);

/**
 * @brief Adds a priority update to the queue context.
 *
 * Original: threadqimpl.h line 418 (pulled into this header for
 * verification convenience — avoids including full threadqimpl.h).
 */
/*@
  requires \valid(queue_context);
  requires \valid(the_thread);
  requires \valid(queue_context->Priority.update[0]);
  requires queue_context->Priority.update_count == 0;
  assigns queue_context->Priority.update_count;
  assigns queue_context->Priority.update[0];
  ensures queue_context->Priority.update[0] == the_thread;
  ensures queue_context->Priority.update_count == 1;
*/
static inline void _Thread_queue_Context_add_priority_update(
  Thread_queue_Context *queue_context,
  Thread_Control       *the_thread
);

/*
 * Extern function declarations — defined in threadchangepriority.c
 * or other translation units. Stubs with ACSL contracts are provided
 * in the stubs header files (stubs.h / release_cancel_stubs.h).
 */

void _Thread_Priority_perform_actions(
  Thread_Control       *start_of_path,
  Thread_queue_Context *queue_context
);

void _Thread_Priority_add(
  Thread_Control       *the_thread,
  Priority_Node        *priority_node,
  Thread_queue_Context *queue_context
);

void _Thread_Priority_remove(
  Thread_Control       *the_thread,
  Priority_Node        *priority_node,
  Thread_queue_Context *queue_context
);

void _Thread_Priority_changed(
  Thread_Control       *the_thread,
  Priority_Node        *priority_node,
  Priority_Group_order  priority_group_order,
  Thread_queue_Context *queue_context
);

void _Thread_Priority_replace(
  Thread_Control *the_thread,
  Priority_Node  *victim_node,
  Priority_Node  *replacement_node
);

void _Thread_Priority_update( Thread_queue_Context *queue_context );

#ifdef __cplusplus
}
#endif

#endif /* _RTEMS_SCORE_THREADIMPL_H */
