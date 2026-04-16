/*
 * Frama-C/WP verification stubs for RTEMS 6.2 EDF scheduler.
 * Used with: scheduleredfreleasejob.c
 *
 * These stubs provide ACSL contracts for functions whose implementations
 * are either in other translation units or too complex for WP to handle.
 * Each signature matches the actual RTEMS 6.2 declaration exactly.
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
#include <rtems/score/priorityimpl.h>
#include <sys/tree.h>

/*@ ghost
  extern Priority_Node *g_min_priority_node;
  extern Scheduler_Node *g_scheduler_node_of_wait_priority_node;
  extern Scheduler_EDF_Node *g_min_edf_node;
  extern Scheduler_EDF_Context *g_edf_sched_context;
  extern bool g_new_minimum;
*/

/* Forward declarations for static functions defined in threadchangepriority.c.
 * These are not visible here but are referenced by the outer contracts. */
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
 * This stub returns the ghost variable instead.
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

/* threadqops.h:60 — extern, not static */
/*@
  requires \valid(priority_actions);
  assigns priority_actions->actions;
  ensures priority_actions->actions == NULL;
*/
void _Thread_queue_Do_nothing_priority_actions(
  Thread_queue_Queue *queue,
  Priority_Actions   *priority_actions
);

/* threadimpl.h:720 — extern */
/*@
  requires \valid(the_thread->Wait.queue);
  requires \valid(the_thread->Wait.operations);
  requires \valid(the_thread->Scheduler.nodes);
  requires \valid(the_thread);
  requires \valid(priority_node);
  requires \valid(queue_context);
  requires \valid(g_scheduler_node_of_wait_priority_node) && \valid(g_min_priority_node);
  requires \valid(queue_context->Priority.update[0]);
  requires queue_context->Priority.update_count == 0;
  requires the_thread->Wait.operations->priority_actions ==
    _Thread_queue_Do_nothing_priority_actions;

  requires \separated(
    queue_context,
    the_thread->Scheduler.nodes,
    the_thread->Wait.operations,
    the_thread->Wait.queue,
    the_thread
  );

  requires
    \separated(
      &the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
      &g_scheduler_node_of_wait_priority_node->Priority.value,
      &g_min_priority_node->priority,
      &priority_node->priority,
      &queue_context->Priority.update_count
    );

  ensures queue_context->Priority.Actions.actions == NULL;

  behavior priority_add_new_minimum:
    assumes g_new_minimum;
    assigns
      g_scheduler_node_of_wait_priority_node->Priority.value,
      the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
      queue_context->Priority.update[0],
      queue_context->Priority.update_count,
      queue_context->Priority.Actions.actions,
      the_thread->Scheduler.nodes->Wait.Priority.Action.type,
      the_thread->Scheduler.nodes->Wait.Priority.Action.node;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value == (\old(priority_node->priority) | 1);
    ensures \old(the_thread->Scheduler.nodes)->Wait.Priority.Node.priority == \old(priority_node->priority);
    ensures queue_context->Priority.update[0] == the_thread;
    ensures queue_context->Priority.update_count == 1;

  behavior priority_add_no_new_minimum:
    assumes !g_new_minimum;
    assigns
       queue_context->Priority.Actions.actions,
       the_thread->Scheduler.nodes->Wait.Priority.Action.type,
       the_thread->Scheduler.nodes->Wait.Priority.Action.node;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value == \old(g_scheduler_node_of_wait_priority_node->Priority.value);
*/
void _Thread_Priority_add(
  Thread_Control       *the_thread,
  Priority_Node        *priority_node,
  Thread_queue_Context *queue_context
);

/* threadimpl.h:766 — extern */
/*@
  requires \valid(the_thread->Wait.queue);
  requires \valid(the_thread->Wait.operations);
  requires \valid(the_thread->Scheduler.nodes);
  requires \valid(the_thread);
  requires \valid(priority_node);
  requires \valid(queue_context);
  requires \valid(g_scheduler_node_of_wait_priority_node) && \valid(g_min_priority_node);
  requires \valid(queue_context->Priority.update[0]);
  requires queue_context->Priority.update_count == 0;
  requires the_thread->Wait.operations->priority_actions ==
    _Thread_queue_Do_nothing_priority_actions;

  requires \separated(
    queue_context,
    the_thread->Scheduler.nodes,
    the_thread->Wait.operations,
    the_thread->Wait.queue,
    the_thread
  );

  requires
    \separated(
      &the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
      &g_scheduler_node_of_wait_priority_node->Priority.value,
      &g_min_priority_node->priority,
      &priority_node->priority,
      &queue_context->Priority.update_count
    );

  behavior priority_change_new_minimum:
    assumes the_thread->Scheduler.nodes->Wait.Priority.Node.priority != g_min_priority_node->priority;
    assigns
      g_scheduler_node_of_wait_priority_node->Priority.value,
      \old(the_thread->Scheduler.nodes)->Wait.Priority.Node.priority,
      queue_context->Priority.update[0],
      queue_context->Priority.update_count,
      queue_context->Priority.Actions.actions,
      the_thread->Scheduler.nodes->Wait.Priority.Action.type,
      the_thread->Scheduler.nodes->Wait.Priority.Action.node;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value == (g_min_priority_node->priority | (Priority_Control) priority_group_order);
    ensures \old(the_thread->Scheduler.nodes)->Wait.Priority.Node.priority == g_min_priority_node->priority;
    ensures queue_context->Priority.update[0] == the_thread;
    ensures queue_context->Priority.update_count == 1;

  behavior priority_change_no_new_minimum:
    assumes the_thread->Scheduler.nodes->Wait.Priority.Node.priority == g_min_priority_node->priority;
    assigns
       queue_context->Priority.Actions.actions,
       the_thread->Scheduler.nodes->Wait.Priority.Action.type,
       the_thread->Scheduler.nodes->Wait.Priority.Action.node;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value == \old(g_scheduler_node_of_wait_priority_node->Priority.value);

  complete behaviors;
  disjoint behaviors;
*/
void _Thread_Priority_changed(
  Thread_Control       *the_thread,
  Priority_Node        *priority_node,
  Priority_Group_order  priority_group_order,
  Thread_queue_Context *queue_context
);

