/*
 * Abstract thread-priority update worklist model. Verification only header.
 */

#ifndef VERIFICATION_6_2_THREAD_PRIORITY_UPDATES_H
#define VERIFICATION_6_2_THREAD_PRIORITY_UPDATES_H

#include <rtems/score/threadq.h>

/*@
  predicate thread_priority_update_pending{L}(
    Thread_queue_Context *queue_context,
    Thread_Control       *thread
  ) =
    queue_context->Priority.update_count <= 2 &&
    (
      (
        queue_context->Priority.update_count >= 1 &&
        queue_context->Priority.update[ 0 ] == thread
      ) ||
      (
        queue_context->Priority.update_count >= 2 &&
        queue_context->Priority.update[ 1 ] == thread
      )
    );
*/

#endif /* VERIFICATION_6_2_THREAD_PRIORITY_UPDATES_H */
