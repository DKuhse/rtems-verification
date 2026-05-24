/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSScoreScheduler
 *
 * @brief This header file provides interfaces of the
 *   @ref RTEMSScoreScheduler which are only used by the implementation.
 */

/*
 *  Copyright (C) 2010 Gedare Bloom.
 *  Copyright (C) 2011 On-Line Applications Research Corporation (OAR).
 *  Copyright (C) 2014, 2017 embedded brains GmbH & Co. KG
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

#ifndef _RTEMS_SCORE_SCHEDULERIMPL_H
#define _RTEMS_SCORE_SCHEDULERIMPL_H

#include <rtems/score/scheduler.h>
#include <rtems/score/assert.h>
#include <rtems/score/priorityimpl.h>
#include <rtems/score/smpimpl.h>
#include <rtems/score/status.h>
#include <rtems/score/threadimpl.h>

#ifdef __FRAMAC__
extern const Scheduler_Control _Scheduler_Table[ 1 ];
#include <rtems/score/scheduleredf.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup RTEMSScoreScheduler Scheduler Handler
 *
 * @ingroup RTEMSScore
 *
 * @brief This group contains the Scheduler Handler implementation.
 *
 * This handler encapsulates functionality related to managing sets of threads
 * that are ready for execution.
 *
 * Schedulers are used by the system to manage sets of threads that are ready
 * for execution.  A scheduler consists of
 *
 * * a scheduler algorithm implementation,
 *
 * * a scheduler index and an associated name, and
 *
 * * a set of processors owned by the scheduler (may be empty, but never
 *   overlaps with a set owned by another scheduler).
 *
 * Each thread uses exactly one scheduler as its home scheduler.  Threads may
 * temporarily use another scheduler due to actions of locking protocols.
 *
 * All properties of a scheduler can be configured and controlled by the user.
 * Some properties are fixed at link time (defined by application configuration
 * options), other properties can be changed at runtime through directive
 * calls.
 *
 * The scheduler index, name, and initial processor set are defined for a
 * particular application by the application configuration.  The schedulers are
 * registered in the ::_Scheduler_Table which has ::_Scheduler_Count entries.
 *
 * @{
 */

/**
 * @brief Initializes the scheduler to the policy chosen by the user.
 *
 * This routine initializes the scheduler to the policy chosen by the user
 * through confdefs, or to the priority scheduler with ready chains by
 * default.
 */
void _Scheduler_Handler_initialization( void );

/**
 * @brief Gets the context of the scheduler.
 *
 * @param scheduler The scheduler to get the context of.
 *
 * @return The context of @a scheduler.
 */
/*@
  requires \valid_read( scheduler );

  assigns \result \from scheduler->context;

  ensures \result == scheduler->context;
*/
static inline Scheduler_Context *_Scheduler_Get_context(
  const Scheduler_Control *scheduler
)
{
  return scheduler->context;
}

/**
 * @brief Gets the scheduler for the cpu.
 *
 * @param cpu The cpu control to get the scheduler of.
 *
 * @return The scheduler for the cpu.
 */
static inline const Scheduler_Control *_Scheduler_Get_by_CPU(
  const Per_CPU_Control *cpu
)
{
#if defined(RTEMS_SMP)
  return cpu->Scheduler.control;
#else
  (void) cpu;
  return &_Scheduler_Table[ 0 ];
#endif
}

/**
 * @brief Acquires the scheduler instance inside a critical section (interrupts
 * disabled).
 *
 * @param scheduler The scheduler instance.
 * @param lock_context The lock context to use for
 *   _Scheduler_Release_critical().
 */
static inline void _Scheduler_Acquire_critical(
  const Scheduler_Control *scheduler,
  ISR_lock_Context        *lock_context
)
{
#if defined(RTEMS_SMP)
  Scheduler_Context *context;

  context = _Scheduler_Get_context( scheduler );
  _ISR_lock_Acquire( &context->Lock, lock_context );
#else
  (void) scheduler;
  (void) lock_context;
#endif
}

/**
 * @brief Releases the scheduler instance inside a critical section (interrupts
 * disabled).
 *
 * @param scheduler The scheduler instance.
 * @param lock_context The lock context used for
 *   _Scheduler_Acquire_critical().
 */
static inline void _Scheduler_Release_critical(
  const Scheduler_Control *scheduler,
  ISR_lock_Context        *lock_context
)
{
#if defined(RTEMS_SMP)
  Scheduler_Context *context;

  context = _Scheduler_Get_context( scheduler );
  _ISR_lock_Release( &context->Lock, lock_context );
#else
  (void) scheduler;
  (void) lock_context;
#endif
}

#if defined(RTEMS_SMP)
/**
 * @brief Indicate if the thread non-preempt mode is supported by the
 * scheduler.
 *
 * @param scheduler The scheduler instance.
 *
 * @return True if the non-preempt mode for threads is supported by the
 *   scheduler, otherwise false.
 */
static inline bool _Scheduler_Is_non_preempt_mode_supported(
  const Scheduler_Control *scheduler
)
{
  return scheduler->is_non_preempt_mode_supported;
}
#endif

/**
 * The preferred method to add a new scheduler is to define the jump table
 * entries and add a case to the _Scheduler_Initialize routine.
 *
 * Generic scheduling implementations that rely on the ready queue only can
 * be found in the _Scheduler_queue_XXX functions.
 */

/*
 * Passing the Scheduler_Control* to these functions allows for multiple
 * scheduler's to exist simultaneously, which could be useful on an SMP
 * system.  Then remote Schedulers may be accessible.  How to protect such
 * accesses remains an open problem.
 */

/**
 * @brief General scheduling decision.
 *
 * This kernel routine implements the scheduling decision logic for
 * the scheduler. It does NOT dispatch.
 *
 * @param the_thread The thread which state changed previously.
 */
static inline void _Scheduler_Schedule( Thread_Control *the_thread )
{
  const Scheduler_Control *scheduler;
  ISR_lock_Context         lock_context;

  scheduler = _Thread_Scheduler_get_home( the_thread );
  _Scheduler_Acquire_critical( scheduler, &lock_context );

  ( *scheduler->Operations.schedule )( scheduler, the_thread );

  _Scheduler_Release_critical( scheduler, &lock_context );
}

/**
 * @brief Scheduler yield with a particular thread.
 *
 * This routine is invoked when a thread wishes to voluntarily transfer control
 * of the processor to another thread.
 *
 * @param the_thread The yielding thread.
 */
static inline void _Scheduler_Yield( Thread_Control *the_thread )
{
  const Scheduler_Control *scheduler;
  ISR_lock_Context         lock_context;

  scheduler = _Thread_Scheduler_get_home( the_thread );
  _Scheduler_Acquire_critical( scheduler, &lock_context );
  ( *scheduler->Operations.yield )(
    scheduler,
    the_thread,
    _Thread_Scheduler_get_home_node( the_thread )
  );
  _Scheduler_Release_critical( scheduler, &lock_context );
}

