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

      // `is_preemptible` is passed explicitly instead of being dereferenced
      // inside the predicate body. WP's typed_cast model has trouble with
      // Thread_Control dereferences inside predicate bodies (Thread_Control
      // has flexible-array members, so the corresponding `\valid_read`
      // hypotheses get dropped by Frama-C, which then encodes the predicate
      // application as a zero-arity opaque atom and lets the call site
      // become inconsistent with other hypotheses). Validity of `heir` and
      // readability of its `is_preemptible` field are the caller's
      // responsibility.
      predicate edf_preemptible_heir_is_earliest_ready{L}(
        Scheduler_EDF_Context *context,
        Thread_Control        *heir,
        boolean                is_preemptible
      ) =
        !is_preemptible || edf_thread_is_earliest_ready{L}( context, heir );
    }
*/

#endif /* VERIFICATION_6_2_EDF_PROPERTY_H */
