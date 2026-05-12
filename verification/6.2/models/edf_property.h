/*
 * Abstract EDF scheduler property model. Verification only header.
 */

#ifndef VERIFICATION_6_2_EDF_PROPERTY_H
#define VERIFICATION_6_2_EDF_PROPERTY_H

#include <edf_ready_set.h>

/*@ axiomatic EDFProperty {
      predicate edf_ready_node_not_after{L}(
        Scheduler_EDF_Node *left,
        Scheduler_EDF_Node *right
      ) =
        \valid_read( left ) &&
        \valid_read( right ) &&
        left->priority <= right->priority;

      predicate edf_ready_earliest_node{L}(
        set<Scheduler_EDF_Node *> nodes,
        Scheduler_EDF_Node       *node
      ) =
        edf_ready_valid_nodes{L}( nodes ) &&
        node \in nodes &&
        \forall Scheduler_EDF_Node *other;
          other \in nodes ==> edf_ready_node_not_after{L}( node, other );

      predicate edf_thread_owns_earliest_ready_node{L}(
        set<Scheduler_EDF_Node *> nodes,
        Thread_Control           *thread
      ) =
        \exists Scheduler_EDF_Node *node;
          edf_ready_earliest_node{L}( nodes, node ) &&
          node->Base.owner == thread;

      predicate edf_thread_is_earliest_ready{L}(
        Scheduler_EDF_Context *context,
        Thread_Control        *thread
      ) =
        edf_thread_owns_earliest_ready_node{L}(
          edf_ready_set{L}( context ),
          thread
        );

      // EDF PROPERTY
      // `is_preemptible` is passed explicitly instead of being dereferenced
      // inside the predicate body. WP's typed_cast model has trouble with
      // Thread_Control dereferences inside predicate bodies
      predicate edf_preemptible_heir_is_earliest_ready{L}(
        Scheduler_EDF_Context *context,
        Thread_Control        *heir,
        boolean                is_preemptible
      ) =
        !is_preemptible || edf_thread_is_earliest_ready{L}( context, heir );

      // EDF PROPERTY WITH EXPLICIT WITNESS
      // Prover sometimes has issues with exists quantified version... concrete witness version:

            predicate edf_node_represents_thread{L}(
        Scheduler_EDF_Node *node,
        Thread_Control  *thread
      ) =
        \valid_read( node ) &&
        node->Base.owner == thread;

      predicate edf_thread_node_is_earliest_ready{L}(
        Scheduler_EDF_Context *context,
        Thread_Control        *thread,
        Scheduler_EDF_Node    *node
      ) =
        edf_node_represents_thread{L}( node, thread ) &&
        edf_ready_earliest_node{L}(
          edf_ready_set{L}( context ),
          node
        );

      predicate edf_preemptible_heir_node_is_earliest_ready{L}(
        Scheduler_EDF_Context *context,
        Thread_Control        *heir,
        Scheduler_EDF_Node    *node,
        boolean                is_preemptible
      ) =
        !is_preemptible ||
        edf_thread_node_is_earliest_ready{L}(
          context,
          heir,
          node
        );



      lemma edf_scheduler_node_earliest_implies_thread_earliest{L}:
        \forall Scheduler_EDF_Context *context;
        \forall Thread_Control *thread;
        \forall Scheduler_EDF_Node *node;
          edf_thread_node_is_earliest_ready{L}(
            context,
            thread,
            node
          ) ==>
          edf_thread_is_earliest_ready{L}( context, thread );
    }
*/

#endif /* VERIFICATION_6_2_EDF_PROPERTY_H */
