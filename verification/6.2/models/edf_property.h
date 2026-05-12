/*
 * Abstract EDF scheduler property model. Verification only header.
 */

#ifndef VERIFICATION_6_2_EDF_PROPERTY_H
#define VERIFICATION_6_2_EDF_PROPERTY_H

#include <edf_ready_set.h>

/*@ axiomatic EDFProperty {

      // --- Base ordering predicates (set-level) -------------------------

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

      // --- EDF Witness-explicit form -------------------------------------
      // Used inside the EDF operation proofs (Unblock, Block, ...) where
      // the witness node is locally known. Avoids existential introduction
      // in the noisy contract context.

      predicate edf_node_represents_thread{L}(
        Scheduler_EDF_Node *node,
        Thread_Control     *thread
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
        edf_thread_node_is_earliest_ready{L}( context, heir, node );

      // --- EDF Existential form ------------------------------------------
      // The "real" EDF property exposed to callers above the scheduler
      // abstraction: heir is non-preemptible or *some* node represents it
      // as earliest-ready. `is_preemptible` is passed explicitly rather
      // than dereferenced inside the predicate body -- Thread_Control has
      // flexible-array members, which causes WP's typed_cast model to
      // drop `\valid_read(heir)` hypotheses and encode the predicate
      // application as an opaque zero-arity atom that interacts badly
      // with the surrounding context.

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

      predicate edf_preemptible_heir_is_earliest_ready{L}(
        Scheduler_EDF_Context *context,
        Thread_Control        *heir,
        boolean                is_preemptible
      ) =
        !is_preemptible || edf_thread_is_earliest_ready{L}( context, heir );

      // --- Bridge ------------------------------------------------------
      // Existential introduction: a concrete witness satisfying the
      // witness-explicit form discharges the existential form. Proves
      // cleanly in isolation; Alt-Ergo can then apply it as a rewrite
      // when the witness predicate appears alongside the existential.

      lemma edf_scheduler_node_earliest_implies_thread_earliest{L}:
        \forall Scheduler_EDF_Context *context;
        \forall Thread_Control *thread;
        \forall Scheduler_EDF_Node *node;
          edf_thread_node_is_earliest_ready{L}( context, thread, node ) ==>
          edf_thread_is_earliest_ready{L}( context, thread );
    }
*/

#endif /* VERIFICATION_6_2_EDF_PROPERTY_H */