/**
 * @brief Blocks a thread with respect to the scheduler.
 *
 * This routine removes @a the_thread from the scheduling decision for
 * the scheduler. The primary task is to remove the thread from the
 * ready queue.  It performs any necessary scheduling operations
 * including the selection of a new heir thread.
 *
 * @param the_thread The thread.
 */
static inline void _Scheduler_Block( Thread_Control *the_thread )
{
#if defined(RTEMS_SMP)
  Chain_Node              *node;
  const Chain_Node        *tail;
  Scheduler_Node          *scheduler_node;
  const Scheduler_Control *scheduler;
  ISR_lock_Context         lock_context;

  node = _Chain_First( &the_thread->Scheduler.Scheduler_nodes );
  tail = _Chain_Immutable_tail( &the_thread->Scheduler.Scheduler_nodes );

  scheduler_node = SCHEDULER_NODE_OF_THREAD_SCHEDULER_NODE( node );
  scheduler = _Scheduler_Node_get_scheduler( scheduler_node );

  _Scheduler_Acquire_critical( scheduler, &lock_context );
  ( *scheduler->Operations.block )(
    scheduler,
    the_thread,
    scheduler_node
  );
  _Scheduler_Release_critical( scheduler, &lock_context );

  node = _Chain_Next( node );

  while ( node != tail ) {
    scheduler_node = SCHEDULER_NODE_OF_THREAD_SCHEDULER_NODE( node );
    scheduler = _Scheduler_Node_get_scheduler( scheduler_node );

    _Scheduler_Acquire_critical( scheduler, &lock_context );
    ( *scheduler->Operations.withdraw_node )(
      scheduler,
      the_thread,
      scheduler_node,
      THREAD_SCHEDULER_BLOCKED
    );
    _Scheduler_Release_critical( scheduler, &lock_context );

    node = _Chain_Next( node );
  }
#else
  const Scheduler_Control *scheduler;

  scheduler = _Thread_Scheduler_get_home( the_thread );
  ( *scheduler->Operations.block )(
    scheduler,
    the_thread,
    _Thread_Scheduler_get_home_node( the_thread )
  );
#endif
}

/**
 * @brief Unblocks a thread with respect to the scheduler.
 *
 * This operation must fetch the latest thread priority value for this
 * scheduler instance and update its internal state if necessary.
 *
 * @param the_thread The thread.
 *
 * @see _Scheduler_Node_get_priority().
 */
static inline void _Scheduler_Unblock( Thread_Control *the_thread )
{
  Scheduler_Node          *scheduler_node;
  const Scheduler_Control *scheduler;
  ISR_lock_Context         lock_context;

#if defined(RTEMS_SMP)
  scheduler_node = SCHEDULER_NODE_OF_THREAD_SCHEDULER_NODE(
    _Chain_First( &the_thread->Scheduler.Scheduler_nodes )
  );
  scheduler = _Scheduler_Node_get_scheduler( scheduler_node );
#else
  scheduler_node = _Thread_Scheduler_get_home_node( the_thread );
  scheduler = _Thread_Scheduler_get_home( the_thread );
#endif

  _Scheduler_Acquire_critical( scheduler, &lock_context );
  ( *scheduler->Operations.unblock )( scheduler, the_thread, scheduler_node );
  _Scheduler_Release_critical( scheduler, &lock_context );
}

/**
 * @brief Propagates a priority change of a thread to the scheduler.
 *
 * On uni-processor configurations, this operation must evaluate the thread
 * state.  In case the thread is not ready, then the priority update should be
 * deferred to the next scheduler unblock operation.
 *
 * The operation must update the heir and thread dispatch necessary variables
 * in case the set of scheduled threads changes.
 *
 * @param the_thread The thread changing its priority.
 *
 * @see _Scheduler_Node_get_priority().
 */
