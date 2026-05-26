/*
 * Abstract ready-set models. Verification only header.
 */

#ifndef VERIFICATION_6_2_EDF_READY_SET_H
#define VERIFICATION_6_2_EDF_READY_SET_H

#include <rtems/score/scheduleredf.h>

/*@ axiomatic EDFReadyNodes {
      // edf_ready_set is opaque w.r.t. RBTree node-field internals.  The root
      // pointer selects the abstract set, and the root node is the explicit
      // read frame of the root-level model.  The set "changes" only through
      // the ensures of operations (Enqueue/Extract); full RBTree traversal
      // and node-link bookkeeping stay below the abstraction boundary.

      logic set<Scheduler_EDF_Node *> edf_ready_set_from_root(
        RBTree_Node *root
      )
        reads root;

      axiom edf_ready_set_from_null_empty:
        \forall Scheduler_EDF_Node *node;
          !( node \in edf_ready_set_from_root( \null ) );

      logic set<Scheduler_EDF_Node *> edf_ready_set{L}(
        Scheduler_EDF_Context *context
      ) =
        edf_ready_set_from_root( context->Ready.rbh_root );

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

      // Owner-injective ready set: one ready node per thread
      predicate edf_ready_owners_distinct{L}(
        set<Scheduler_EDF_Node *> nodes
      ) =
        \forall Scheduler_EDF_Node *m1, *m2;
          m1 \in nodes && m2 \in nodes &&
            m1->Base.owner == m2->Base.owner ==> m1 == m2;

      // Owner-canonical ready node: the owner points back to this node as
      // its home scheduler node. This is the node-side ownership bijection
      // invariant used by the EDF proof.
      predicate edf_ready_node_has_canonical_owner{L}(
        Scheduler_EDF_Node *node
      ) =
        \valid_read( node ) &&
        node->Base.owner != \null &&
        \valid_read( &node->Base.owner->Scheduler.nodes ) &&
        node->Base.owner->Scheduler.nodes == &node->Base;

      predicate edf_ready_owners_canonical{L}(
        set<Scheduler_EDF_Node *> nodes
      ) =
        \forall Scheduler_EDF_Node *node;
          node \in nodes ==>
            edf_ready_node_has_canonical_owner{L}( node );

      predicate edf_ready_context_well_formed{L}(
        Scheduler_EDF_Context *context
      ) =
        \valid( context ) &&
        edf_ready_valid_nodes{L}( edf_ready_set{L}( context ) ) &&
        edf_ready_owners_distinct{L}( edf_ready_set{L}( context ) ) &&
        edf_ready_owners_canonical{L}( edf_ready_set{L}( context ) );

      // For every node in `nodes`, its EDF-cached `priority` agrees with
      // the scheduler-aggregation priority used for ordering decisions
      // (`Base.Wait.Priority.Node.priority`).
      predicate edf_ready_node_cache_consistent{L}(
        Scheduler_EDF_Node *node
      ) =
        node->priority ==
          node->Base.Wait.Priority.Node.priority;

      predicate edf_priority_cache_consistent{L}(
        set<Scheduler_EDF_Node *> nodes
      ) =
        \forall Scheduler_EDF_Node *node;
          node \in nodes ==>
            edf_ready_node_cache_consistent{L}( node );

      predicate edf_ready_context_cache_consistent{L}(
        Scheduler_EDF_Context *context
      ) =
        \valid( context ) &&
        edf_priority_cache_consistent{L}( edf_ready_set{L}( context ) );

      predicate edf_priority_cache_consistency_preserved{L1,L2}(
        set<Scheduler_EDF_Node *> nodes
      ) =
        \forall Scheduler_EDF_Node *node;
          node \in nodes &&
          edf_ready_node_cache_consistent{L1}( node ) ==>
            edf_ready_node_cache_consistent{L2}( node );

      // --- RBTree operations -------------------------------------

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

      // --- Owner-distinct preservation lemmas ----------------------------
      lemma edf_ready_owners_distinct_under_extract{L}:
        \forall set<Scheduler_EDF_Node *> nodes;
        \forall Scheduler_EDF_Node *removed;
          edf_ready_owners_distinct{L}( nodes ) ==>
            edf_ready_owners_distinct{L}(
              edf_ready_extract( nodes, removed ) );

      lemma edf_ready_owners_distinct_under_insert{L}:
        \forall set<Scheduler_EDF_Node *> nodes;
        \forall Scheduler_EDF_Node *added;
          edf_ready_owners_distinct{L}( nodes ) &&
          ( \forall Scheduler_EDF_Node *m;
              m \in nodes ==> m->Base.owner != added->Base.owner ) ==>
            edf_ready_owners_distinct{L}(
              edf_ready_insert( nodes, added ) );

      // --- Owner-canonical preservation lemmas --------------------------
      lemma edf_ready_owners_canonical_under_extract{L}:
        \forall set<Scheduler_EDF_Node *> nodes;
        \forall Scheduler_EDF_Node *removed;
          edf_ready_owners_canonical{L}( nodes ) ==>
            edf_ready_owners_canonical{L}(
              edf_ready_extract( nodes, removed ) );

      lemma edf_ready_owners_canonical_under_insert{L}:
        \forall set<Scheduler_EDF_Node *> nodes;
        \forall Scheduler_EDF_Node *added;
          edf_ready_owners_canonical{L}( nodes ) &&
          edf_ready_node_has_canonical_owner{L}( added ) ==>
            edf_ready_owners_canonical{L}(
              edf_ready_insert( nodes, added ) );

      // --- Priority-cache preservation lemmas ---------------------------
      lemma edf_priority_cache_consistent_under_extract{L}:
        \forall set<Scheduler_EDF_Node *> nodes;
        \forall Scheduler_EDF_Node *removed;
          edf_priority_cache_consistent{L}( nodes ) ==>
            edf_priority_cache_consistent{L}(
              edf_ready_extract( nodes, removed ) );

      lemma edf_priority_cache_consistent_under_insert{L}:
        \forall set<Scheduler_EDF_Node *> nodes;
        \forall Scheduler_EDF_Node *added;
          edf_priority_cache_consistent{L}( nodes ) &&
          edf_ready_node_cache_consistent{L}( added ) ==>
            edf_priority_cache_consistent{L}(
              edf_ready_insert( nodes, added ) );

      lemma edf_priority_cache_consistent_with_refreshed_member{L}:
        \forall set<Scheduler_EDF_Node *> nodes;
        \forall Scheduler_EDF_Node *node;
          node \in nodes &&
          edf_priority_cache_consistent{L}(
            edf_ready_extract( nodes, node ) ) &&
          edf_ready_node_cache_consistent{L}( node ) ==>
            edf_priority_cache_consistent{L}( nodes );
    }
*/

#endif /* VERIFICATION_6_2_EDF_READY_SET_H */
