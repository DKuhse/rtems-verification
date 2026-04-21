/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * Annotated copy of cpukit/score/src/rbtreemin.c for Frama-C/WP.
 * Differs from pristine only in the ACSL contract and loop invariants.
 *
 * Probe 3: BST invariant. `bst_node(n, d, lo, hi)` carries BST ordering
 * (every key in the subtree strictly in (lo, hi)). The dynamic upper
 * bound is expressed through `parent`: once parent is non-null, the
 * hi bound tightens to `key(parent)`.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems/score/rbtreeimpl.h>

/*@
  requires \valid_read(tree);
  requires bst_node(tree->rbh_root, g_tree_depth, g_lo, g_hi);
  assigns  g_rbtree_min \from tree, tree->rbh_root;
  ensures  \result == g_rbtree_min;
  ensures  tree->rbh_root == \null ==> \result == \null;
  ensures  tree->rbh_root != \null ==> \result != \null;
  ensures  \result != \null ==> \valid_read(\result);
  ensures  \result != \null ==> \result->Node.rbe_left == \null;
  ensures  \result != \null ==> key(\result) < g_hi;
  ensures  \result != \null && tree->rbh_root != \null
           ==> key(\result) <= key(tree->rbh_root);
  ensures  \result != \null ==>
             \forall RBTree_Node *n;
               in_subtree(tree->rbh_root, n) ==> key(\result) <= key(n);
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
    loop invariant parent == \null ==> bst_node(node, d, g_lo, g_hi);
    loop invariant parent != \null ==> bst_node(node, d, g_lo, key(parent));
    loop invariant parent == \null || \valid_read(parent);
    loop invariant parent == \null || parent->Node.rbe_left == node;
    loop invariant parent != \null ==> key(parent) < g_hi;
    loop invariant parent != \null && tree->rbh_root != \null
                   ==> key(parent) <= key(tree->rbh_root);
    loop invariant parent == \null ==> node == tree->rbh_root;
    loop invariant tree->rbh_root == \null ==> parent == \null;
    loop invariant parent != \null ==> in_subtree(tree->rbh_root, parent);
    loop invariant node   != \null ==> in_subtree(tree->rbh_root, node);
    loop invariant \forall RBTree_Node *m;
      in_subtree(tree->rbh_root, m) ==>
           (node != \null && in_subtree(node, m))
        || (parent != \null && key(parent) <= key(m));
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