#if !defined(RTEMS_SMP)
/*@
  requires \valid_read( _Scheduler_Table + ( 0 .. 0 ) );
  requires _Scheduler_Table[ 0 ].Operations.update_priority ==
    _Scheduler_EDF_Update_priority;
  requires \valid( (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
  requires edf_ready_context_well_formed{Pre}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
  requires thread_priority_edf_node_valid{Pre}( the_thread );
  requires \separated(
    the_thread + (..),
    the_thread->Scheduler.nodes + (..)
  );
  requires thread_priority_edf_heir_valid{Pre}( _Thread_Heir );
  requires edf_scheduler_decision{Pre}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    _Per_CPU_Information[ 0 ].per_cpu.executing,
    _Thread_Heir,
    _Thread_Heir->is_preemptible,
    _Thread_Dispatch_necessary_ghost );
  requires edf_preemptible_heir_is_earliest_ready{Pre}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    _Thread_Heir,
    _Thread_Heir->is_preemptible );
  requires thread_priority_edf_update_ready_pre{Pre}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    the_thread );
  requires the_thread->current_state == STATES_READY ==>
    edf_ready_member{Pre}(
      (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
      (Scheduler_EDF_Node *) the_thread->Scheduler.nodes );
  requires the_thread->current_state == STATES_READY ==>
    SCHEDULER_PRIORITY_PURIFY( the_thread->Scheduler.nodes->Priority.value ) ==
      ((Scheduler_EDF_Node *)
        the_thread->Scheduler.nodes)->Base.Wait.Priority.Node.priority;
  requires thread_priority_edf_update_separated{Pre}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    the_thread );
  requires \separated(
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..),
    (Scheduler_Control const *) _Scheduler_Table + (..),
    &((Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context)->Ready
  );
  requires \separated(
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..),
    (Scheduler_Control const *) _Scheduler_Table + (..),
    &((Scheduler_EDF_Node *) the_thread->Scheduler.nodes)->Base.Priority
  );
  requires \separated(
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..),
    (Scheduler_Control const *) _Scheduler_Table + (..),
    &((Scheduler_EDF_Node *) the_thread->Scheduler.nodes)->priority
  );
  requires \separated(
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..),
    (Scheduler_Control const *) _Scheduler_Table + (..),
    &(_Per_CPU_Information[ 0 ].per_cpu.heir)->cpu_time_used
  );
  requires \separated(
    the_thread + (..),
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..),
    (Scheduler_Control const *) _Scheduler_Table + (..)
  );

  assigns ((Scheduler_EDF_Node *) the_thread->Scheduler.nodes)->priority,
          ((Scheduler_EDF_Node *) the_thread->Scheduler.nodes)->Base.Priority,
          ((Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context)->Ready,
          _Per_CPU_Information[ 0 ].per_cpu.heir,
          _Per_CPU_Information[ 0 ].per_cpu.dispatch_necessary,
          _Thread_Dispatch_necessary_ghost,
          _Thread_Heir->cpu_time_used,
          _Per_CPU_Information[ 0 ].per_cpu.heir->cpu_time_used,
          _Per_CPU_Information[ 0 ].per_cpu.cpu_usage_timestamp;

  ensures edf_scheduler_decision{Post}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    _Per_CPU_Information[ 0 ].per_cpu.executing,
    _Thread_Heir,
    _Thread_Heir->is_preemptible,
    _Thread_Dispatch_necessary_ghost );
  ensures edf_preemptible_heir_is_earliest_ready{Post}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    _Thread_Heir,
    _Thread_Heir->is_preemptible );
  ensures edf_ready_context_well_formed{Post}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
  ensures edf_ready_set{Post}(
            (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context ) ==
          edf_ready_set{Pre}(
            (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
  ensures edf_priority_cache_consistency_preserved{Pre,Post}(
    edf_ready_set{Pre}( (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context ) );
  ensures the_thread->current_state == STATES_READY ==>
    edf_ready_node_cache_consistent{Post}(
      (Scheduler_EDF_Node *) the_thread->Scheduler.nodes );
  ensures priority_contributors{Post}(
            &the_thread->Scheduler.nodes->Wait.Priority ) ==
          priority_contributors{Pre}(
            &the_thread->Scheduler.nodes->Wait.Priority );
  ensures \at(
    priority_aggregation_well_formed(
      &the_thread->Scheduler.nodes->Wait.Priority ),
    Pre
  ) ==>
    priority_aggregation_well_formed{Post}(
      &the_thread->Scheduler.nodes->Wait.Priority );
  ensures \at(
    priority_aggregation_cached_minimum(
      &the_thread->Scheduler.nodes->Wait.Priority ),
    Pre
  ) ==>
    priority_aggregation_cached_minimum{Post}(
      &the_thread->Scheduler.nodes->Wait.Priority );
  ensures \forall Priority_Node *priority_node;
    \valid_read( priority_node ) &&
    \separated(
      &priority_node->priority,
      &((Scheduler_EDF_Node *) \at( the_thread->Scheduler.nodes, Pre ))->priority
    ) ==>
      priority_node->priority == \at( priority_node->priority, Pre );
*/
#endif
static inline void _Scheduler_Update_priority( Thread_Control *the_thread )
{
#if defined(RTEMS_SMP)
  Chain_Node       *node;
  const Chain_Node *tail;

  _Thread_Scheduler_process_requests( the_thread );

  node = _Chain_First( &the_thread->Scheduler.Scheduler_nodes );
  tail = _Chain_Immutable_tail( &the_thread->Scheduler.Scheduler_nodes );

  do {
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
  } while ( node != tail );
#else
  const Scheduler_Control *scheduler;
  Scheduler_Node          *scheduler_node;

  scheduler = _Thread_Scheduler_get_home( the_thread );
  scheduler_node = _Thread_Scheduler_get_home_node( the_thread );
  /*@ assert scheduler == &_Scheduler_Table[ 0 ]; */
  /*@ assert scheduler_node == the_thread->Scheduler.nodes; */
  /*@ assert (Scheduler_EDF_Context *) scheduler->context ==
        (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context; */
  /*@ assert &((Scheduler_EDF_Node *) scheduler_node)->Base == scheduler_node; */
  /*@ assert ((Scheduler_EDF_Node *) scheduler_node)->Base.owner == the_thread; */
  /*@ assert edf_scheduler_decision{Here}(
        (Scheduler_EDF_Context *) scheduler->context,
        _Per_CPU_Information[ 0 ].per_cpu.executing,
        _Thread_Heir,
        _Thread_Heir->is_preemptible,
        _Thread_Dispatch_necessary_ghost ); */
  /*@ assert the_thread->current_state == STATES_READY ==>
        edf_ready_member{Here}(
          (Scheduler_EDF_Context *) scheduler->context,
          (Scheduler_EDF_Node *) scheduler_node ); */
  /*@ assert the_thread->current_state == STATES_READY ==>
        SCHEDULER_PRIORITY_PURIFY( the_thread->Scheduler.nodes->Priority.value ) ==
          ((Scheduler_EDF_Node *)
            the_thread->Scheduler.nodes)->Base.Wait.Priority.Node.priority; */
  /*@ assert the_thread->current_state == STATES_READY ==>
        SCHEDULER_PRIORITY_PURIFY( scheduler_node->Priority.value ) ==
        ((Scheduler_EDF_Node *) scheduler_node)->Base.Wait.Priority.Node.priority; */
#ifdef __FRAMAC__
Before_Update:
#endif
  /*@ calls _Scheduler_EDF_Update_priority; */
  ( *scheduler->Operations.update_priority )(
    scheduler,
    the_thread,
    scheduler_node
  );
  /*@ assert priority_contributors{Here}(
          &((Scheduler_EDF_Node *) scheduler_node)->Base.Wait.Priority ) ==
        priority_contributors{Before_Update}(
          &((Scheduler_EDF_Node *) scheduler_node)->Base.Wait.Priority ); */
  /*@ assert \at(
        priority_aggregation_well_formed(
          &((Scheduler_EDF_Node *) scheduler_node)->Base.Wait.Priority ),
        Before_Update
      ) ==>
        priority_aggregation_well_formed{Here}(
          &((Scheduler_EDF_Node *) scheduler_node)->Base.Wait.Priority ); */
  /*@ assert \at(
        priority_aggregation_cached_minimum(
          &((Scheduler_EDF_Node *) scheduler_node)->Base.Wait.Priority ),
        Before_Update
      ) ==>
        priority_aggregation_cached_minimum{Here}(
          &((Scheduler_EDF_Node *) scheduler_node)->Base.Wait.Priority ); */
  /*@ assert &the_thread->Scheduler.nodes->Wait.Priority ==
        &((Scheduler_EDF_Node *) scheduler_node)->Base.Wait.Priority; */
  /*@ assert priority_contributors{Before_Update}(
          &the_thread->Scheduler.nodes->Wait.Priority ) ==
        priority_contributors{Pre}(
          &the_thread->Scheduler.nodes->Wait.Priority ); */
  /*@ assert \at(
        priority_aggregation_well_formed(
          &the_thread->Scheduler.nodes->Wait.Priority ),
        Pre
      ) ==>
        priority_aggregation_well_formed{Before_Update}(
          &the_thread->Scheduler.nodes->Wait.Priority ); */
  /*@ assert \at(
        priority_aggregation_cached_minimum(
          &the_thread->Scheduler.nodes->Wait.Priority ),
        Pre
      ) ==>
        priority_aggregation_cached_minimum{Before_Update}(
          &the_thread->Scheduler.nodes->Wait.Priority ); */
  /*@ assert edf_scheduler_decision{Here}(
        (Scheduler_EDF_Context *) scheduler->context,
        _Per_CPU_Information[ 0 ].per_cpu.executing,
        _Thread_Heir,
        _Thread_Heir->is_preemptible,
        _Thread_Dispatch_necessary_ghost ); */
  /*@ assert edf_preemptible_heir_is_earliest_ready{Here}(
        (Scheduler_EDF_Context *) scheduler->context,
        _Thread_Heir,
        _Thread_Heir->is_preemptible ); */
  /*@ assert \forall Priority_Node *priority_node;
        \valid_read( priority_node ) &&
        \separated(
          &priority_node->priority,
          &((Scheduler_EDF_Node *) scheduler_node)->priority
        ) ==>
          priority_node->priority == \at( priority_node->priority, Pre ); */
#endif
}

