/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSScoreScheduler
 *
 * @brief This header file provides interfaces of the supporting the
 *   implementation of uniprocessor schedulers.
 *
 * VERIFICATION CHANGES from original RTEMS 6.2:
 * - ACSL contract added to _Scheduler_uniprocessor_Update_heir
 *   (same pattern as 5.1's _Scheduler_Update_heir: contract provides
 *   \from clauses while the function body is still inlined)
 * - ACSL contract added to _Scheduler_uniprocessor_Update_heir_if_preemptible
 */

/*
 *  Copyright (C) 2010 Gedare Bloom.
 *  Copyright (C) 2011 On-Line Applications Research Corporation (OAR).
 *  Copyright (C) 2014, 2022 embedded brains GmbH & Co. KG
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

#ifndef _RTEMS_SCORE_SCHEDULERUNIIMPL_H
#define _RTEMS_SCORE_SCHEDULERUNIIMPL_H

#include <rtems/score/schedulerimpl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @addtogroup RTEMSScoreScheduler
 *
 * @{
 */

/*@
  requires \valid(heir) && \valid(new_heir) && \valid(_Per_CPU_Get());
  requires heir != new_heir;
  assigns _Thread_Heir \from new_heir;
  assigns _Thread_Dispatch_necessary \from \nothing;
  ensures _Thread_Heir == new_heir;
  //ensures _Thread_Dispatch_necessary == true; // unprovable: dispatch_necessary is volatile bool
*/
static inline void _Scheduler_uniprocessor_Update_heir(
  Thread_Control *heir,
  Thread_Control *new_heir
)
{
  _Assert( heir != new_heir );
#if defined(RTEMS_SMP)
  heir->Scheduler.state = THREAD_SCHEDULER_BLOCKED;
  new_heir->Scheduler.state = THREAD_SCHEDULER_SCHEDULED;
#endif
  _Thread_Update_CPU_time_used( heir, _Thread_Get_CPU( heir ) );
  _Thread_Heir = new_heir;
  _Thread_Dispatch_necessary = true;
}

static inline void _Scheduler_uniprocessor_Update_heir_if_necessary(
  Thread_Control *new_heir
)
{
  Thread_Control *heir = _Thread_Heir;

  if ( heir != new_heir ) {
    _Scheduler_uniprocessor_Update_heir( heir, new_heir );
  }
}

/*@
  requires \valid(heir) && \valid(new_heir) && \valid(_Per_CPU_Get());
  behavior new_h:
    assumes heir != new_heir && heir->is_preemptible;
    assigns _Thread_Heir \from new_heir;
    assigns _Thread_Dispatch_necessary \from \nothing;
    ensures _Thread_Heir == new_heir;
  behavior no_new_h:
    assumes !(heir != new_heir && heir->is_preemptible);
    assigns \nothing;
  complete behaviors;
  disjoint behaviors;
*/
static inline void _Scheduler_uniprocessor_Update_heir_if_preemptible(
  Thread_Control *heir,
  Thread_Control *new_heir
)
{
  if ( heir != new_heir && heir->is_preemptible ) {
    _Scheduler_uniprocessor_Update_heir( heir, new_heir );
  }
}

static inline void _Scheduler_uniprocessor_Block(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Scheduler_Node          *node,
  void                  ( *extract )(
                             const Scheduler_Control *,
                             Thread_Control *,
                             Scheduler_Node *
                        ),
  Thread_Control       *( *get_highest_ready )( const Scheduler_Control * )
)
{
  ( *extract )( scheduler, the_thread, node );

  if ( _Thread_Is_heir( the_thread ) ) {
    Thread_Control *highest_ready;

    highest_ready = ( *get_highest_ready )( scheduler );
    _Scheduler_uniprocessor_Update_heir( _Thread_Heir, highest_ready );
  }
}

static inline void _Scheduler_uniprocessor_Unblock(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Priority_Control         priority
)
{
  Thread_Control *heir;

  heir = _Thread_Heir;

  if ( priority < _Thread_Get_priority( heir ) ) {
    _Scheduler_uniprocessor_Update_heir_if_preemptible( heir, the_thread );
  }
}

static inline void _Scheduler_uniprocessor_Schedule(
  const Scheduler_Control *scheduler,
  Thread_Control       *( *get_highest_ready )( const Scheduler_Control * )
)
{
  Thread_Control *highest_ready;

  highest_ready = ( *get_highest_ready )( scheduler );
  _Scheduler_uniprocessor_Update_heir_if_preemptible(
    _Thread_Heir,
    highest_ready
  );
}

static inline void _Scheduler_uniprocessor_Yield(
  const Scheduler_Control *scheduler,
  Thread_Control       *( *get_highest_ready )( const Scheduler_Control * )
)
{
  Thread_Control *highest_ready;

  highest_ready = ( *get_highest_ready )( scheduler );
  _Scheduler_uniprocessor_Update_heir_if_necessary( highest_ready );
}

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* _RTEMS_SCORE_SCHEDULERUNIIMPL_H */
