/*
 * Abstract priority aggregation contributor model. Verification only header.
 */

#ifndef VERIFICATION_6_2_PRIORITY_AGGREGATION_H
#define VERIFICATION_6_2_PRIORITY_AGGREGATION_H

#include <rtems/score/priority.h>

/*@ axiomatic PriorityAggregation {
      // priority_contributors is opaque w.r.t. RBTree node-field internals.
      // The root pointer selects the abstract set, and the root node is the
      // explicit read frame of the root-level model.  The set changes only
      // through the contracts of priority aggregation operations.
      // (see edf_ready_set.h)
      logic set<Priority_Node *> priority_contributors_from_root(
        RBTree_Node *root
      )
        reads root;

      logic set<Priority_Node *> priority_contributors{L}(
        Priority_Aggregation *aggregation
      ) =
        priority_contributors_from_root( aggregation->Contributors.rbh_root );

      predicate priority_contributor_member{L}(
        Priority_Aggregation *aggregation,
        Priority_Node        *node
      ) =
        node \in priority_contributors{L}( aggregation );

      predicate priority_node_active{L}(
        Priority_Node *node
      ) =
        \valid_read( node ) &&
        node->Node.RBTree.Node.rbe_color != -1;

      predicate priority_node_active_iff_contributor{L}(
        Priority_Aggregation *aggregation,
        Priority_Node        *node
      ) =
        priority_node_active{L}( node ) <==>
          priority_contributor_member{L}( aggregation, node );

      predicate priority_contributors_valid_nodes{L}(
        set<Priority_Node *> nodes
      ) =
        \forall Priority_Node *node;
          node \in nodes ==> \valid_read( node );

      predicate priority_contributors_separated_from_cache{L}(
        Priority_Aggregation *aggregation
      ) =
        \forall Priority_Node *node;
          node \in priority_contributors{L}( aggregation ) ==>
            \separated( node + (..), &aggregation->Node );

      // Structural aggregation invariant. The cached minimum stored in
      // aggregation->Node.priority is intentionally modeled separately
      // as it can be temporarily stale
      predicate priority_aggregation_well_formed{L}(
        Priority_Aggregation *aggregation
      ) =
        \valid( aggregation ) &&
        priority_contributors_valid_nodes{L}(
          priority_contributors{L}( aggregation ) ) &&
        priority_contributors_separated_from_cache{L}( aggregation );

      predicate priority_node_not_after{L}(
        Priority_Node *left,
        Priority_Node *right
      ) =
        \valid_read( left ) &&
        \valid_read( right ) &&
        left->priority <= right->priority;

      predicate priority_contributors_minimum_node{L}(
        set<Priority_Node *> nodes,
        Priority_Node       *node
      ) =
        priority_contributors_valid_nodes{L}( nodes ) &&
        node \in nodes &&
        \forall Priority_Node *other;
          other \in nodes ==> priority_node_not_after{L}( node, other );

      // minimum is correctly cached in aggregation->Node.priority
      predicate priority_aggregation_cached_minimum{L}(
        Priority_Aggregation *aggregation
      ) =
        \valid_read( aggregation ) &&
        \exists Priority_Node *node;
          priority_contributors_minimum_node{L}(
            priority_contributors{L}( aggregation ),
            node
          ) &&
          aggregation->Node.priority == node->priority;


      // --- RBTree operations -------------------------------------

      logic set<Priority_Node *> priority_contributors_insert(
        set<Priority_Node *> nodes,
        Priority_Node       *node
      );

      axiom priority_contributors_insert_membership:
        \forall set<Priority_Node *> nodes;
        \forall Priority_Node *node;
        \forall Priority_Node *n;
          ( n \in priority_contributors_insert( nodes, node ) ) <==>
          ( n == node || n \in nodes );

      logic set<Priority_Node *> priority_contributors_extract(
        set<Priority_Node *> nodes,
        Priority_Node       *node
      );

      axiom priority_contributors_extract_membership:
        \forall set<Priority_Node *> nodes;
        \forall Priority_Node *node;
        \forall Priority_Node *n;
          ( n \in priority_contributors_extract( nodes, node ) ) <==>
          ( n != node && n \in nodes );

      // --- Valid-node preservation lemmas ------------------------------

      lemma priority_contributors_valid_nodes_under_extract{L}:
        \forall set<Priority_Node *> nodes;
        \forall Priority_Node *removed;
          priority_contributors_valid_nodes{L}( nodes ) ==>
            priority_contributors_valid_nodes{L}(
              priority_contributors_extract( nodes, removed ) );

      lemma priority_contributors_valid_nodes_under_insert{L}:
        \forall set<Priority_Node *> nodes;
        \forall Priority_Node *added;
          priority_contributors_valid_nodes{L}( nodes ) &&
          \valid_read( added ) ==>
            priority_contributors_valid_nodes{L}(
              priority_contributors_insert( nodes, added ) );

      // --- Minimum preservation/replacement lemmas ---------------------

      lemma priority_minimum_preserved_under_extract{L}:
        \forall set<Priority_Node *> nodes;
        \forall Priority_Node *old;
        \forall Priority_Node *removed;
          priority_contributors_minimum_node{L}( nodes, old ) &&
          old != removed ==>
            priority_contributors_minimum_node{L}(
              priority_contributors_extract( nodes, removed ),
              old
            );

      lemma priority_minimum_preserved_under_insert{L}:
        \forall set<Priority_Node *> nodes;
        \forall Priority_Node *old;
        \forall Priority_Node *added;
          priority_contributors_minimum_node{L}( nodes, old ) &&
          priority_node_not_after{L}( old, added ) ==>
            priority_contributors_minimum_node{L}(
              priority_contributors_insert( nodes, added ),
              old
            );

      lemma priority_new_minimum_under_insert{L}:
        \forall set<Priority_Node *> nodes;
        \forall Priority_Node *added;
          priority_contributors_valid_nodes{L}( nodes ) &&
          \valid_read( added ) &&
          ( \forall Priority_Node *other;
              other \in nodes ==>
                priority_node_not_after{L}( added, other ) ) ==>
            priority_contributors_minimum_node{L}(
              priority_contributors_insert( nodes, added ),
              added
            );

      lemma priority_aggregation_cached_minimum_preserved{L1,L2}:
        \forall Priority_Aggregation *aggregation;
          priority_aggregation_well_formed{L2}( aggregation ) &&
          priority_aggregation_cached_minimum{L1}( aggregation ) &&
          priority_contributors{L2}( aggregation ) ==
            priority_contributors{L1}( aggregation ) &&
          \at( aggregation->Node.priority, L2 ) ==
            \at( aggregation->Node.priority, L1 ) &&
          ( \forall Priority_Node *node;
              node \in priority_contributors{L1}( aggregation ) ==>
                \at( node->priority, L2 ) == \at( node->priority, L1 ) ) ==>
            priority_aggregation_cached_minimum{L2}( aggregation );
    }
*/

#endif /* VERIFICATION_6_2_PRIORITY_AGGREGATION_H */
