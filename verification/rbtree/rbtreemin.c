/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * Annotated copy of cpukit/score/src/rbtreemin.c for Frama-C/WP.
 * Differs from pristine only in the ACSL contract and loop invariants.
 *
 * Probe 2: the traversal is grounded in a real inductive
 * wellformedness predicate `wf_node` (declared in stubs-rbtree.h).
 * `\valid_read` on the walked chain is a derived consequence of
 * `wf_node_valid`, not a trusted axiom on the accessors.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems/score/rbtreeimpl.h>

/*@
  requires \valid_read(tree);
  requires g_tree_depth >= 0;
  requires wf_node(tree->rbh_root, g_tree_depth);
  assigns  g_rbtree_min \from tree, tree->rbh_root;
  ensures  \result == g_rbtree_min;
  ensures  tree->rbh_root == \null ==> \result == \null;
  ensures  \result != \null ==> \valid_read(\result);
 */
RBTree_Node *_RBTree_Minimum( const RBTree_Control *tree )
{
  RBTree_Node *parent;
  RBTree_Node *node;
  //@ ghost int d = g_tree_depth;

  parent = NULL;
  node = _RBTree_Root( tree );

  /*@
    loop invariant d >= 0;
    loop invariant wf_node(node, d);
    loop invariant node == \null || \valid_read(node);
    loop invariant parent == \null || \valid_read(parent);
    loop invariant tree->rbh_root == \null ==> parent == \null;
    loop invariant node != \null ==> tree->rbh_root != \null;
    loop assigns parent, node, d;
    loop variant d;
   */
  while ( node != NULL ) {
    parent = node;
    //@ ghost d = d - 1;
    node = _RBTree_Left( node );
  }

  /*@ ghost g_rbtree_min = parent; */
  return parent;
}
