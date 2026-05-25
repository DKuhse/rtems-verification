/*
 * Abstract EDF scheduler property model. Verification only header.
 */

#ifndef VERIFICATION_5_1_EDF_PROPERTY_H
#define VERIFICATION_5_1_EDF_PROPERTY_H

#include <stddef.h>
#include <stdint.h>
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
        is_preemptible ==> edf_thread_is_earliest_ready{L}( context, heir );

      predicate edf_dispatch_set_if_heir_differs(
        Thread_Control *executing,
        Thread_Control *heir,
        boolean         dispatch_necessary
      ) =
        executing != heir ==> dispatch_necessary;


      // EDF scheduler invariant
      predicate edf_scheduler_decision{L}(
        Scheduler_EDF_Context *context,
        Thread_Control        *executing,
        Thread_Control        *heir,
        boolean                is_preemptible,
        boolean                dispatch_necessary
      ) =
        // (P3.a) scheduler picks argmin from ready queue
        edf_preemptible_heir_is_earliest_ready{L}(
          context, heir, is_preemptible ) &&
        // (P3.b) heir != executing ==> context switch scheduled
        edf_dispatch_set_if_heir_differs(
          executing, heir, dispatch_necessary );

      // --- Bridge ------------------------------------------------------
      // Existential introduction: a concrete witness satisfying the
      // witness-explicit form discharges the existential form. Proves
      // cleanly in isolation; can then be automatically applied as a rewrite
      // when the witness predicate appears alongside the existential.

      lemma edf_scheduler_node_earliest_implies_thread_earliest{L}:
        \forall Scheduler_EDF_Context *context;
        \forall Thread_Control *thread;
        \forall Scheduler_EDF_Node *node;
          edf_thread_node_is_earliest_ready{L}( context, thread, node ) ==>
          edf_thread_is_earliest_ready{L}( context, thread );

      // --- Earliest preservation under extraction --------------------
      // Removing a node distinct from the witness preserves the witness
      // as earliest.

      lemma edf_ready_earliest_preserved_under_extract{L}:
        \forall set<Scheduler_EDF_Node *> nodes;
        \forall Scheduler_EDF_Node *old;
        \forall Scheduler_EDF_Node *removed;
          edf_ready_earliest_node{L}( nodes, old ) &&
          old != removed ==>
            edf_ready_earliest_node{L}(
              edf_ready_extract( nodes, removed ),
              old
            );

      // --- Earliest preservation/replacement under insertion ----------
      // If the inserted node is not earlier than the old witness, the
      // old witness remains earliest. If it is, the inserted node
      // becomes a new witness.

      lemma edf_ready_earliest_preserved_under_insert{L}:
        \forall set<Scheduler_EDF_Node *> nodes;
        \forall Scheduler_EDF_Node *old;
        \forall Scheduler_EDF_Node *added;
          edf_ready_earliest_node{L}( nodes, old ) &&
          edf_ready_node_not_after{L}( old, added ) ==>
            edf_ready_earliest_node{L}(
              edf_ready_insert( nodes, added ),
              old
            );

      lemma edf_ready_new_earliest_under_insert{L}:
        \forall set<Scheduler_EDF_Node *> nodes;
        \forall Scheduler_EDF_Node *old;
        \forall Scheduler_EDF_Node *added;
          edf_ready_earliest_node{L}( nodes, old ) &&
          edf_ready_node_not_after{L}( added, old ) ==>
            edf_ready_earliest_node{L}(
              edf_ready_insert( nodes, added ),
              added
            );

      lemma edf_preemptible_heir_earliest_preserved{L1,L2}:
        \forall Scheduler_EDF_Context *context;
        \forall Thread_Control *heir;
        \forall boolean is_preemptible;
          edf_preemptible_heir_is_earliest_ready{L1}(
            context,
            heir,
            is_preemptible
          ) &&
          edf_ready_valid_nodes{L2}( edf_ready_set{L2}( context ) ) &&
          edf_ready_set{L2}( context ) == edf_ready_set{L1}( context ) &&
          (
            \forall Scheduler_EDF_Node *node;
              node \in edf_ready_set{L1}( context ) ==>
                \at( node->priority, L2 ) == \at( node->priority, L1 )
          ) &&
          (
            \forall Scheduler_EDF_Node *node;
              node \in edf_ready_set{L1}( context ) ==>
                \at( node->Base.owner, L2 ) == \at( node->Base.owner, L1 )
          ) ==>
            edf_preemptible_heir_is_earliest_ready{L2}(
              context,
              heir,
              is_preemptible
            );
    }
*/

/*@
  // EDF analogue of the C `RTEMS_CONTAINER_OF( p, Scheduler_EDF_Node, Node )`
  // expression.  Defined logic function (not abstract) so WP unfolds it on
  // demand; the unfolded form uses `(uint64_t) ((uintptr_t) ... - 80)` to
  // match the body's C macro expansion at the WP encoding level (both sides
  // canonicalise to `addr_of_int(to_uint64(int_of_addr(p) - 80))`).
  //
  // The hardcoded `80` is `offsetof(Scheduler_EDF_Node, Node)` on this
  // layout.  We cannot just write `RTEMS_CONTAINER_OF(...)` in the
  // annotation: Frama-C does expand macros in annotations (default
  // `-pp-annot`), but the expansion contains `offsetof(T, F)` which becomes
  // `__builtin_offsetof(T, F)`, and the comma between args is rejected by
  // the ACSL parser.  Redefining `RTEMS_CONTAINER_OF` under `__FRAMAC__` to
  // an `offsetof`-free form lets the macro parse in ACSL, but the resulting
  // WP encoding (using a `shiftfield_Node(null)` opaque offset) was harder
  // for Alt-Ergo to bridge against the body than this hand-inlined form,
  // which produces exactly the same term as the unmodified body macro.
  logic Scheduler_EDF_Node *
    edf_container_of_rbtree_node( RBTree_Node *p ) =
      (Scheduler_EDF_Node *)
        (uint64_t) ( (uintptr_t) p - (uintptr_t) 80 );
*/

/*
 * EDF-specialized contract on `_RBTree_Minimum`.
 * In RTEMS 5.1 there's no EDF-specific minimum wrapper, so we annotate
 * this here instead of in the generic rbtree.h.
 * This is the actual abstract-RBTree boundary: callers above this layer
 * never reason about RBTree pointer/color mechanics.
 */
/*@
  requires \valid_read( the_rbtree );

  assigns \nothing;

  ensures the_rbtree->rbh_root == \null ==> \result == \null;
  ensures the_rbtree->rbh_root != \null ==> \result != \null;

  ensures
    ( \exists Scheduler_EDF_Node *m;
        m \in edf_ready_set_from_root( the_rbtree->rbh_root ) ) ==>
    edf_ready_earliest_node{Here}(
      edf_ready_set_from_root( the_rbtree->rbh_root ),
      edf_container_of_rbtree_node( \result ) );
*/
RBTree_Node *_RBTree_Minimum( const RBTree_Control *the_rbtree );

#endif /* VERIFICATION_5_1_EDF_PROPERTY_H */
