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

/*@ ghost 
  extern Priority_Node *g_min_priority_node;
  extern Scheduler_Node *g_scheduler_node_of_wait_priority_node;
  extern Scheduler_EDF_Node *g_min_edf_node;
  extern Scheduler_EDF_Context *g_edf_sched_context;
*/
static void _Thread_Priority_action_change(
  Priority_Aggregation *priority_aggregation,
  bool                  prepend_it,
  Priority_Actions     *priority_actions,
  void                 *arg
);

static void _Thread_Set_scheduler_node_priority(
    Priority_Aggregation *priority_aggregation,
    bool prepend_it);

/*@
  requires \valid(priority_aggregation);
  assigns \nothing;
  ensures \result == g_scheduler_node_of_wait_priority_node;
*/
Scheduler_Node *_Helper_SCHEDULER_NODE_OF_WAIT_PRIORITY_NODE( Priority_Aggregation *priority_aggregation);

/*@
  requires \valid(priority_actions);
  assigns priority_actions->actions;
  ensures priority_actions->actions == NULL;
*/
static void _Thread_queue_Do_nothing_priority_actions(
  Thread_queue_Queue *queue,
  Priority_Actions   *priority_actions
);

/*@
  requires \valid_read(tree);
  assigns \nothing;
  ensures \result == g_min_priority_node;
*/
Priority_Node *_Helper_RBTree_Minimum( const RBTree_Control *tree );

/*@
  requires \valid_read(tree);
  assigns \nothing;
  ensures \result == g_min_edf_node;
*/
Scheduler_EDF_Node *_Helper_RBTree_EDF_Minimum( const RBTree_Control *tree );

/*@
  requires \valid(queue_context);
  requires \valid(the_thread);
  requires \valid(queue_context->Priority.update[0]);
  requires queue_context->Priority.update_count  == 0;
  assigns queue_context->Priority.update_count;
  assigns queue_context->Priority.update[0];
  ensures queue_context->Priority.update[0] == the_thread;
  ensures queue_context->Priority.update_count == 1;
*/
RTEMS_INLINE_ROUTINE void _Thread_queue_Context_add_priority_update(
  Thread_queue_Context *queue_context,
  Thread_Control       *the_thread
);

/*@
  requires \valid_read(the_thread);
  assigns \nothing;
  ensures \result == the_thread->Scheduler.nodes;
 */
RTEMS_INLINE_ROUTINE Scheduler_Node *_Thread_Scheduler_get_home_node(
  const Thread_Control *the_thread
);

/*@
 requires \valid(node);
 assigns node->Priority.value;
 behavior prepend:
  assumes prepend_it;
  ensures node->Priority.value == (new_priority | 0);
 behavior no_prepend:
  assumes !prepend_it;
  ensures node->Priority.value == (new_priority | 1);
 disjoint behaviors;
 complete behaviors;  
 */
RTEMS_INLINE_ROUTINE void _Scheduler_Node_set_priority(
  Scheduler_Node   *node,
  Priority_Control  new_priority,
  bool              prepend_it
);

/*@
  requires \valid(node);
  assigns \nothing;
  ensures \result == node->Priority.value;
*/
RTEMS_INLINE_ROUTINE Priority_Control _Scheduler_Node_get_priority(
  Scheduler_Node *node
);

/*@
  assigns \nothing;
 */
RTEMS_INLINE_ROUTINE Per_CPU_Control *_Thread_Get_CPU(
  const Thread_Control *thread
);

/*@
 assigns \nothing;
 */
RTEMS_INLINE_ROUTINE void _Thread_Update_CPU_time_used(
  Thread_Control  *the_thread,
  Per_CPU_Control *cpu
);

/*@
  assigns \nothing;
  ensures \result == (the_states == STATES_READY);
*/
RTEMS_INLINE_ROUTINE bool _States_Is_ready (
  States_Control the_states
);

/*@
  requires \valid_read(the_thread);
  assigns \nothing;
  ensures \result == (the_thread->current_state == STATES_READY);
*/
RTEMS_INLINE_ROUTINE bool _Thread_Is_ready( 
  const Thread_Control *the_thread 
);

/*@
  requires \valid_read(the_thread) && \valid(the_thread->Scheduler.nodes);
  assigns \nothing;
  ensures \result == the_thread->Scheduler.nodes->Wait.Priority.Node.priority;
*/
RTEMS_INLINE_ROUTINE Priority_Control _Thread_Get_priority(
  const Thread_Control *the_thread
);
