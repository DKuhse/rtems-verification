/*
 * Abstract ready-set models. Verification only header.
 */

#ifndef VERIFICATION_6_2_EDF_READY_MODEL_H
#define VERIFICATION_6_2_EDF_READY_MODEL_H

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

      logic set<Scheduler_EDF_Node *> edf_ready_singleton(
        Scheduler_EDF_Node *node
      ) =
        { n | Scheduler_EDF_Node *n; n == node };

      logic set<Scheduler_EDF_Node *> edf_ready_insert(
        set<Scheduler_EDF_Node *> nodes,
        Scheduler_EDF_Node       *node
      ) =
        { n | Scheduler_EDF_Node *n; n == node || n \in nodes };

      logic set<Scheduler_EDF_Node *> edf_ready_extract(
        set<Scheduler_EDF_Node *> nodes,
        Scheduler_EDF_Node       *node
      ) =
        { n | Scheduler_EDF_Node *n; n != node && n \in nodes };

      predicate edf_ready_minimum_node{L}(
        set<Scheduler_EDF_Node *> nodes,
        Scheduler_EDF_Node       *node
      ) =
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

#endif /* VERIFICATION_6_2_EDF_READY_MODEL_H */
