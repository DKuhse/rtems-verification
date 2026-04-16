/*
 * Frama-C/WP verification stubs for RTEMS 6.2 EDF scheduler.
 * Used with: threadchangepriority.c, scheduleredfchangepriority.c,
 *            scheduleredfunblock.c
 *
 * Each signature matches the actual RTEMS 6.2 declaration.
 * See verification/6.2/HEADERS.md for documentation.
 */

#include <rtems/score/thread.h>
#include <rtems/score/threadq.h>
#include <rtems/score/priority.h>
#include <rtems/score/watchdog.h>
#include <rtems/score/interr.h>
#include <rtems/score/basedefs.h>
#include <rtems/score/percpu.h>
#include <rtems/score/status.h>
#include <rtems/score/scheduleredf.h>
#include <sys/tree.h>
#include <rtems/score/statesimpl.h>

/*
 * Include priorityimpl.h first — it defines Priority_Group_order and
 * the contracted priority functions. The //@ calls annotations inside
 * it reference _Thread_Priority_action_change, which is forward-declared
 * in the ghost block below (ghost declarations are parsed after all
 * includes).
 */
#include <rtems/score/priorityimpl.h>

/*@ ghost
  extern Priority_Node *g_min_priority_node;
  extern Scheduler_Node *g_scheduler_node_of_wait_priority_node;
  extern Scheduler_EDF_Node *g_min_edf_node;
  extern Scheduler_EDF_Context *g_edf_sched_context;
*/

/* Forward declarations for static functions in threadchangepriority.c */
static void _Thread_Priority_action_change(
  Priority_Aggregation *priority_aggregation,
  Priority_Group_order  priority_group_order,
  Priority_Actions     *priority_actions,
  void                 *arg
);

static void _Thread_Set_scheduler_node_priority(
  Priority_Aggregation *priority_aggregation,
  Priority_Group_order  priority_group_order
);

/*
 * Stub for SCHEDULER_NODE_OF_WAIT_PRIORITY_NODE macro.
 * The macro uses RTEMS_CONTAINER_OF which Frama-C cannot reason about.
 *
 * VERIFICATION CHANGE: calls to SCHEDULER_NODE_OF_WAIT_PRIORITY_NODE
 * in threadchangepriority.c are replaced with calls to this function.
 */
/*@
  requires \valid(priority_aggregation);
  assigns \result \from priority_aggregation, g_scheduler_node_of_wait_priority_node;
  ensures \result == g_scheduler_node_of_wait_priority_node;
*/
Scheduler_Node *_Helper_SCHEDULER_NODE_OF_WAIT_PRIORITY_NODE(
  Priority_Aggregation *priority_aggregation
);

/* threadqops.h:60 — extern */
/*@
  requires \valid(priority_actions);
  assigns priority_actions->actions;
  ensures priority_actions->actions == NULL;
*/
void _Thread_queue_Do_nothing_priority_actions(
  Thread_queue_Queue *queue,
  Priority_Actions   *priority_actions
);

/* stubs.h:48 — stub for RBTree minimum (ghost variable) */
/*@
  requires \valid_read(tree);
  assigns \result \from tree, g_min_priority_node;
  ensures \result == g_min_priority_node;
*/
Priority_Node *_Helper_RBTree_Minimum( const RBTree_Control *tree );

/* stubs.h:55 — stub for EDF RBTree minimum (ghost variable) */
/*@
  requires \valid_read(tree);
  assigns \result \from tree, g_min_edf_node;
  ensures \result == g_min_edf_node;
*/
Scheduler_EDF_Node *_Helper_RBTree_EDF_Minimum( const RBTree_Control *tree );

/* threadqimpl.h:418 — static inline */
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

/* threadimpl.h:1582 — static inline */
/*@
  requires \valid_read(the_thread) && \valid(the_thread->Scheduler.nodes);
  assigns \result \from the_thread;
  ensures \result == the_thread->Scheduler.nodes;
 */
static inline Scheduler_Node *_Thread_Scheduler_get_home_node(
  const Thread_Control *the_thread
);

/* schedulernodeimpl.h:217 — static inline */
/*@
 requires \valid(node);
 requires group_order == PRIORITY_GROUP_FIRST || group_order == PRIORITY_GROUP_LAST;
 assigns node->Priority.value;
 behavior group_first:
  assumes group_order == PRIORITY_GROUP_FIRST;
  ensures node->Priority.value == (new_priority | 0);
 behavior group_last:
  assumes group_order == PRIORITY_GROUP_LAST;
  ensures node->Priority.value == (new_priority | 1);
 disjoint behaviors;
 complete behaviors;
 */
static inline void _Scheduler_Node_set_priority(
  Scheduler_Node      *node,
  Priority_Control     new_priority,
  Priority_Group_order group_order
);

/* schedulernodeimpl.h:184 — static inline */
/*@
  requires \valid(node);
  assigns \nothing;
  ensures \result == node->Priority.value;
*/
static inline Priority_Control _Scheduler_Node_get_priority(
  Scheduler_Node *node
);

/* threadimpl.h:996 — static inline */
/*@
  assigns \result \from thread;
 */
static inline Per_CPU_Control *_Thread_Get_CPU(
  const Thread_Control *thread
);

/* threadimpl.h:1269 — static inline */
/*@
 assigns \nothing;
 */
static inline void _Thread_Update_CPU_time_used(
  Thread_Control  *the_thread,
  Per_CPU_Control *cpu
);

/* statesimpl.h:212 — static inline */
/*@
  assigns \nothing;
  ensures \result == (the_states == STATES_READY);
*/
static inline bool _States_Is_ready(
  States_Control the_states
);

/* threadimpl.h:471 — static inline */
/*@
  requires \valid_read(the_thread);
  assigns \nothing;
  ensures \result == (the_thread->current_state == STATES_READY);
*/
static inline bool _Thread_Is_ready(
  const Thread_Control *the_thread
);

/* threadimpl.h:1754 — static inline */
/*@
  requires \valid_read(the_thread) && \valid(the_thread->Scheduler.nodes);
  assigns \nothing;
  ensures \result == the_thread->Scheduler.nodes->Wait.Priority.Node.priority;
*/
static inline Priority_Control _Thread_Get_priority(
  const Thread_Control *the_thread
);