/**
 * @brief Maps a thread priority from the user domain to the scheduler domain.
 *
 * Let M be the maximum scheduler priority.  The mapping must be bijective in
 * the closed interval [0, M], e.g. _Scheduler_Unmap_priority( scheduler,
 * _Scheduler_Map_priority( scheduler, p ) ) == p for all p in [0, M].  For
 * other values the mapping is undefined.
 *
 * @param scheduler The scheduler instance.
 * @param priority The user domain thread priority.
 *
 * @return The corresponding thread priority of the scheduler domain is returned.
 */
static inline Priority_Control _Scheduler_Map_priority(
  const Scheduler_Control *scheduler,
  Priority_Control         priority
)
{
  return ( *scheduler->Operations.map_priority )( scheduler, priority );
}

/**
 * @brief Unmaps a thread priority from the scheduler domain to the user domain.
 *
 * @param scheduler The scheduler instance.
 * @param priority The scheduler domain thread priority.
 *
 * @return The corresponding thread priority of the user domain is returned.
 */
static inline Priority_Control _Scheduler_Unmap_priority(
  const Scheduler_Control *scheduler,
  Priority_Control         priority
)
{
  return ( *scheduler->Operations.unmap_priority )( scheduler, priority );
}

/**
 * @brief Initializes a scheduler node.
 *
 * The scheduler node contains arbitrary data on function entry.  The caller
 * must ensure that _Scheduler_Node_destroy() will be called after a
 * _Scheduler_Node_initialize() before the memory of the scheduler node is
 * destroyed.
 *
 * @param scheduler The scheduler instance.
 * @param[out] node The scheduler node to initialize.
 * @param the_thread The thread of the scheduler node to initialize.
 * @param priority The thread priority.
 */
static inline void _Scheduler_Node_initialize(
  const Scheduler_Control *scheduler,
  Scheduler_Node          *node,
  Thread_Control          *the_thread,
  Priority_Control         priority
)
{
  ( *scheduler->Operations.node_initialize )(
    scheduler,
    node,
    the_thread,
    priority
  );
}

/**
 * @brief Destroys a scheduler node.
 *
 * The caller must ensure that _Scheduler_Node_destroy() will be called only
 * after a corresponding _Scheduler_Node_initialize().
 *
 * @param scheduler The scheduler instance.
 * @param[out] node The scheduler node to destroy.
 */
static inline void _Scheduler_Node_destroy(
  const Scheduler_Control *scheduler,
  Scheduler_Node          *node
)
{
  ( *scheduler->Operations.node_destroy )( scheduler, node );
}

/**
 * @brief Releases a job of a thread with respect to the scheduler.
 *
 * @param the_thread The thread.
 * @param priority_node The priority node of the job.
 * @param deadline The deadline in watchdog ticks since boot.
 * @param queue_context The thread queue context to provide the set of
 *   threads for _Thread_Priority_update().
 */
