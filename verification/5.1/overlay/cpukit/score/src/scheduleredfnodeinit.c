/**
 *  @file
 *
 *  @brief Scheduler EDF Allocate
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

/*@
  requires \valid_read( scheduler );
  requires \valid( node );
  requires \valid( (Scheduler_EDF_Node *) node );
  requires &((Scheduler_EDF_Node *) node)->Base == node;
  requires \valid( &((Scheduler_EDF_Node *) node)->Node );

  assigns ((Scheduler_EDF_Node *) node)->Base.owner,
          ((Scheduler_EDF_Node *) node)->Base.Priority.value,
          ((Scheduler_EDF_Node *) node)->priority;

  ensures ((Scheduler_EDF_Node *) node)->Base.owner == the_thread;
  ensures ((Scheduler_EDF_Node *) node)->Base.Priority.value == priority;
  ensures ((Scheduler_EDF_Node *) node)->priority == priority;
*/
void _Scheduler_EDF_Node_initialize(
  const Scheduler_Control *scheduler,
  Scheduler_Node          *node,
  Thread_Control          *the_thread,
  Priority_Control         priority
)
{
  Scheduler_EDF_Node *the_node;

  _Scheduler_Node_do_initialize( scheduler, node, the_thread, priority );

  the_node = _Scheduler_EDF_Node_downcast( node );
  _RBTree_Initialize_node( &the_node->Node );
  the_node->priority = priority;
}
