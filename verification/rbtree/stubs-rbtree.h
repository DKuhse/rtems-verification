/*
 * Stubs for RBTree Frama-C/WP verification.
 *
 * Pulled in via `-include` before any TU source is preprocessed. The
 * structural predicates (`wf_node`, `wf_node_valid`, `wf_node_left`)
 * and the ghost declarations live in the annotated rbtree.h — they
 * have to precede their first use inside the header's own contracts.
 *
 * This file exists as an explicit extension point and to mirror the
 * EDF verification's stubs.h layout.
 */

#include <rtems/score/rbtree.h>