#if !defined(RTEMS_SMP)
/*@
  requires \valid_read( _Scheduler_Table + ( 0 .. 0 ) );
  requires _Scheduler_Table[ 0 ].Operations.release_job ==
    _Scheduler_EDF_Release_job;
  requires deadline < 0x8000000000000000;
  requires \valid( (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
  requires edf_ready_context_well_formed{Pre}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
  requires thread_priority_edf_heir_valid{Pre}( _Thread_Heir );
  requires edf_preemptible_heir_is_earliest_ready{Pre}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    _Thread_Heir,
    _Thread_Heir->is_preemptible );
  requires thread_priority_edf_update_ready_pre{Pre}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    the_thread );
  requires thread_priority_edf_node_valid{Pre}( the_thread );
  requires \valid_read( &the_thread->Scheduler.nodes );
  requires \valid( priority_node );
  requires \valid( queue_context );
  requires \valid( the_thread->Scheduler.nodes );
  requires priority_aggregation_well_formed{Pre}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  requires priority_aggregation_cached_minimum{Pre}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  requires priority_node_active_iff_contributor{Pre}(
    &the_thread->Scheduler.nodes->Wait.Priority,
    priority_node );
  requires \exists Priority_Node *node;
    node \in priority_contributors{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority );
  requires \forall Priority_Node *node;
    node \in priority_contributors{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority ) ==>
        \separated( queue_context + (..), node + (..) );
  requires \separated(
    _Scheduler_Table + ( 0 .. 0 ),
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context + (..),
    the_thread + (..),
    the_thread->Scheduler.nodes + (..),
    priority_node + (..),
    queue_context + (..)
  );
  requires \forall Scheduler_EDF_Node *node;
    node \in edf_ready_set{Pre}(
      (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context ) ==>
      \separated( queue_context + (..), node + (..) );
  requires \forall Scheduler_EDF_Node *node;
    node \in edf_ready_set{Pre}(
      (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context ) ==>
      \separated( &node->priority, &priority_node->priority );
  requires \forall Scheduler_EDF_Node *node;
    node \in edf_ready_set{Pre}(
      (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context ) ==>
      \separated(
        &node->priority,
        &the_thread->Scheduler.nodes->Wait.Priority,
        &the_thread->Scheduler.nodes->Priority.value
      );

  assigns priority_node->priority,
          the_thread->Scheduler.nodes->Wait.Priority,
          the_thread->Scheduler.nodes->Priority.value,
          queue_context->Priority;

  ensures priority_node->priority == SCHEDULER_PRIORITY_MAP( deadline );
  ensures priority_aggregation_well_formed{Post}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  ensures priority_aggregation_cached_minimum{Post}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  ensures ((Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context)->Ready.rbh_root ==
    \at( ((Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context)->Ready.rbh_root, Pre );
  ensures edf_ready_set{Post}(
            (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context ) ==
          edf_ready_set{Pre}(
            (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
  ensures edf_ready_context_well_formed{Post}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
  ensures thread_priority_edf_heir_valid{Post}( _Thread_Heir );
  ensures edf_preemptible_heir_is_earliest_ready{Post}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    _Thread_Heir,
    _Thread_Heir->is_preemptible );
  ensures _Thread_Heir == \at( _Thread_Heir, Pre );
  ensures _Per_CPU_Information[ 0 ].per_cpu.heir ==
    \at( _Per_CPU_Information[ 0 ].per_cpu.heir, Pre );
  ensures the_thread->Scheduler.nodes->Priority.value !=
            \at( the_thread->Scheduler.nodes->Priority.value, Pre ) ==>
          thread_priority_update_pending{Post}( queue_context, the_thread );
  ensures queue_context->Priority.update_count <= 1;
  ensures queue_context->Priority.update_count == 1 ==>
          queue_context->Priority.update[ 0 ] == the_thread;
  ensures queue_context->Priority.update_count == 1 ==>
          thread_priority_edf_update_ready_pre{Post}(
            (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
            the_thread );
  ensures queue_context->Priority.update_count == 1 &&
          the_thread->current_state == STATES_READY ==>
          edf_ready_member{Post}(
            (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
            (Scheduler_EDF_Node *) the_thread->Scheduler.nodes );
  ensures queue_context->Priority.update_count == 0 &&
          the_thread->current_state == STATES_READY &&
          \at( edf_ready_node_cache_consistent(
            (Scheduler_EDF_Node *) the_thread->Scheduler.nodes ), Pre ) ==>
          edf_ready_node_cache_consistent{Post}(
            (Scheduler_EDF_Node *) the_thread->Scheduler.nodes );

  behavior active:
    assumes priority_node_active{Pre}( priority_node );
    ensures priority_contributors{Post}(
              &the_thread->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors{Pre}(
              &the_thread->Scheduler.nodes->Wait.Priority );

  behavior active_noop:
    assumes priority_node_active{Pre}( priority_node );
    assumes \valid_read( &the_thread->Wait.operations );
    assumes \valid( the_thread->Wait.operations );
    assumes the_thread->Wait.operations->priority_actions ==
      _Thread_queue_Do_nothing_priority_actions;
    assumes queue_context->Priority.update_count <= 1;
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
      priority_node + (..),
      _Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority ) + (..)
    );
    assumes \separated(
      &queue_context->Priority.Actions.actions,
      &queue_context->Priority.update_count,
      queue_context->Priority.update + (0 .. 1),
      &priority_node->priority,
      &the_thread->Scheduler.nodes->Wait.Priority.Contributors,
      &the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
      &_Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
    );
    assumes \separated(
      the_thread->Wait.operations + (..),
      queue_context + (..),
      the_thread->Scheduler.nodes + (..),
      priority_node + (..)
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
    ensures queue_context->Priority.Actions.actions == \null;

  behavior inactive:
    assumes !priority_node_active{Pre}( priority_node );
    ensures priority_contributors{Post}(
              &the_thread->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors_insert(
              priority_contributors{Pre}(
                &the_thread->Scheduler.nodes->Wait.Priority ),
              priority_node );

  behavior inactive_noop:
    assumes !priority_node_active{Pre}( priority_node );
    assumes \valid_read( &the_thread->Wait.operations );
    assumes \valid( the_thread->Wait.operations );
    assumes the_thread->Wait.operations->priority_actions ==
      _Thread_queue_Do_nothing_priority_actions;
    assumes queue_context->Priority.update_count <= 1;
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
      priority_node + (..),
      _Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority ) + (..)
    );
    assumes \separated(
      &queue_context->Priority.Actions.actions,
      &queue_context->Priority.update_count,
      queue_context->Priority.update + (0 .. 1),
      &priority_node->priority,
      &the_thread->Scheduler.nodes->Wait.Priority.Contributors,
      &the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
      &_Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
    );
    assumes \separated(
      the_thread->Wait.operations + (..),
      queue_context + (..),
      the_thread->Scheduler.nodes + (..),
      priority_node + (..)
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
              priority_node );
    ensures priority_aggregation_well_formed{Post}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    ensures priority_aggregation_cached_minimum{Post}(
      &the_thread->Scheduler.nodes->Wait.Priority );
    ensures queue_context->Priority.Actions.actions == \null;

  complete behaviors active, inactive;
  disjoint behaviors active, inactive;
*/
#endif
static inline void _Scheduler_Release_job(
  Thread_Control       *the_thread,
  Priority_Node        *priority_node,
  uint64_t              deadline,
  Thread_queue_Context *queue_context
)
{
  const Scheduler_Control *scheduler = _Thread_Scheduler_get_home( the_thread );

  _Thread_queue_Context_clear_priority_updates( queue_context );
#ifdef __FRAMAC__
Before_Release_job:
#endif
  /*@ assert scheduler == &_Scheduler_Table[ 0 ]; */
  /*@ assert queue_context->Priority.update_count == 0; */
  /*@ assert (Scheduler_EDF_Context *) scheduler->context ==
        (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context; */
  /*@ assert edf_preemptible_heir_is_earliest_ready{Here}(
        (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
        _Thread_Heir,
        _Thread_Heir->is_preemptible ); */
  /*@ assert edf_preemptible_heir_is_earliest_ready{Here}(
        (Scheduler_EDF_Context *) scheduler->context,
        _Thread_Heir,
        _Thread_Heir->is_preemptible ); */
  /*@ calls _Scheduler_EDF_Release_job; */
  ( *scheduler->Operations.release_job )(
    scheduler,
    the_thread,
    priority_node,
    deadline,
    queue_context
  );
  /*@ assert \at( queue_context->Priority.update_count, Before_Release_job ) == 0; */
  /*@ assert queue_context->Priority.update_count == 1 ==>
        thread_priority_edf_update_ready_pre{Here}(
          (Scheduler_EDF_Context *) scheduler->context,
          the_thread ); */
  /*@ assert (Scheduler_EDF_Context *) scheduler->context ==
        (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context; */
}

/**
 * @brief Cancels a job of a thread with respect to the scheduler.
 *
 * @param the_thread The thread.
 * @param priority_node The priority node of the job.
 * @param queue_context The thread queue context to provide the set of
 *   threads for _Thread_Priority_update().
 */
