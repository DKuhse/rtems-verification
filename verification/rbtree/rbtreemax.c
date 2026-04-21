/* SPDX-License-Identifier: BSD-2-Clause */

/*
 * Annotated copy of cpukit/score/src/rbtreemax.c for Frama-C/WP.
 * Differs from pristine only in the ACSL contract and loop invariants.
 *
 * Mirror of rbtreemin.c: walks rbe_right instead of rbe_left, tightens
 * the `lo` bound instead of the `hi` bound on each step.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <rtems/score/rbtreeimpl.h>

/*@
  requires \valid_read(tree);
  requires bst_node(tree->rbh_root, g_tree_depth, g_lo, g_hi);
  assigns  g_rbtree_max \from tree, tree->rbh_root;
  ensures  \result == g_rbtree_max;
  ensures  tree->rbh_root == \null ==> \result == \null;
  ensures  tree->rbh_root != \null ==> \result != \null;
  ensures  \result != \null ==> \valid_read(\result);
  ensures  \result != \null ==> \result->Node.rbe_right == \null;
  ensures  \result != \null ==> key(\result) > g_lo;
  ensures  \result != \null && tree->rbh_root != \null
           ==> key(\result) >= key(tree->rbh_root);
  ensures  \result != \null ==>
             \forall RBTree_Node *n;
               in_subtree(tree->rbh_root, n) ==> key(\result) >= key(n);
 */
RBTree_Node *_RBTree_Maximum( const RBTree_Control *tree )
{
  RBTree_Node *parent;
  RBTree_Node *node;
  //@ ghost int d = g_tree_depth;

  parent = NULL;
  node = _RBTree_Root( tree );

  /*@
    loop invariant d >= 0;
    loop invariant parent == \null ==> bst_node(node, d, g_lo, g_hi);
    loop invariant parent != \null ==> bst_node(node, d, key(parent), g_hi);
    loop invariant parent == \null || \valid_read(parent);
    loop invariant parent == \null || parent->Node.rbe_right == node;
    loop invariant parent != \null ==> key(parent) > g_lo;
    loop invariant parent != \null && tree->rbh_root != \null
                   ==> key(parent) >= key(tree->rbh_root);
    loop invariant parent == \null ==> node == tree->rbh_root;
    loop invariant tree->rbh_root == \null ==> parent == \null;
    loop invariant parent != \null ==> in_subtree(tree->rbh_root, parent);
    loop invariant node   != \null ==> in_subtree(tree->rbh_root, node);
    loop invariant \forall RBTree_Node *m;
      in_subtree(tree->rbh_root, m) ==>
           (node != \null && in_subtree(node, m))
        || (parent != \null && key(parent) >= key(m));
    loop assigns parent, node, d;
    loop variant d;
   */
  while ( node != NULL ) {
    parent = node;
    //@ ghost d = d - 1;
    node = _RBTree_Right( node );
  }

  /*@ ghost g_rbtree_max = parent; */
  return parent;
}
