/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSScoreSchedulerEDF
 *
 * @brief This header file provides interfaces of the
 *   @ref RTEMSScoreSchedulerEDF which are only used by the implementation.
 *
 * VERIFICATION CHANGES from original RTEMS 6.2:
 * - ACSL contracts added to inline functions
 * - Ghost variable g_edf_sched_context and g_min_edf_node declared
 * - _Scheduler_EDF_Get_highest_ready: replaced _RBTree_Minimum +
 *   RTEMS_CONTAINER_OF with _Helper_RBTree_EDF_Minimum stub (lines 240-241)
 */

/*
 *  Copryight (c) 2011 Petr Benes.
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

#ifndef _RTEMS_SCORE_SCHEDULEREDFIMPL_H
#define _RTEMS_SCORE_SCHEDULEREDFIMPL_H

#include <rtems/score/scheduleredf.h>
#include <rtems/score/scheduleruniimpl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup RTEMSScoreSchedulerEDF
 *
 * @{
 */

#define SCHEDULER_EDF_PRIO_MSB 0x8000000000000000

/*@ ghost Scheduler_EDF_Context *g_edf_sched_context; */
/*@ ghost Scheduler_EDF_Node *g_min_edf_node; */

/*@
  requires \valid_read(scheduler);
  assigns \result \from scheduler, g_edf_sched_context;
  ensures \result == g_edf_sched_context;
*/
static inline Scheduler_EDF_Context *
  _Scheduler_EDF_Get_context( const Scheduler_Control *scheduler )
{
  return (Scheduler_EDF_Context *) _Scheduler_Get_context( scheduler );
}

static inline Scheduler_EDF_Node *_Scheduler_EDF_Thread_get_node(
  Thread_Control *the_thread
)
{
  return (Scheduler_EDF_Node *) _Thread_Scheduler_get_home_node( the_thread );
}

/*@
  requires \valid(node);
  assigns \result \from node;
  ensures \result == (Scheduler_EDF_Node *) node;
*/
static inline Scheduler_EDF_Node * _Scheduler_EDF_Node_downcast(
  Scheduler_Node *node
)
{
  return (Scheduler_EDF_Node *) node;
}

static inline bool _Scheduler_EDF_Less(
  const void        *left,
  const RBTree_Node *right
)
{
  const Priority_Control   *the_left;
  const Scheduler_EDF_Node *the_right;
  Priority_Control          prio_left;
  Priority_Control          prio_right;

  the_left = (const Priority_Control *) left;
  the_right = RTEMS_CONTAINER_OF( right, Scheduler_EDF_Node, Node );

  prio_left = *the_left;
  prio_right = the_right->priority;

  return prio_left < prio_right;
}

static inline bool _Scheduler_EDF_Priority_less_equal(
  const void        *left,
  const RBTree_Node *right
)
{
  const Priority_Control   *the_left;
  const Scheduler_EDF_Node *the_right;
  Priority_Control          prio_left;
  Priority_Control          prio_right;

  the_left = (const Priority_Control *) left;
  the_right = RTEMS_CONTAINER_OF( right, Scheduler_EDF_Node, Node );

  prio_left = *the_left;
  prio_right = the_right->priority;

  return prio_left <= prio_right;
}

/*@
  assigns \nothing;
*/
static inline void _Scheduler_EDF_Enqueue(
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

/*@
  assigns \nothing;
 */
static inline void _Scheduler_EDF_Extract(
  Scheduler_EDF_Context *context,
  Scheduler_EDF_Node    *node
)
{
  _RBTree_Extract( &context->Ready, &node->Node );
}

static inline void _Scheduler_EDF_Extract_body(
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

/*@
  requires \valid_read(scheduler) && \valid(g_min_edf_node) && \valid_read(g_edf_sched_context);
  requires \valid(g_min_edf_node->Base.owner);
  assigns \result \from scheduler, g_edf_sched_context, g_min_edf_node;
  ensures \result == g_min_edf_node->Base.owner;
*/
static inline Thread_Control *_Scheduler_EDF_Get_highest_ready(
  const Scheduler_Control *scheduler
)
{
  Scheduler_EDF_Context *context;
  Scheduler_EDF_Node    *node;

  context = _Scheduler_EDF_Get_context( scheduler );
  //@ assert context == g_edf_sched_context;
  /* VERIFICATION CHANGE: replaced _RBTree_Minimum + RTEMS_CONTAINER_OF */
  node = _Helper_RBTree_EDF_Minimum( &context->Ready );
  //@ assert node == g_min_edf_node;

  return node->Base.owner;
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif
/* end of include file */
