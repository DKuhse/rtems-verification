/**
 * @file
 *
 * @ingroup RTEMSScoreScheduler
 *
 * @brief Inlined Routines Associated with the Manipulation of the Scheduler
 *
 * This inline file contains all of the inlined routines associated with
 * the manipulation of the scheduler.
 */

/*
 *  Copyright (C) 2010 Gedare Bloom.
 *  Copyright (C) 2011 On-Line Applications Research Corporation (OAR).
 *  Copyright (c) 2014, 2017 embedded brains GmbH
 *
 *  The license and distribution terms for this file may be
 *  found in the file LICENSE in this distribution or at
 *  http://www.rtems.org/license/LICENSE.
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
#include <rtems/score/scheduleredf.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup RTEMSScoreScheduler
 *
 * @{
 */

#ifdef __FRAMAC__
// Forward-declare EDF callback targets used by _Scheduler_Generic_block()
// `calls` annotations. Their contracts are supplied by scheduleredfimpl.h.
RTEMS_INLINE_ROUTINE void _Scheduler_EDF_Extract_body(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Scheduler_Node          *node
);

RTEMS_INLINE_ROUTINE void _Scheduler_EDF_Schedule_body(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  bool                     force_dispatch
);
#endif

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
RTEMS_INLINE_ROUTINE Scheduler_Context *_Scheduler_Get_context(
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
RTEMS_INLINE_ROUTINE const Scheduler_Control *_Scheduler_Get_by_CPU(
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
RTEMS_INLINE_ROUTINE void _Scheduler_Acquire_critical(
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
RTEMS_INLINE_ROUTINE void _Scheduler_Release_critical(
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
RTEMS_INLINE_ROUTINE bool _Scheduler_Is_non_preempt_mode_supported(
  const Scheduler_Control *scheduler
)
{
  return scheduler->is_non_preempt_mode_supported;
}
#endif

#if defined(RTEMS_SMP)
void _Scheduler_Request_ask_for_help( Thread_Control *the_thread );

/**
 * @brief Registers an ask for help request if necessary.
 *
 * The actual ask for help operation is carried out during
 * _Thread_Do_dispatch() on a processor related to the thread.  This yields a
 * better separation of scheduler instances.  A thread of one scheduler
 * instance should not be forced to carry out too much work for threads on
 * other scheduler instances.
 *
 * @param the_thread The thread in need for help.
 */
RTEMS_INLINE_ROUTINE void _Scheduler_Ask_for_help( Thread_Control *the_thread )
{
  _Assert( _Thread_State_is_owner( the_thread ) );

  if ( the_thread->Scheduler.helping_nodes > 0 ) {
    _Scheduler_Request_ask_for_help( the_thread );
  }
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
RTEMS_INLINE_ROUTINE void _Scheduler_Schedule( Thread_Control *the_thread )
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
RTEMS_INLINE_ROUTINE void _Scheduler_Yield( Thread_Control *the_thread )
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
RTEMS_INLINE_ROUTINE void _Scheduler_Block( Thread_Control *the_thread )
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
RTEMS_INLINE_ROUTINE void _Scheduler_Unblock( Thread_Control *the_thread )
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
RTEMS_INLINE_ROUTINE void _Scheduler_Update_priority( Thread_Control *the_thread )
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

  scheduler = _Thread_Scheduler_get_home( the_thread );
#ifdef __FRAMAC__
  // Frama-C breaks when a a function call is inside a @call annotated function pointer call
  // so we separately get the home node here and pass it as an argument to the function pointer call below.
  // This is just like the SMP case, so behavior preserving.
  Scheduler_Node          *scheduler_node;
  scheduler_node = _Thread_Scheduler_get_home_node( the_thread );
#endif
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
#ifdef __FRAMAC__
    scheduler_node
#else
    _Thread_Scheduler_get_home_node( the_thread )
#endif
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

#if defined(RTEMS_SMP)
/**
 * @brief Changes the sticky level of the home scheduler node and propagates a
 * priority change of a thread to the scheduler.
 *
 * @param the_thread The thread changing its priority or sticky level.
 *
 * @see _Scheduler_Update_priority().
 */
RTEMS_INLINE_ROUTINE void _Scheduler_Priority_and_sticky_update(
  Thread_Control *the_thread,
  int             sticky_level_change
)
{
  Chain_Node              *node;
  const Chain_Node        *tail;
  Scheduler_Node          *scheduler_node;
  const Scheduler_Control *scheduler;
  ISR_lock_Context         lock_context;

  _Thread_Scheduler_process_requests( the_thread );

  node = _Chain_First( &the_thread->Scheduler.Scheduler_nodes );
  scheduler_node = SCHEDULER_NODE_OF_THREAD_SCHEDULER_NODE( node );
  scheduler = _Scheduler_Node_get_scheduler( scheduler_node );

  _Scheduler_Acquire_critical( scheduler, &lock_context );

  scheduler_node->sticky_level += sticky_level_change;
  _Assert( scheduler_node->sticky_level >= 0 );

  ( *scheduler->Operations.update_priority )(
    scheduler,
    the_thread,
    scheduler_node
  );

  _Scheduler_Release_critical( scheduler, &lock_context );

  tail = _Chain_Immutable_tail( &the_thread->Scheduler.Scheduler_nodes );
  node = _Chain_Next( node );

  while ( node != tail ) {
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
#endif

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
RTEMS_INLINE_ROUTINE Priority_Control _Scheduler_Map_priority(
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
RTEMS_INLINE_ROUTINE Priority_Control _Scheduler_Unmap_priority(
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
RTEMS_INLINE_ROUTINE void _Scheduler_Node_initialize(
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
RTEMS_INLINE_ROUTINE void _Scheduler_Node_destroy(
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
  requires thread_priority_edf_node_valid{Pre}( the_thread );
  requires the_thread->current_state == STATES_READY ==>
    edf_ready_member{Pre}(
      (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
      (Scheduler_EDF_Node *) the_thread->Scheduler.nodes );
  requires the_thread->current_state == STATES_READY ==>
    SCHEDULER_PRIORITY_PURIFY( the_thread->Scheduler.nodes->Priority.value ) ==
      ((Scheduler_EDF_Node *)
        the_thread->Scheduler.nodes)->Base.Wait.Priority.Node.priority;
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
  ensures queue_context->Priority.update_count == 0 ==>
          queue_context->Priority.update[ 0 ] ==
            \at( queue_context->Priority.update[ 0 ], Pre );
  ensures queue_context->Priority.update_count == 0 ==>
          queue_context->Priority.update[ 0 ]->Scheduler.nodes ==
            \at( queue_context->Priority.update[ 0 ]->Scheduler.nodes, Pre );
  ensures queue_context->Priority.update_count == 1 ==>
          queue_context->Priority.update[ 0 ] == the_thread;
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
RTEMS_INLINE_ROUTINE void _Scheduler_Release_job(
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
  /*@ assert queue_context->Priority.update_count == 1 &&
        the_thread->current_state == STATES_READY ==>
          edf_ready_member{Here}(
            (Scheduler_EDF_Context *) scheduler->context,
            (Scheduler_EDF_Node *) the_thread->Scheduler.nodes ); */
  /*@ assert queue_context->Priority.update_count == 1 &&
        the_thread->current_state == STATES_READY ==>
          SCHEDULER_PRIORITY_PURIFY(
            the_thread->Scheduler.nodes->Priority.value ) ==
          ((Scheduler_EDF_Node *) the_thread->Scheduler.nodes)->
            Base.Wait.Priority.Node.priority; */
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
  requires thread_priority_edf_node_valid{Pre}( the_thread );
  requires the_thread->current_state == STATES_READY ==>
    edf_ready_member{Pre}(
      (Scheduler_EDF_Context *) _Scheduler_Table[ 0 ].context,
      (Scheduler_EDF_Node *) the_thread->Scheduler.nodes );
  requires the_thread->current_state == STATES_READY ==>
    SCHEDULER_PRIORITY_PURIFY( the_thread->Scheduler.nodes->Priority.value ) ==
      ((Scheduler_EDF_Node *)
        the_thread->Scheduler.nodes)->Base.Wait.Priority.Node.priority;
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
  ensures queue_context->Priority.update_count == 0 ==>
          queue_context->Priority.update[ 0 ] ==
            \at( queue_context->Priority.update[ 0 ], Pre );
  ensures queue_context->Priority.update_count == 0 ==>
          queue_context->Priority.update[ 0 ]->Scheduler.nodes ==
            \at( queue_context->Priority.update[ 0 ]->Scheduler.nodes, Pre );
  ensures queue_context->Priority.update_count == 1 ==>
          queue_context->Priority.update[ 0 ] == the_thread;
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
RTEMS_INLINE_ROUTINE void _Scheduler_Cancel_job(
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
 * @brief Scheduler method invoked at each clock tick.
 *
 * This method is invoked at each clock tick to allow the scheduler
 * implementation to perform any activities required.  For the
 * scheduler which support standard RTEMS features, this includes
 * time-slicing management.
 *
 * @param cpu The cpu control for the operation.
 */
RTEMS_INLINE_ROUTINE void _Scheduler_Tick( const Per_CPU_Control *cpu )
{
  const Scheduler_Control *scheduler = _Scheduler_Get_by_CPU( cpu );
  Thread_Control *executing = cpu->executing;

  if ( scheduler != NULL && executing != NULL ) {
    ( *scheduler->Operations.tick )( scheduler, executing );
  }
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
RTEMS_INLINE_ROUTINE void _Scheduler_Start_idle(
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
RTEMS_INLINE_ROUTINE bool _Scheduler_Has_processor_ownership(
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
RTEMS_INLINE_ROUTINE const Processor_mask *_Scheduler_Get_processors(
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
 * @retval true The copy operation was lossless.
 * @retval false The copy operation was not lossless
 */
bool _Scheduler_Get_affinity(
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
 * @retval true @a affinity is a subset of the online processors.
 * @retval false @a affinity is not a subset of the online processors.
 */
RTEMS_INLINE_ROUTINE bool _Scheduler_default_Set_affinity_body(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Scheduler_Node          *node,
  const Processor_mask    *affinity
)
{
  (void) scheduler;
  (void) the_thread;
  (void) node;
  return _Processor_mask_Is_subset( affinity, _SMP_Get_online_processors() );
}

/**
 * @brief Sets the thread's scheduler's affinity.
 *
 * @param[in, out] the_thread The thread to set the affinity of.
 * @param cpusetsize The size of @a cpuset.
 * @param cpuset The cpuset to set the affinity.
 *
 * @retval true The operation succeeded.
 * @retval false The operation did not succeed.
 */
bool _Scheduler_Set_affinity(
  Thread_Control  *the_thread,
  size_t           cpusetsize,
  const cpu_set_t *cpuset
);

/**
 * @brief Blocks the thread.
 *
 * @param scheduler The scheduler instance.
 * @param the_thread The thread to block.
 * @param node The corresponding scheduler node.
 * @param extract Method to extract the thread.
 * @param schedule Method for scheduling threads.
 */
RTEMS_INLINE_ROUTINE void _Scheduler_Generic_block(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Scheduler_Node          *node,
  void                  ( *extract )(
                             const Scheduler_Control *,
                             Thread_Control *,
                             Scheduler_Node *
                        ),
  void                  ( *schedule )(
                             const Scheduler_Control *,
                             Thread_Control *,
                             bool
                        )
)
{
#ifdef __FRAMAC__
Before_extract:
#endif
  /*@ calls _Scheduler_EDF_Extract_body; */
  ( *extract )( scheduler, the_thread, node );

  /* TODO: flash critical section? */

  if ( _Thread_Is_executing( the_thread ) || _Thread_Is_heir( the_thread ) ) {
    /*@ assert the_thread ==
          _Per_CPU_Information[ 0 ].per_cpu.executing ||
        the_thread == _Thread_Heir; */
    /*@ assert the_thread ==
          \at( _Per_CPU_Information[ 0 ].per_cpu.executing, Before_extract ) ||
        the_thread == \at( _Thread_Heir, Before_extract ); */
    /*@ assert \exists Scheduler_EDF_Node *other;
        other != (Scheduler_EDF_Node *) node &&
        other \in edf_ready_set{Before_extract}(
          (Scheduler_EDF_Context *) scheduler->context ); */
    /*@ assert edf_ready_set{Here}(
          (Scheduler_EDF_Context *) scheduler->context ) ==
        edf_ready_extract(
          edf_ready_set{Before_extract}(
            (Scheduler_EDF_Context *) scheduler->context ),
          (Scheduler_EDF_Node *) node ); */
    /*@ assert \exists Scheduler_EDF_Node *some;
        some \in edf_ready_extract(
          edf_ready_set{Before_extract}(
            (Scheduler_EDF_Context *) scheduler->context ),
          (Scheduler_EDF_Node *) node ); */
    /*@ assert \exists Scheduler_EDF_Node *some;
        some \in edf_ready_set{Here}(
          (Scheduler_EDF_Context *) scheduler->context ); */
    /*@ calls _Scheduler_EDF_Schedule_body; */
    ( *schedule )( scheduler, the_thread, true );
  }
}

/**
 * @brief Gets the number of processors of the scheduler.
 *
 * @param scheduler The scheduler instance to get the number of processors of.
 *
 * @return The number of processors.
 */
RTEMS_INLINE_ROUTINE uint32_t _Scheduler_Get_processor_count(
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
RTEMS_INLINE_ROUTINE Objects_Id _Scheduler_Build_id( uint32_t scheduler_index )
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
RTEMS_INLINE_ROUTINE uint32_t _Scheduler_Get_index_by_id( Objects_Id id )
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
RTEMS_INLINE_ROUTINE const Scheduler_Control *_Scheduler_Get_by_id(
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
RTEMS_INLINE_ROUTINE uint32_t _Scheduler_Get_index(
  const Scheduler_Control *scheduler
)
{
  return (uint32_t) (scheduler - &_Scheduler_Table[ 0 ]);
}

#if defined(RTEMS_SMP)
/**
 * @brief Gets an idle thread from the scheduler instance.
 *
 * @param context The scheduler instance context.
 *
 * @return idle An idle thread for use.  This function must always return an
 * idle thread.  If none is available, then this is a fatal error.
 */
typedef Thread_Control *( *Scheduler_Get_idle_thread )(
  Scheduler_Context *context
);

/**
 * @brief Releases an idle thread to the scheduler instance for reuse.
 *
 * @param context The scheduler instance context.
 * @param idle The idle thread to release.
 */
typedef void ( *Scheduler_Release_idle_thread )(
  Scheduler_Context *context,
  Thread_Control    *idle
);

/**
 * @brief Changes the threads state to the given new state.
 *
 * @param[out] the_thread The thread to change the state of.
 * @param new_state The new state for @a the_thread.
 */
RTEMS_INLINE_ROUTINE void _Scheduler_Thread_change_state(
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
 * @brief Sets the scheduler node's idle thread.
 *
 * @param[in, out] node The node to receive an idle thread.
 * @param idle The idle thread control for the operation.
 */
RTEMS_INLINE_ROUTINE void _Scheduler_Set_idle_thread(
  Scheduler_Node *node,
  Thread_Control *idle
)
{
  _Assert( _Scheduler_Node_get_idle( node ) == NULL );
  _Assert(
    _Scheduler_Node_get_owner( node ) == _Scheduler_Node_get_user( node )
  );

  _Scheduler_Node_set_user( node, idle );
  node->idle = idle;
}

/**
 * @brief Uses an idle thread for this scheduler node.
 *
 * A thread whose home scheduler node has a sticky level greater than zero may
 * use an idle thread in the home scheduler instance in the case it executes
 * currently in another scheduler instance or in the case it is in a blocking
 * state.
 *
 * @param context The scheduler instance context.
 * @param[in, out] node The node which wants to use the idle thread.
 * @param cpu The processor for the idle thread.
 * @param get_idle_thread Function to get an idle thread.
 */
RTEMS_INLINE_ROUTINE Thread_Control *_Scheduler_Use_idle_thread(
  Scheduler_Context         *context,
  Scheduler_Node            *node,
  Per_CPU_Control           *cpu,
  Scheduler_Get_idle_thread  get_idle_thread
)
{
  Thread_Control *idle = ( *get_idle_thread )( context );

  _Scheduler_Set_idle_thread( node, idle );
  _Thread_Set_CPU( idle, cpu );
  return idle;
}

typedef enum {
  SCHEDULER_TRY_TO_SCHEDULE_DO_SCHEDULE,
  SCHEDULER_TRY_TO_SCHEDULE_DO_IDLE_EXCHANGE,
  SCHEDULER_TRY_TO_SCHEDULE_DO_BLOCK
} Scheduler_Try_to_schedule_action;

/**
 * @brief Tries to schedule this scheduler node.
 *
 * @param context The scheduler instance context.
 * @param[in, out] node The node which wants to get scheduled.
 * @param idle A potential idle thread used by a potential victim node.
 * @param get_idle_thread Function to get an idle thread.
 *
 * @retval true This node can be scheduled.
 * @retval false This node cannot be scheduled.
 */
RTEMS_INLINE_ROUTINE Scheduler_Try_to_schedule_action
_Scheduler_Try_to_schedule_node(
  Scheduler_Context         *context,
  Scheduler_Node            *node,
  Thread_Control            *idle,
  Scheduler_Get_idle_thread  get_idle_thread
)
{
  ISR_lock_Context                  lock_context;
  Scheduler_Try_to_schedule_action  action;
  Thread_Control                   *owner;

  action = SCHEDULER_TRY_TO_SCHEDULE_DO_SCHEDULE;
  owner = _Scheduler_Node_get_owner( node );
  _Assert( _Scheduler_Node_get_user( node ) == owner );
  _Assert( _Scheduler_Node_get_idle( node ) == NULL );

  _Thread_Scheduler_acquire_critical( owner, &lock_context );

  if ( owner->Scheduler.state == THREAD_SCHEDULER_READY ) {
    _Thread_Scheduler_cancel_need_for_help( owner, _Thread_Get_CPU( owner ) );
    _Scheduler_Thread_change_state( owner, THREAD_SCHEDULER_SCHEDULED );
  } else if (
    owner->Scheduler.state == THREAD_SCHEDULER_SCHEDULED
      && node->sticky_level <= 1
  ) {
    action = SCHEDULER_TRY_TO_SCHEDULE_DO_BLOCK;
  } else if ( node->sticky_level == 0 ) {
    action = SCHEDULER_TRY_TO_SCHEDULE_DO_BLOCK;
  } else if ( idle != NULL ) {
    action = SCHEDULER_TRY_TO_SCHEDULE_DO_IDLE_EXCHANGE;
  } else {
    _Scheduler_Use_idle_thread(
      context,
      node,
      _Thread_Get_CPU( owner ),
      get_idle_thread
    );
  }

  _Thread_Scheduler_release_critical( owner, &lock_context );
  return action;
}

/**
 * @brief Releases an idle thread using this scheduler node.
 *
 * @param context The scheduler instance context.
 * @param[in, out] node The node which may have an idle thread as user.
 * @param release_idle_thread Function to release an idle thread.
 *
 * @retval idle The idle thread which used this node.
 * @retval NULL This node had no idle thread as an user.
 */
RTEMS_INLINE_ROUTINE Thread_Control *_Scheduler_Release_idle_thread(
  Scheduler_Context             *context,
  Scheduler_Node                *node,
  Scheduler_Release_idle_thread  release_idle_thread
)
{
  Thread_Control *idle = _Scheduler_Node_get_idle( node );

  if ( idle != NULL ) {
    Thread_Control *owner = _Scheduler_Node_get_owner( node );

    node->idle = NULL;
    _Scheduler_Node_set_user( node, owner );
    ( *release_idle_thread )( context, idle );
  }

  return idle;
}

/**
 * @brief Exchanges an idle thread from the scheduler node that uses it
 *      right now to another scheduler node.
 *
 * @param needs_idle The scheduler node that needs an idle thread.
 * @param uses_idle The scheduler node that used the idle thread.
 * @param idle The idle thread that is exchanged.
 */
RTEMS_INLINE_ROUTINE void _Scheduler_Exchange_idle_thread(
  Scheduler_Node *needs_idle,
  Scheduler_Node *uses_idle,
  Thread_Control *idle
)
{
  uses_idle->idle = NULL;
  _Scheduler_Node_set_user(
    uses_idle,
    _Scheduler_Node_get_owner( uses_idle )
  );
  _Scheduler_Set_idle_thread( needs_idle, idle );
}

/**
 * @brief Blocks this scheduler node.
 *
 * @param context The scheduler instance context.
 * @param[in, out] thread The thread which wants to get blocked referencing this
 *   node.  This is not necessarily the user of this node in case the node
 *   participates in the scheduler helping protocol.
 * @param[in, out] node The node which wants to get blocked.
 * @param is_scheduled This node is scheduled.
 * @param get_idle_thread Function to get an idle thread.
 *
 * @retval thread_cpu The processor of the thread.  Indicates to continue with
 *   the blocking operation.
 * @retval NULL Otherwise.
 */
RTEMS_INLINE_ROUTINE Per_CPU_Control *_Scheduler_Block_node(
  Scheduler_Context         *context,
  Thread_Control            *thread,
  Scheduler_Node            *node,
  bool                       is_scheduled,
  Scheduler_Get_idle_thread  get_idle_thread
)
{
  int               sticky_level;
  ISR_lock_Context  lock_context;
  Per_CPU_Control  *thread_cpu;

  sticky_level = node->sticky_level;
  --sticky_level;
  node->sticky_level = sticky_level;
  _Assert( sticky_level >= 0 );

  _Thread_Scheduler_acquire_critical( thread, &lock_context );
  thread_cpu = _Thread_Get_CPU( thread );
  _Thread_Scheduler_cancel_need_for_help( thread, thread_cpu );
  _Scheduler_Thread_change_state( thread, THREAD_SCHEDULER_BLOCKED );
  _Thread_Scheduler_release_critical( thread, &lock_context );

  if ( sticky_level > 0 ) {
    if ( is_scheduled && _Scheduler_Node_get_idle( node ) == NULL ) {
      Thread_Control *idle;

      idle = _Scheduler_Use_idle_thread(
        context,
        node,
        thread_cpu,
        get_idle_thread
      );
      _Thread_Dispatch_update_heir( _Per_CPU_Get(), thread_cpu, idle );
    }

    return NULL;
  }

  _Assert( thread == _Scheduler_Node_get_user( node ) );
  return thread_cpu;
}

/**
 * @brief Discard the idle thread from the scheduler node.
 *
 * @param context The scheduler context.
 * @param[in, out] the_thread The thread for the operation.
 * @param[in, out] node The scheduler node to discard the idle thread from.
 * @param release_idle_thread Method to release the idle thread from the context.
 */
RTEMS_INLINE_ROUTINE void _Scheduler_Discard_idle_thread(
  Scheduler_Context             *context,
  Thread_Control                *the_thread,
  Scheduler_Node                *node,
  Scheduler_Release_idle_thread  release_idle_thread
)
{
  Thread_Control  *idle;
  Thread_Control  *owner;
  Per_CPU_Control *cpu;

  idle = _Scheduler_Node_get_idle( node );
  owner = _Scheduler_Node_get_owner( node );

  node->idle = NULL;
  _Assert( _Scheduler_Node_get_user( node ) == idle );
  _Scheduler_Node_set_user( node, owner );
  ( *release_idle_thread )( context, idle );

  cpu = _Thread_Get_CPU( idle );
  _Thread_Set_CPU( the_thread, cpu );
  _Thread_Dispatch_update_heir( _Per_CPU_Get(), cpu, the_thread );
}

/**
 * @brief Unblocks this scheduler node.
 *
 * @param context The scheduler instance context.
 * @param[in, out] the_thread The thread which wants to get unblocked.
 * @param[in, out] node The node which wants to get unblocked.
 * @param is_scheduled This node is scheduled.
 * @param release_idle_thread Function to release an idle thread.
 *
 * @retval true Continue with the unblocking operation.
 * @retval false Do not continue with the unblocking operation.
 */
RTEMS_INLINE_ROUTINE bool _Scheduler_Unblock_node(
  Scheduler_Context             *context,
  Thread_Control                *the_thread,
  Scheduler_Node                *node,
  bool                           is_scheduled,
  Scheduler_Release_idle_thread  release_idle_thread
)
{
  bool unblock;

  ++node->sticky_level;
  _Assert( node->sticky_level > 0 );

  if ( is_scheduled ) {
    _Scheduler_Discard_idle_thread(
      context,
      the_thread,
      node,
      release_idle_thread
    );
    _Scheduler_Thread_change_state( the_thread, THREAD_SCHEDULER_SCHEDULED );
    unblock = false;
  } else {
    _Scheduler_Thread_change_state( the_thread, THREAD_SCHEDULER_READY );
    unblock = true;
  }

  return unblock;
}
#endif

/**
 * @brief Updates the heir.
 *
 * @param[in, out] new_heir The new heir.
 * @param force_dispatch Indicates whether the dispatch happens also if the
 *      currently running thread is set as not preemptible.
 */
/*@
  requires \valid_read( &_Thread_Heir->is_preemptible );
  requires \valid( &_Thread_Heir->cpu_time_used );
  requires \separated(
    _Thread_Heir + (..),
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..)
  );
  requires \separated(
    new_heir + (..),
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..)
  );
  requires \separated(
    &_Thread_Heir->cpu_time_used,
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..)
  );

  // Dispatch monotonicity: regardless of which behavior is taken, a Pre
  // dispatch-necessary remains dispatch-necessary at Post. Used by
  // Schedule_body and its EDF entry-point callers (Yield, Block) to keep
  // the P3.b dispatch-set invariant across the call without needing WP
  // to case-split on Update_heir's behaviors.
  ensures \at( _Thread_Dispatch_necessary_ghost, Pre ) ==>
          _Thread_Dispatch_necessary_ghost;

  behavior keep:
    assumes \at( _Thread_Heir, Pre ) == new_heir ||
            ( !\at( _Thread_Heir, Pre )->is_preemptible && !force_dispatch );
    assigns \nothing;
    ensures _Thread_Heir == \at( _Thread_Heir, Pre );
    // Explicit exact preservation of dispatch in the keep behavior, so
    // callers don't have to rely on `assigns \nothing` being case-split.
    ensures _Thread_Dispatch_necessary_ghost ==
              \at( _Thread_Dispatch_necessary_ghost, Pre );

  behavior update:
    assumes \at( _Thread_Heir, Pre ) != new_heir &&
            ( \at( _Thread_Heir, Pre )->is_preemptible || force_dispatch );
    assigns _Per_CPU_Information[ 0 ].per_cpu.heir \from new_heir;
    assigns _Per_CPU_Information[ 0 ].per_cpu.dispatch_necessary,
            _Thread_Dispatch_necessary_ghost;
    assigns \at( _Thread_Heir, Pre )->cpu_time_used,
            _Per_CPU_Information[ 0 ].per_cpu.cpu_usage_timestamp;
    ensures _Thread_Heir == new_heir;
    ensures _Thread_Dispatch_necessary_ghost == true;

  // Dispatch monotonicity: WP's frame analysis at call sites uses the union
  // of behavior assigns, not the keep-side `assigns \nothing`. A top-level
  // ensures lets callers carry dispatch state across the call.
  ensures \at( _Thread_Dispatch_necessary_ghost, Pre ) ==>
            _Thread_Dispatch_necessary_ghost;
  // executing is not assigned. State it explicitly so callers can carry
  // executing-side invariants across the call.
  ensures _Per_CPU_Information[ 0 ].per_cpu.executing ==
          \at( _Per_CPU_Information[ 0 ].per_cpu.executing, Pre );
  // Heir-unchanged disjunction: keep gives the left disjunct, update gives
  // the right (dispatch becomes true).
  ensures _Thread_Heir == \at( _Thread_Heir, Pre ) ||
          _Thread_Dispatch_necessary_ghost;

  complete behaviors;
  disjoint behaviors;
*/
RTEMS_INLINE_ROUTINE void _Scheduler_Update_heir(
  Thread_Control *new_heir,
  bool            force_dispatch
)
{
  Thread_Control *heir = _Thread_Heir;

  if ( heir != new_heir && ( heir->is_preemptible || force_dispatch ) ) {
#if defined(RTEMS_SMP)
    /*
     * We need this state only for _Thread_Get_CPU_time_used().  Cannot use
     * _Scheduler_Thread_change_state() since THREAD_SCHEDULER_BLOCKED to
     * THREAD_SCHEDULER_BLOCKED state changes are illegal for the real SMP
     * schedulers.
     */
    heir->Scheduler.state = THREAD_SCHEDULER_BLOCKED;
    new_heir->Scheduler.state = THREAD_SCHEDULER_SCHEDULED;
#endif
    _Thread_Update_CPU_time_used( heir, _Thread_Get_CPU( heir ) );
    _Thread_Heir = new_heir;
    _Thread_Dispatch_necessary = true;
  }
}

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
RTEMS_INLINE_ROUTINE Status_Control _Scheduler_Set(
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
      || !( *new_scheduler->Operations.set_affinity )(
        new_scheduler,
        the_thread,
        new_scheduler_node,
        &the_thread->Scheduler.Affinity
      )
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

    _Scheduler_Node_set_priority( new_scheduler_node, priority, false );

    if ( _States_Is_ready( current_state ) ) {
      _Scheduler_Unblock( the_thread );
    }

    return STATUS_SUCCESSFUL;
  }
#endif

  _Scheduler_Node_set_priority( new_scheduler_node, priority, false );
  _Scheduler_Update_priority( the_thread );
  return STATUS_SUCCESSFUL;
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif
/* end of include file */