#if !defined(RTEMS_SMP)
/*@
  requires \valid_read( _Scheduler_Table + ( 0 .. 0 ) );
  requires _Scheduler_Table[ 0 ].Operations.cancel_job ==
    _Scheduler_EDF_Cancel_job;
  requires \valid( (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
  requires edf_ready_context_well_formed{Pre}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
  requires thread_priority_edf_heir_valid{Pre}( _Thread_Heir );
  requires edf_preemptible_heir_is_earliest_ready{Pre}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    _Thread_Heir,
    _Thread_Heir->is_preemptible );
  requires thread_priority_edf_update_ready_pre{Pre}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    the_thread );
  requires thread_priority_edf_node_valid{Pre}( the_thread );
  requires \valid_read( &the_thread->Scheduler.nodes );
  requires \valid( priority_node );
  requires \valid( queue_context );
  requires \valid( the_thread->Scheduler.nodes );
  requires priority_aggregation_well_formed{Pre}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  requires priority_aggregation_cached_minimum{Pre}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  requires priority_node_active_iff_contributor{Pre}(
    &the_thread->Scheduler.nodes->Wait.Priority,
    priority_node );
  requires priority_node_active{Pre}( priority_node ) ==>
    ( \exists Priority_Node *other;
        other != priority_node &&
        other \in priority_contributors{Pre}(
          &the_thread->Scheduler.nodes->Wait.Priority ) );
  requires \forall Priority_Node *node;
    node \in priority_contributors{Pre}(
      &the_thread->Scheduler.nodes->Wait.Priority ) ==>
        \separated( queue_context + (..), node + (..) );
  requires \separated(
    _Scheduler_Table + ( 0 .. 0 ),
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context + (..),
    the_thread + (..),
    the_thread->Scheduler.nodes + (..),
    priority_node + (..),
    queue_context + (..)
  );
  requires \forall Scheduler_EDF_Node *node;
    node \in edf_ready_set{Pre}(
      (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context ) ==>
      \separated( queue_context + (..), node + (..) );

  assigns priority_node->Node.RBTree.Node.rbe_color,
          the_thread->Scheduler.nodes->Wait.Priority,
          the_thread->Scheduler.nodes->Priority.value,
          queue_context->Priority;

  ensures !priority_node_active{Post}( priority_node );
  ensures !\at( priority_node_active( priority_node ), Pre ) ==>
          priority_contributors{Post}(
            &the_thread->Scheduler.nodes->Wait.Priority ) ==
          priority_contributors{Pre}(
            &the_thread->Scheduler.nodes->Wait.Priority );
  ensures priority_aggregation_well_formed{Post}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  ensures priority_aggregation_cached_minimum{Post}(
    &the_thread->Scheduler.nodes->Wait.Priority );
  ensures ((Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context)->Ready.rbh_root ==
    \at( ((Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context)->Ready.rbh_root, Pre );
  ensures edf_ready_set{Post}(
            (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context ) ==
          edf_ready_set{Pre}(
            (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
  ensures edf_ready_context_well_formed{Post}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context );
  ensures thread_priority_edf_heir_valid{Post}( _Thread_Heir );
  ensures edf_preemptible_heir_is_earliest_ready{Post}(
    (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
    _Thread_Heir,
    _Thread_Heir->is_preemptible );
  ensures the_thread->Scheduler.nodes->Priority.value !=
            \at( the_thread->Scheduler.nodes->Priority.value, Pre ) ==>
          thread_priority_update_pending{Post}( queue_context, the_thread );
  ensures queue_context->Priority.update_count <= 1;
  ensures queue_context->Priority.update_count == 1 ==>
          queue_context->Priority.update[ 0 ] == the_thread;
  ensures queue_context->Priority.update_count == 1 ==>
          thread_priority_edf_update_ready_pre{Post}(
            (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
            the_thread );
  ensures queue_context->Priority.update_count == 1 &&
          the_thread->current_state == STATES_READY ==>
          edf_ready_member{Post}(
            (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
            (Scheduler_EDF_Node *) the_thread->Scheduler.nodes );
  ensures queue_context->Priority.update_count == 1 &&
          the_thread->current_state == STATES_READY ==>
          SCHEDULER_PRIORITY_PURIFY(
            the_thread->Scheduler.nodes->Priority.value ) ==
          ((Scheduler_EDF_Node *) the_thread->Scheduler.nodes)->
            Base.Wait.Priority.Node.priority;
  ensures queue_context->Priority.update_count == 0 &&
          the_thread->current_state == STATES_READY &&
          \at( edf_ready_node_cache_consistent(
            (Scheduler_EDF_Node *) the_thread->Scheduler.nodes ), Pre ) ==>
          edf_ready_node_cache_consistent{Post}(
            (Scheduler_EDF_Node *) the_thread->Scheduler.nodes );

  behavior active:
    assumes priority_node_active{Pre}( priority_node );
    ensures priority_contributors{Post}(
              &the_thread->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors_extract(
              priority_contributors{Pre}(
                &the_thread->Scheduler.nodes->Wait.Priority ),
              priority_node );

  behavior active_noop:
    assumes priority_node_active{Pre}( priority_node );
    assumes \valid_read( &the_thread->Wait.operations );
    assumes \valid( the_thread->Wait.operations );
    assumes the_thread->Wait.operations->priority_actions ==
      _Thread_queue_Do_nothing_priority_actions;
    assumes queue_context->Priority.update_count <= 1;
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
      priority_node + (..),
      _Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority ) + (..)
    );
    assumes \separated(
      &queue_context->Priority.Actions.actions,
      &queue_context->Priority.update_count,
      queue_context->Priority.update + (0 .. 1),
      &priority_node->priority,
      &the_thread->Scheduler.nodes->Wait.Priority.Contributors,
      &the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
      &_Priority_Verify_scheduler_node_of_aggregation(
        &the_thread->Scheduler.nodes->Wait.Priority )->Priority.value
    );
    assumes \separated(
      the_thread->Wait.operations + (..),
      queue_context + (..),
      the_thread->Scheduler.nodes + (..),
      priority_node + (..)
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
              priority_node );
    ensures queue_context->Priority.Actions.actions == \null;

  behavior inactive:
    assumes !priority_node_active{Pre}( priority_node );
    ensures priority_contributors{Post}(
              &the_thread->Scheduler.nodes->Wait.Priority ) ==
            priority_contributors{Pre}(
              &the_thread->Scheduler.nodes->Wait.Priority );

  complete behaviors active, inactive;
  disjoint behaviors active, inactive;
*/
#endif
static inline void _Scheduler_Cancel_job(
  Thread_Control       *the_thread,
  Priority_Node        *priority_node,
  Thread_queue_Context *queue_context
)
{
  const Scheduler_Control *scheduler = _Thread_Scheduler_get_home( the_thread );

  _Thread_queue_Context_clear_priority_updates( queue_context );
  /*@ assert scheduler == &_Scheduler_Table[ 0 ]; */
  /*@ assert (Scheduler_EDF_Context *) scheduler->context ==
        (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context; */
  /*@ assert edf_preemptible_heir_is_earliest_ready{Here}(
        (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
        _Thread_Heir,
        _Thread_Heir->is_preemptible ); */
  /*@ assert edf_preemptible_heir_is_earliest_ready{Here}(
        (Scheduler_EDF_Context *) scheduler->context,
        _Thread_Heir,
        _Thread_Heir->is_preemptible ); */
  /*@ calls _Scheduler_EDF_Cancel_job; */
  ( *scheduler->Operations.cancel_job )(
    scheduler,
    the_thread,
    priority_node,
    queue_context
  );
}

/**
 * @brief Starts the idle thread for a particular processor.
 *
 * @param scheduler The scheduler instance.
 * @param[in,out] the_thread The idle thread for the processor.
 * @param[in,out] cpu The processor for the idle thread.
 *
 * @see _Thread_Create_idle().
 */
static inline void _Scheduler_Start_idle(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Per_CPU_Control         *cpu
)
{
  ( *scheduler->Operations.start_idle )( scheduler, the_thread, cpu );
}

/**
 * @brief Checks if the scheduler of the cpu with the given index is equal
 *      to the given scheduler.
 *
 * @param scheduler The scheduler for the comparison.
 * @param cpu_index The index of the cpu for the comparison.
 *
 * @retval true The scheduler of the cpu is the given @a scheduler.
 * @retval false The scheduler of the cpu is not the given @a scheduler.
 */
static inline bool _Scheduler_Has_processor_ownership(
  const Scheduler_Control *scheduler,
  uint32_t                 cpu_index
)
{
#if defined(RTEMS_SMP)
  const Per_CPU_Control   *cpu;
  const Scheduler_Control *scheduler_of_cpu;

  cpu = _Per_CPU_Get_by_index( cpu_index );
  scheduler_of_cpu = _Scheduler_Get_by_CPU( cpu );

  return scheduler_of_cpu == scheduler;
#else
  (void) scheduler;
  (void) cpu_index;

  return true;
#endif
}

/**
 * @brief Gets the processors of the scheduler
 *
 * @param scheduler The scheduler to get the processors of.
 *
 * @return The processors of the context of the given scheduler.
 */
static inline const Processor_mask *_Scheduler_Get_processors(
  const Scheduler_Control *scheduler
)
{
#if defined(RTEMS_SMP)
  return &_Scheduler_Get_context( scheduler )->Processors;
#else
  return &_Processor_mask_The_one_and_only;
#endif
}