/* threadimpl.h:741 — extern */
/*@
  requires \valid(the_thread->Wait.queue);
  requires \valid(the_thread->Wait.operations);
  requires \valid(the_thread->Scheduler.nodes);
  requires \valid(the_thread);
  requires \valid(priority_node);
  requires \valid(queue_context);
  requires \valid(g_scheduler_node_of_wait_priority_node) && \valid(g_min_priority_node);
  requires \valid(queue_context->Priority.update[0]);
  requires queue_context->Priority.update_count == 0;
  requires the_thread->Wait.operations->priority_actions ==
    _Thread_queue_Do_nothing_priority_actions;

  requires \separated(
    queue_context,
    the_thread->Scheduler.nodes,
    the_thread->Wait.operations,
    the_thread->Wait.queue,
    the_thread
  );

  requires
    \separated(
      &the_thread->Scheduler.nodes->Wait.Priority.Node.priority,
      &g_scheduler_node_of_wait_priority_node->Priority.value,
      &g_min_priority_node->priority,
      &priority_node->priority,
      &queue_context->Priority.update_count
    );

  ensures queue_context->Priority.Actions.actions == NULL;

  behavior priority_remove_new_minimum:
    assumes priority_node->priority < g_min_priority_node->priority;
    assigns
      g_scheduler_node_of_wait_priority_node->Priority.value,
      \old(the_thread->Scheduler.nodes)->Wait.Priority.Node.priority,
      queue_context->Priority.update[0],
      queue_context->Priority.update_count,
      queue_context->Priority.Actions.actions,
      the_thread->Scheduler.nodes->Wait.Priority.Action.type,
      the_thread->Scheduler.nodes->Wait.Priority.Action.node;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value == (g_min_priority_node->priority | 0);
    ensures \old(the_thread->Scheduler.nodes)->Wait.Priority.Node.priority == g_min_priority_node->priority;
    ensures queue_context->Priority.update[0] == the_thread;
    ensures queue_context->Priority.update_count == 1;

  behavior priority_remove_no_new_minimum:
    assumes !(priority_node->priority < g_min_priority_node->priority);
    assigns
      queue_context->Priority.Actions.actions,
      the_thread->Scheduler.nodes->Wait.Priority.Action.type,
      the_thread->Scheduler.nodes->Wait.Priority.Action.node;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value == \old(g_scheduler_node_of_wait_priority_node->Priority.value);

  complete behaviors;
  disjoint behaviors;
*/
void _Thread_Priority_remove(
  Thread_Control       *the_thread,
  Priority_Node        *priority_node,
  Thread_queue_Context *queue_context
);

/* threadimpl.h:1956 — static inline */
/*@
 assigns \nothing;
 */
static inline void _Thread_Wait_acquire_critical(
  Thread_Control       *the_thread,
  Thread_queue_Context *queue_context
);

/* threadimpl.h:2031 — static inline */
/*@
 assigns \nothing;
 */
static inline void _Thread_Wait_release_critical(
  Thread_Control       *the_thread,
  Thread_queue_Context *queue_context
);

/* rbtree.h:121 — static inline */
/*@
  requires \valid(the_node);
  assigns \nothing;
  ensures \result == (RB_COLOR(the_node, Node) == -1);
 */
static inline bool _RBTree_Is_node_off_tree(
  const RBTree_Node *the_node
);

/* rbtree.h:106 — static inline */
/*@
  requires \valid(the_node);
  assigns RB_COLOR( the_node, Node );
  ensures RB_COLOR( the_node, Node ) == -1;
*/
static inline void _RBTree_Set_off_tree( RBTree_Node *the_node );
