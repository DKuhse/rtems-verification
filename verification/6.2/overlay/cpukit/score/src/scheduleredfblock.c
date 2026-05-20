/* SPDX-License-Identifier: BSD-2-Clause */

/**
 * @file
 *
 * @ingroup RTEMSScoreSchedulerEDF
 *
 * @brief This source file contains the implementation of
 *   _Scheduler_EDF_Block().
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

#ifdef __FRAMAC__
/*
 * FC 32 is strict about implicit function declarations. Forward-declare
 * timestamp helpers reachable through timestampimpl.h.
 */
#include <stdint.h>
#include <sys/time.h>
#include <time.h>

int64_t          tstosbt( struct timespec );
struct timespec  sbttots( int64_t );
struct timeval   sbttotv( int64_t );

#ifndef SBT_1S
#define SBT_1S ( (int64_t) 1 << 32 )
#endif
#endif

#include <rtems/score/scheduleredfimpl.h>
#include <rtems/score/schedulerimpl.h>
#include <rtems/score/thread.h>

/*@
  requires \valid_read( scheduler );
  requires \valid( the_thread );
  requires \valid( node );
  requires \valid( (Scheduler_EDF_Node *) node );
  requires &((Scheduler_EDF_Node *) node)->Base == node;
  requires \valid( (Scheduler_EDF_Context *) scheduler->context );
  requires edf_ready_context_well_formed{Pre}(
    (Scheduler_EDF_Context *) scheduler->context );
  requires edf_ready_context_cache_consistent{Pre}(
    (Scheduler_EDF_Context *) scheduler->context );

  // the_thread's node is in the ready set: required for Extract.
  requires edf_ready_member{Pre}(
    (Scheduler_EDF_Context *) scheduler->context,
    (Scheduler_EDF_Node *) node );
  // The node belongs to the_thread.
  requires ((Scheduler_EDF_Node *) node)->Base.owner == the_thread;

  requires \valid( _Thread_Heir );
  requires \valid_read( &_Thread_Heir->is_preemptible );
  requires \valid_read( &_Thread_Heir->Scheduler.nodes );
  requires \valid( _Thread_Heir->Scheduler.nodes );
  requires \valid_read( (Scheduler_EDF_Node *) _Thread_Heir->Scheduler.nodes );
  requires &((Scheduler_EDF_Node *) _Thread_Heir->Scheduler.nodes)->Base ==
    _Thread_Heir->Scheduler.nodes;
  requires \valid( &_Thread_Heir->cpu_time_used );
  requires \valid(
    &_Per_CPU_Information[ 0 ].per_cpu.cpu_usage_timestamp );

  requires edf_preemptible_heir_is_earliest_ready{Pre}(
    (Scheduler_EDF_Context *) scheduler->context,
    _Thread_Heir,
    _Thread_Heir->is_preemptible );
  requires edf_dispatch_set_if_heir_differs(
    _Per_CPU_Information[ 0 ].per_cpu.executing,
    _Thread_Heir,
    _Thread_Dispatch_necessary_ghost );

  // In the heir branch, after extracting the_thread's node the ready set
  // must still be non-empty so Get_highest_ready can pick a new heir.
  requires the_thread == _Thread_Heir ==>
    \exists Scheduler_EDF_Node *other;
      other != (Scheduler_EDF_Node *) node &&
      other \in edf_ready_set{Pre}(
        (Scheduler_EDF_Context *) scheduler->context );

  requires \separated(
    (Scheduler_EDF_Node *) node + (..),
    (Scheduler_EDF_Context *) scheduler->context + (..)
  );
  // Heir's home node must be separate from the tree root so its
  // `Base.owner` survives Extract's `assigns context->Ready` framing.
  requires \separated(
    (Scheduler_EDF_Node *) _Thread_Heir->Scheduler.nodes + (..),
    (Scheduler_EDF_Node *) node + (..),
    (Scheduler_EDF_Context *) scheduler->context + (..)
  );
  requires \separated(
    node + (..),
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..)
  );
  requires \separated(
    the_thread + (..),
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..)
  );
  requires \separated(
    scheduler + (..),
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..)
  );
  requires \separated(
    _Thread_Heir + (..),
    (Per_CPU_Control_envelope *) _Per_CPU_Information + (..),
    scheduler + (..),
    (Scheduler_EDF_Context *) scheduler->context + (..)
  );

  assigns ((Scheduler_EDF_Context *) scheduler->context)->Ready,
          _Per_CPU_Information[ 0 ].per_cpu.heir,
          _Per_CPU_Information[ 0 ].per_cpu.dispatch_necessary,
          _Thread_Dispatch_necessary_ghost,
          _Thread_Heir->cpu_time_used,
          _Per_CPU_Information[ 0 ].per_cpu.heir->cpu_time_used,
          _Per_CPU_Information[ 0 ].per_cpu.cpu_usage_timestamp;

  // the_thread's node is removed from the ready set.
  ensures edf_ready_set{Post}(
            (Scheduler_EDF_Context *) scheduler->context ) ==
          edf_ready_extract(
            edf_ready_set{Pre}(
              (Scheduler_EDF_Context *) scheduler->context ),
            (Scheduler_EDF_Node *) node );

  // EDF property at exit.
  ensures edf_preemptible_heir_is_earliest_ready{Post}(
    (Scheduler_EDF_Context *) scheduler->context,
    _Thread_Heir,
    _Thread_Heir->is_preemptible );
  ensures edf_dispatch_set_if_heir_differs(
    _Per_CPU_Information[ 0 ].per_cpu.executing,
    _Thread_Heir,
    _Thread_Dispatch_necessary_ghost );

  // Inductive invariant: the ready context remains well-formed at every
  // EDF API boundary.
  ensures edf_ready_context_well_formed{Post}(
    (Scheduler_EDF_Context *) scheduler->context );
  ensures edf_ready_context_cache_consistent{Post}(
    (Scheduler_EDF_Context *) scheduler->context );

  behavior not_heir:
    assumes the_thread != \at( _Thread_Heir, Pre );
    ensures _Thread_Heir == \at( _Thread_Heir, Pre );

  behavior is_heir:
    assumes the_thread == \at( _Thread_Heir, Pre );
    // _Thread_Heir is replaced with the owner of the post-state earliest
    // ready node. The exact identity is captured by the global EDF-property
    // post-condition above (witness-explicit form pinned in the body).

  complete behaviors;
  disjoint behaviors;
*/
void _Scheduler_EDF_Block(
  const Scheduler_Control *scheduler,
  Thread_Control          *the_thread,
  Scheduler_Node          *node
)
{
  // Recover the concrete home-node witness from the existential EDF
  // property and the ready-set canonical-owner invariant.
  /*@ assert _Thread_Heir->is_preemptible ==>
        edf_thread_node_is_earliest_ready{Here}(
          (Scheduler_EDF_Context *) scheduler->context,
          _Thread_Heir,
          (Scheduler_EDF_Node *) _Thread_Heir->Scheduler.nodes
        ); */

  _Scheduler_uniprocessor_Block(
    scheduler,
    the_thread,
    node,
    _Scheduler_EDF_Extract_body,
    _Scheduler_EDF_Get_highest_ready
  );

  /*@ assert edf_dispatch_set_if_heir_differs(
        _Per_CPU_Information[ 0 ].per_cpu.executing,
        _Thread_Heir,
        _Thread_Dispatch_necessary_ghost ); */

  // Case A: not_heir, preemptible. Recover from canonical-owner invariant. 
  // The extract preservation lemma gives that the heir's home node is still earliest in the post-state.
  /*@ assert the_thread != \at( _Thread_Heir, Pre ) &&
        _Thread_Heir->is_preemptible ==>
        edf_thread_node_is_earliest_ready{Here}(
          (Scheduler_EDF_Context *) scheduler->context,
          _Thread_Heir,
          (Scheduler_EDF_Node *) _Thread_Heir->Scheduler.nodes
        ); */

  // Case B: is_heir. _Thread_Heir is now highest_ready, the owner of the
  // post-state earliest ready node. Get_highest_ready's contract gives the
  // witness directly.
  /*@ assert the_thread == \at( _Thread_Heir, Pre ) ==>
        edf_thread_node_is_earliest_ready{Here}(
          (Scheduler_EDF_Context *) scheduler->context,
          _Thread_Heir,
          (Scheduler_EDF_Node *) _Thread_Heir->Scheduler.nodes
        ); */
}