/**
 * @brief Copies the thread's scheduler's affinity to the given cpuset.
 *
 * @param the_thread The thread to get the affinity of its scheduler.
 * @param cpusetsize The size of @a cpuset.
 * @param[out] cpuset The cpuset that serves as destination for the copy operation
 *
 * @retval STATUS_SUCCESSFUL The operation succeeded.
 *
 * @retval STATUS_INVALID_SIZE The processor set was too small.
 */
Status_Control _Scheduler_Get_affinity(
  Thread_Control *the_thread,
  size_t          cpusetsize,
  cpu_set_t      *cpuset
);

/**
 * @brief Checks if the affinity is a subset of the online processors.
 *
 * @param scheduler This parameter is unused.
 * @param the_thread This parameter is unused.
 * @param node This parameter is unused.
 * @param affinity The processor mask to check.
 *
 * @retval STATUS_SUCCESSFUL The affinity is a subset of the online processors.
 *
 * @retval STATUS_INVALID_NUMBER The affinity is not a subset of the online
 *   processors.
 */
static inline Status_Control _Scheduler_default_Set_affinity_body(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Scheduler_Node          *node,
  const Processor_mask    *affinity
)
{
  (void) scheduler;
  (void) the_thread;
  (void) node;

  if ( !_Processor_mask_Is_subset( affinity, _SMP_Get_online_processors() ) ) {
    return STATUS_INVALID_NUMBER;
  }

  return STATUS_SUCCESSFUL;
}

/**
 * @brief Sets the thread's scheduler's affinity.
 *
 * @param[in, out] the_thread The thread to set the affinity of.
 * @param cpusetsize The size of @a cpuset.
 * @param cpuset The cpuset to set the affinity.
 *
 * @retval STATUS_SUCCESSFUL The operation succeeded.
 *
 * @retval STATUS_INVALID_NUMBER The processor set was not a valid new
 *   processor affinity set for the thread.
 */
Status_Control _Scheduler_Set_affinity(
  Thread_Control  *the_thread,
  size_t           cpusetsize,
  const cpu_set_t *cpuset
);

/**
 * @brief Gets the number of processors of the scheduler.
 *
 * @param scheduler The scheduler instance to get the number of processors of.
 *
 * @return The number of processors.
 */
static inline uint32_t _Scheduler_Get_processor_count(
  const Scheduler_Control *scheduler
)
{
#if defined(RTEMS_SMP)
  const Scheduler_Context *context = _Scheduler_Get_context( scheduler );

  return _Processor_mask_Count( &context->Processors );
#else
  (void) scheduler;

  return 1;
#endif
}

/**
 * @brief Builds an object build id.
 *
 * @param scheduler_index The index to build the build id out of.
 *
 * @return The build id.
 */
static inline Objects_Id _Scheduler_Build_id( uint32_t scheduler_index )
{
  return _Objects_Build_id(
    OBJECTS_FAKE_OBJECTS_API,
    OBJECTS_FAKE_OBJECTS_SCHEDULERS,
    _Objects_Local_node,
    (uint16_t) ( scheduler_index + 1 )
  );
}

/**
 * @brief Gets the scheduler index from the given object build id.
 *
 * @param id The object build id.
 *
 * @return The scheduler index.
 */
static inline uint32_t _Scheduler_Get_index_by_id( Objects_Id id )
{
  uint32_t minimum_id = _Scheduler_Build_id( 0 );

  return id - minimum_id;
}

/**
 * @brief Gets the scheduler from the given object build id.
 *
 * @param id The object build id.
 *
 * @return The scheduler to the object id.
 */
static inline const Scheduler_Control *_Scheduler_Get_by_id(
  Objects_Id id
)
{
  uint32_t index;

  index = _Scheduler_Get_index_by_id( id );

  if ( index >= _Scheduler_Count ) {
    return NULL;
  }

  return &_Scheduler_Table[ index ];
}

/**
 * @brief Gets the index of the scheduler
 *
 * @param scheduler The scheduler to get the index of.
 *
 * @return The index of the given scheduler.
 */
static inline uint32_t _Scheduler_Get_index(
  const Scheduler_Control *scheduler
)
{
  return (uint32_t) (scheduler - &_Scheduler_Table[ 0 ]);
}

#if defined(RTEMS_SMP)
/**
 * @brief Gets a scheduler node which is owned by an unused idle thread.
 *
 * @param arg is the handler argument.
 *
 * @return Returns a scheduler node owned by an idle thread for use.  This
 *   handler must always return a node.  If none is available, then this is a
 *   fatal error.
 */
typedef Scheduler_Node *( *Scheduler_Get_idle_node )( void *arg );

/**
 * @brief Releases the scheduler node which is owned by an idle thread.
 *
 * @param node is the node to release.
 *
 * @param arg is the handler argument.
 */
typedef void ( *Scheduler_Release_idle_node )(
  Scheduler_Node *node,
  void           *arg
);

/**
 * @brief Changes the threads state to the given new state.
 *
 * @param[out] the_thread The thread to change the state of.
 * @param new_state The new state for @a the_thread.
 */
static inline void _Scheduler_Thread_change_state(
  Thread_Control         *the_thread,
  Thread_Scheduler_state  new_state
)
{
  _Assert(
    _ISR_lock_Is_owner( &the_thread->Scheduler.Lock )
      || the_thread->Scheduler.state == THREAD_SCHEDULER_BLOCKED
      || !_System_state_Is_up( _System_state_Get() )
  );

  the_thread->Scheduler.state = new_state;
}

/**
 * @brief Uses an idle thread for the scheduler node.
 *
 * @param[in, out] node is the node which wants to use an idle thread.
 *
 * @param get_idle_node is the get idle node handler.
 *
 * @param arg is the handler argument.
 */
static inline Thread_Control *_Scheduler_Use_idle_thread(
  Scheduler_Node          *node,
  Scheduler_Get_idle_node  get_idle_node,
  void                    *arg
)
{
  Scheduler_Node *idle_node;
  Thread_Control *idle;

  idle_node = ( *get_idle_node )( arg );
  idle = _Scheduler_Node_get_owner( idle_node );
  _Assert( idle->is_idle );
  _Scheduler_Node_set_idle_user( node, idle );

  return idle;
}

/**
 * @brief Releases the idle thread used by the scheduler node.
 *
 * @param[in, out] node is the node which wants to release the idle thread.
 *
 * @param idle is the idle thread to release.
 *
 * @param release_idle_node is the release idle node handler.
 *
 * @param arg is the handler argument.
 */
static inline void _Scheduler_Release_idle_thread(
  Scheduler_Node             *node,
  const Thread_Control       *idle,
  Scheduler_Release_idle_node release_idle_node,
  void                       *arg
)
{
  Thread_Control *owner;
  Scheduler_Node *idle_node;

  owner = _Scheduler_Node_get_owner( node );
  _Assert( _Scheduler_Node_get_user( node ) == idle );
  _Scheduler_Node_set_user( node, owner );
  node->idle = NULL;
  idle_node = _Thread_Scheduler_get_home_node( idle );
  ( *release_idle_node )( idle_node, arg );
}

