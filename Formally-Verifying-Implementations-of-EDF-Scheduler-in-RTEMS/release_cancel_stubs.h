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

/*@ ghost 
  extern Priority_Node *g_min_priority_node;
  extern Scheduler_Node *g_scheduler_node_of_wait_priority_node;
  extern Scheduler_EDF_Node *g_min_edf_node;
  extern Scheduler_EDF_Context *g_edf_sched_context;
  extern bool g_new_minimum;
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
    Thread_Control *the_thread,
    Priority_Node *priority_node,
    Thread_queue_Context *queue_context);

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
    assumes !prepend_it;
    assigns 
      g_scheduler_node_of_wait_priority_node->Priority.value, 
      \old(the_thread->Scheduler.nodes)->Wait.Priority.Node.priority,
      queue_context->Priority.update[0], 
      queue_context->Priority.update_count, 
      queue_context->Priority.Actions.actions,
      the_thread->Scheduler.nodes->Wait.Priority.Action.type, 
      the_thread->Scheduler.nodes->Wait.Priority.Action.node;
    ensures g_scheduler_node_of_wait_priority_node->Priority.value == (g_min_priority_node->priority | 1);
    ensures \old(the_thread->Scheduler.nodes)->Wait.Priority.Node.priority == g_min_priority_node->priority;
    ensures queue_context->Priority.update[0] == the_thread;
    ensures queue_context->Priority.update_count == 1;

  behavior priority_change_new_minimum_prepend:
    assumes the_thread->Scheduler.nodes->Wait.Priority.Node.priority != g_min_priority_node->priority;
    assumes prepend_it;
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
    Thread_Control *the_thread,
    Priority_Node *priority_node,
    bool prepend_it,
    Thread_queue_Context *queue_context);
    
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
    Thread_Control *the_thread,
    Priority_Node *priority_node,
    Thread_queue_Context *queue_context);
    
/*@
 assigns \nothing;
 */
RTEMS_INLINE_ROUTINE void _Thread_Wait_acquire_critical(
  Thread_Control       *the_thread,
  Thread_queue_Context *queue_context
);

/*@
 assigns \nothing;
 */
RTEMS_INLINE_ROUTINE void _Thread_Wait_release_critical(
  Thread_Control       *the_thread,
  Thread_queue_Context *queue_context
);

/*@
  requires \valid(the_node);
  assigns \nothing;
  ensures \result == (RB_COLOR(the_node, Node) == -1);
 */
RTEMS_INLINE_ROUTINE bool _RBTree_Is_node_off_tree(
  const RBTree_Node *the_node
);

/*@
  requires \valid(the_node);
  assigns RB_COLOR( the_node, Node );
  ensures RB_COLOR( the_node, Node ) == -1;
*/
RTEMS_INLINE_ROUTINE void _RBTree_Set_off_tree( RBTree_Node *the_node );
