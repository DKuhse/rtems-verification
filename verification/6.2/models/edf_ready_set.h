/*
 * Abstract ready-set models. Verification only header.
 */

#ifndef VERIFICATION_6_2_EDF_READY_SET_H
#define VERIFICATION_6_2_EDF_READY_SET_H

#include <rtems/score/scheduleredf.h>

/*@ axiomatic EDFReadyNodes {
      logic set<Scheduler_EDF_Node *> edf_ready_set{L}(
        Scheduler_EDF_Context *context
      )
        reads context->Ready,
              { node->Node | Scheduler_EDF_Node *node; \valid( node ) };

      predicate edf_ready_member{L}(
        Scheduler_EDF_Context *context,
        Scheduler_EDF_Node    *node
      ) =
        node \in edf_ready_set{L}( context );

      predicate edf_ready_empty{L}(
        Scheduler_EDF_Context *context
      ) =
        edf_ready_set{L}( context ) == \empty;

      predicate edf_ready_set_member(
        set<Scheduler_EDF_Node *> nodes,
        Scheduler_EDF_Node       *node
      ) =
        node \in nodes;

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

      // Set constructors are declared abstractly and characterised by
      // membership axioms below. FC 32's WP rejects the equivalent
      // `{ n | T *n; P(n) }` set-comprehension definitions with
      // `Concretization for comprehension sets not implemented yet`.
      logic set<Scheduler_EDF_Node *> edf_ready_singleton(
        Scheduler_EDF_Node *node
      );

      axiom edf_ready_singleton_membership:
        \forall Scheduler_EDF_Node *node;
        \forall Scheduler_EDF_Node *n;
          ( n \in edf_ready_singleton( node ) ) <==> ( n == node );

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

      predicate edf_ready_minimum_node{L}(
        set<Scheduler_EDF_Node *> nodes,
        Scheduler_EDF_Node       *node
      ) =
        edf_ready_valid_nodes{L}( nodes ) &&
        node \in nodes &&
        \forall Scheduler_EDF_Node *other;
          other \in nodes ==> node->priority <= other->priority;

      predicate edf_ready_minimum{L}(
        Scheduler_EDF_Context *context,
        Scheduler_EDF_Node    *node
      ) =
        edf_ready_minimum_node{L}( edf_ready_set{L}( context ), node );
    }
*/

#endif /* VERIFICATION_6_2_EDF_READY_SET_H */