/**
 * @brief Releases the idle thread used by the scheduler node if the node uses
 *   an idle thread.
 *
 * @param[in, out] node is the node which wants to release the idle thread.
 *
 * @param release_idle_node is the release idle node handler.
 *
 * @param arg is the handler argument.
 *
 * @retval NULL The scheduler node did not use an idle thread.
 *
 * @return Returns the idle thread used by the scheduler node.
 */
static inline Thread_Control *_Scheduler_Release_idle_thread_if_necessary(
  Scheduler_Node             *node,
  Scheduler_Release_idle_node release_idle_node,
  void                        *arg
)
{
  Thread_Control *idle;

  idle = _Scheduler_Node_get_idle( node );

  if ( idle != NULL ) {
    _Scheduler_Release_idle_thread( node, idle, release_idle_node, arg );
  }

  return idle;
}

/**
 * @brief Discards the idle thread used by the scheduler node.
 *
 * @param[in, out] the_thread is the thread owning the node.
 *
 * @param[in, out] node is the node which wants to release the idle thread.
 *
 * @param release_idle_node is the release idle node handler.
 *
 * @param arg is the handler argument.
 */
static inline void _Scheduler_Discard_idle_thread(
  Thread_Control             *the_thread,
  Scheduler_Node             *node,
  Scheduler_Release_idle_node release_idle_node,
  void                       *arg
)
{
  Thread_Control  *idle;
  Per_CPU_Control *cpu;

  idle = _Scheduler_Node_get_idle( node );
  _Scheduler_Release_idle_thread( node, idle, release_idle_node, arg );

  cpu = _Thread_Get_CPU( idle );
  _Thread_Set_CPU( the_thread, cpu );
  _Thread_Dispatch_update_heir( _Per_CPU_Get(), cpu, the_thread );
}
#endif

/**
 * @brief Sets a new scheduler.
 *
 * @param new_scheduler The new scheduler to set.
 * @param[in, out] the_thread The thread for the operations.
 * @param priority The initial priority for the thread with the new scheduler.
 *
 * @retval STATUS_SUCCESSFUL The operation succeeded.
 * @retval STATUS_RESOURCE_IN_USE The thread's wait queue is not empty.
 * @retval STATUS_UNSATISFIED The new scheduler has no processors.
 */
static inline Status_Control _Scheduler_Set(
  const Scheduler_Control *new_scheduler,
  Thread_Control          *the_thread,
  Priority_Control         priority
)
{
  Scheduler_Node          *new_scheduler_node;
  Scheduler_Node          *old_scheduler_node;
#if defined(RTEMS_SMP)
  ISR_lock_Context         lock_context;
  const Scheduler_Control *old_scheduler;

#endif

#if defined(RTEMS_SCORE_THREAD_HAS_SCHEDULER_CHANGE_INHIBITORS)
  if ( the_thread->is_scheduler_change_inhibited ) {
    return STATUS_RESOURCE_IN_USE;
  }
#endif

  if ( the_thread->Wait.queue != NULL ) {
    return STATUS_RESOURCE_IN_USE;
  }

  old_scheduler_node = _Thread_Scheduler_get_home_node( the_thread );
  _Priority_Plain_extract(
    &old_scheduler_node->Wait.Priority,
    &the_thread->Real_priority
  );

  if (
    !_Priority_Is_empty( &old_scheduler_node->Wait.Priority )
#if defined(RTEMS_SMP)
      || !_Chain_Has_only_one_node( &the_thread->Scheduler.Wait_nodes )
      || the_thread->Scheduler.pin_level != 0
#endif
  ) {
    _Priority_Plain_insert(
      &old_scheduler_node->Wait.Priority,
      &the_thread->Real_priority,
      the_thread->Real_priority.priority
    );
    return STATUS_RESOURCE_IN_USE;
  }

#if defined(RTEMS_SMP)
  old_scheduler = _Thread_Scheduler_get_home( the_thread );
  new_scheduler_node = _Thread_Scheduler_get_node_by_index(
    the_thread,
    _Scheduler_Get_index( new_scheduler )
  );

  _Scheduler_Acquire_critical( new_scheduler, &lock_context );

  if (
    _Scheduler_Get_processor_count( new_scheduler ) == 0
      || ( *new_scheduler->Operations.set_affinity )(
        new_scheduler,
        the_thread,
        new_scheduler_node,
        &the_thread->Scheduler.Affinity
      ) != STATUS_SUCCESSFUL
  ) {
    _Scheduler_Release_critical( new_scheduler, &lock_context );
    _Priority_Plain_insert(
      &old_scheduler_node->Wait.Priority,
      &the_thread->Real_priority,
      the_thread->Real_priority.priority
    );
    return STATUS_UNSATISFIED;
  }

  _Assert( the_thread->Scheduler.pinned_scheduler == NULL );
  the_thread->Scheduler.home_scheduler = new_scheduler;

  _Scheduler_Release_critical( new_scheduler, &lock_context );

  _Thread_Scheduler_process_requests( the_thread );
#else
  new_scheduler_node = old_scheduler_node;
#endif

  the_thread->Start.initial_priority = priority;
  _Priority_Node_set_priority( &the_thread->Real_priority, priority );
  _Priority_Initialize_one(
    &new_scheduler_node->Wait.Priority,
    &the_thread->Real_priority
  );

#if defined(RTEMS_SMP)
  if ( old_scheduler != new_scheduler ) {
    States_Control current_state;

    current_state = the_thread->current_state;

    if ( _States_Is_ready( current_state ) ) {
      _Scheduler_Block( the_thread );
    }

    _Assert( old_scheduler_node->sticky_level == 0 );
    _Assert( new_scheduler_node->sticky_level == 0 );

    _Chain_Extract_unprotected( &old_scheduler_node->Thread.Wait_node );
    _Assert( _Chain_Is_empty( &the_thread->Scheduler.Wait_nodes ) );
    _Chain_Initialize_one(
      &the_thread->Scheduler.Wait_nodes,
      &new_scheduler_node->Thread.Wait_node
    );
    _Chain_Extract_unprotected(
      &old_scheduler_node->Thread.Scheduler_node.Chain
    );
    _Assert( _Chain_Is_empty( &the_thread->Scheduler.Scheduler_nodes ) );
    _Chain_Initialize_one(
      &the_thread->Scheduler.Scheduler_nodes,
      &new_scheduler_node->Thread.Scheduler_node.Chain
    );

    _Scheduler_Node_set_priority(
      new_scheduler_node,
      priority,
      PRIORITY_GROUP_LAST
    );

    if ( _States_Is_ready( current_state ) ) {
      _Scheduler_Unblock( the_thread );
    }

    return STATUS_SUCCESSFUL;
  }
#endif

  _Scheduler_Node_set_priority(
    new_scheduler_node,
    priority,
    PRIORITY_GROUP_LAST
  );
  _Scheduler_Update_priority( the_thread );
  return STATUS_SUCCESSFUL;
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif
/* end of include file */
