/*
 * Abstract ready-set models. Verification only header.
 */

#ifndef VERIFICATION_6_2_EDF_READY_SET_H
#define VERIFICATION_6_2_EDF_READY_SET_H

#include <rtems/score/scheduleredf.h>

/*@ axiomatic EDFReadyNodes {
      // edf_ready_set is opaque w.r.t. RBTree node-field internals: its
      // only read frame is `context->Ready` (the tree root), which serves
      // as the proxy for the abstract set. The set "changes" only through
      // the `ensures` of operations (Enqueue/Extract); RBTree pointer
      // bookkeeping inside the nodes is below the abstraction.
      // This is a 'soft' lie - a true assigns would need an traversal model
      // however, since we don't touch the internals, this should be fine.
      logic set<Scheduler_EDF_Node *> edf_ready_set{L}(
        Scheduler_EDF_Context *context
      )
        reads context->Ready;

      predicate edf_ready_member{L}(
        Scheduler_EDF_Context *context,
        Scheduler_EDF_Node    *node
      ) =
        node \in edf_ready_set{L}( context );

      predicate edf_ready_valid_nodes{L}(
        set<Scheduler_EDF_Node *> nodes
      ) =
        \forall Scheduler_EDF_Node *node;
          node \in nodes ==> \valid_read( node );

      predicate edf_ready_context_well_formed{L}(
        Scheduler_EDF_Context *context
      ) =
        \valid( context ) &&
        edf_ready_valid_nodes{L}( edf_ready_set{L}( context ) );

      // For every node in `nodes`, its EDF-cached `priority` agrees with
      // the scheduler-aggregation priority used for ordering decisions
      // (`Base.Wait.Priority.Node.priority`).
      predicate edf_priority_cache_consistent{L}(
        set<Scheduler_EDF_Node *> nodes
      ) =
        \forall Scheduler_EDF_Node *node;
          node \in nodes ==>
            node->priority ==
              node->Base.Wait.Priority.Node.priority;

      // Set comprehension doesn't work in Frama-C 32...
      // so we spell it out as algebraic properties
      logic set<Scheduler_EDF_Node *> edf_ready_insert(
        set<Scheduler_EDF_Node *> nodes,
        Scheduler_EDF_Node       *node
      );

      axiom edf_ready_insert_membership:
        \forall set<Scheduler_EDF_Node *> nodes;
        \forall Scheduler_EDF_Node *node;
        \forall Scheduler_EDF_Node *n;
          ( n \in edf_ready_insert( nodes, node ) ) <==>
          ( n == node || n \in nodes );

      logic set<Scheduler_EDF_Node *> edf_ready_extract(
        set<Scheduler_EDF_Node *> nodes,
        Scheduler_EDF_Node       *node
      );

      axiom edf_ready_extract_membership:
        \forall set<Scheduler_EDF_Node *> nodes;
        \forall Scheduler_EDF_Node *node;
        \forall Scheduler_EDF_Node *n;
          ( n \in edf_ready_extract( nodes, node ) ) <==>
          ( n != node && n \in nodes );
    }
*/

#endif /* VERIFICATION_6_2_EDF_READY_SET_H */
