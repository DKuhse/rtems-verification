/*
 * Scheduler-level predicates layered on the abstract FreeRTOS list model.
 *
 * Include this after struct tskTaskControlBlock is complete.  This layer may
 * talk about TCB ownership and deadlines; freertos_list_model.h must stay
 * generic and task-agnostic.
 */

#ifndef VERIFICATION_FREERTOS_MODEL_SCHEDULER_MODEL_H
#define VERIFICATION_FREERTOS_MODEL_SCHEDULER_MODEL_H

#include "freertos_list_model.h"

#ifndef __FRAMAC__
    #error "scheduler_model.h is a Frama-C verification-only model."
#endif

/*@
  predicate TaskItem{L}(ListItem_t *item) =
    \valid(item) &&
    item->pvOwner != \null &&
    \valid((struct tskTaskControlBlock *)item->pvOwner) &&
    &((struct tskTaskControlBlock *)item->pvOwner)->xStateListItem == item;

  predicate TaskList{L}(List_t *list) =
    ListInv(list) &&
    HeadIsMinimum(list) &&
    \forall ListItem_t *item;
      \valid(item) && In(item, list) ==> TaskItem(item);

  predicate ReadyItemDeadlineMatches{L}(ListItem_t *item) =
    TaskItem(item) &&
    item->xItemValue ==
      ((struct tskTaskControlBlock *)item->pvOwner)->xDeadline;

  predicate ReadyListDeadlineMatches{L}(List_t *ready) =
    \forall ListItem_t *item;
      \valid(item) && In(item, ready) ==>
        ReadyItemDeadlineMatches(item);

  predicate ReadyList{L}(List_t *ready) =
    TaskList(ready) &&
    ReadyListDeadlineMatches(ready);

  predicate DelayedList{L}(List_t *delayed) =
    TaskList(delayed);

  predicate ReadyInsertPre{L}(List_t *ready, ListItem_t *item) =
    ReadyList(ready) &&
    Detached(item) &&
    ReadyItemDeadlineMatches(item);

  predicate EDFProperty{L}(List_t *ready,
                           struct tskTaskControlBlock *running) =
    \valid(running) &&
    In(&running->xStateListItem, ready) &&
    \forall ListItem_t *item;
      \valid(item) && In(item, ready) ==>
        running->xDeadline <=
          ((struct tskTaskControlBlock *)item->pvOwner)->xDeadline;
*/

#endif /* VERIFICATION_FREERTOS_MODEL_SCHEDULER_MODEL_H */
