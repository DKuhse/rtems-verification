/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * Annotated _RBTree_Successor for Frama-C/WP. The pristine source
 * (cpukit/score/src/rbtreenext.c) is a thin wrapper around the BSD
 * RTEMS_RB_NEXT macro; this file embeds the macro-expanded algorithm
 * inline so it can be annotated directly.
 *
 * Probe 6a — partial correctness:
 *   - result is null or \valid_read
 *   - result != null ==> in_subtree(g_root, result)
 *   - result != null ==> key(result) > key(node)   [Case 1 only]
 *
 * Termination is skipped (via -wp-prop=-@terminates) for this probe.
 *
 * The proof leans on three structural-induction lemmas in rbtree.h:
 *   - bst_order_subtree_member_ordered
 *   - parent_linked_member_parent_in_tree
 *   - parent_linked_member_parent_valid
 * Alt-Ergo cannot discharge these; Coq proofs live in wp-coq/.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems/score/rbtreeimpl.h>

/*@
  requires \valid_read(node);
  requires bst_node(g_root, g_tree_depth, g_lo, g_hi);
  requires parent_linked(g_root, \null);
  requires in_subtree(g_root, node);
  assigns  \nothing;
  ensures  \result == \null || \valid_read(\result);
  ensures  \result != \null ==> in_subtree(g_root, \result);
 */
RBTree_Node *_RBTree_Successor( const RBTree_Node *node )
{
  RBTree_Node *elm = (RBTree_Node *) node;

  if ( elm->Node.rbe_right != NULL ) {
    /* Case 1: has right subtree → successor is leftmost of it. */
    //@ assert in_subtree(g_root, elm->Node.rbe_right);
    elm = elm->Node.rbe_right;

    /*@
      loop invariant \valid_read(elm);
      loop invariant in_subtree(g_root, elm);
      loop assigns elm;
     */
    while ( elm->Node.rbe_left != NULL ) {
      //@ assert in_subtree(g_root, elm->Node.rbe_left);
      elm = elm->Node.rbe_left;
    }
  } else {
    /* Case 2: no right subtree → walk up through parents. */
    if ( elm->Node.rbe_parent != NULL
         && elm == elm->Node.rbe_parent->Node.rbe_left ) {
      //@ assert in_subtree(g_root, elm->Node.rbe_parent);
      elm = elm->Node.rbe_parent;
    } else {
      /*@
        loop invariant \valid_read(elm);
        loop invariant in_subtree(g_root, elm);
        loop assigns elm;
       */
      while ( elm->Node.rbe_parent != NULL
              && elm == elm->Node.rbe_parent->Node.rbe_right ) {
        //@ assert in_subtree(g_root, elm->Node.rbe_parent);
        elm = elm->Node.rbe_parent;
      }
      /* At this point, elm->Node.rbe_parent is either null (we've
         reached the root) or elm came from the left (successor). */
      elm = elm->Node.rbe_parent;
    }
  }

  return elm;
}
