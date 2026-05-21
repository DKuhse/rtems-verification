/*
 * Scheduler-level predicates layered on the abstract FreeRTOS list model.
 *
 * Include this after struct tskTaskControlBlock is complete.  This layer may
 * talk about TCB ownership and deadlines; list_model.h must stay
 * generic and task-agnostic.
 */

#ifndef VERIFICATION_FREERTOS_MODEL_SCHEDULER_MODEL_H
#define VERIFICATION_FREERTOS_MODEL_SCHEDULER_MODEL_H

#include "list_model.h"

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

  predicate TaskEventListLinkValid{L}(struct tskTaskControlBlock *task,
                                      List_t *readyList,
                                      List_t *delayedList,
                                      List_t *overflowDelayedList) =
    \valid(task) &&
    \valid(&task->xEventListItem) &&
    (task->xEventListItem.pxContainer == \null ||
      (ListInv(task->xEventListItem.pxContainer) &&
       In(&task->xEventListItem, task->xEventListItem.pxContainer) &&
       task->xEventListItem.pxContainer->uxNumberOfItems > (UBaseType_t)0 &&
       task->xEventListItem.pxContainer != readyList &&
       task->xEventListItem.pxContainer != delayedList &&
       task->xEventListItem.pxContainer != overflowDelayedList));

  predicate DelayedTasksHaveValidEventListLinks{L}(List_t *delayed,
                                                   List_t *readyList,
                                                   List_t *overflowDelayedList) =
    \forall ListItem_t *stateItem;
      \valid(stateItem) && In(stateItem, delayed) ==>
        TaskItem(stateItem) &&
        TaskEventListLinkValid(
          (struct tskTaskControlBlock *)stateItem->pvOwner,
          readyList,
          delayed,
          overflowDelayedList);

  predicate SchedulerListContext{L}(List_t *ready,
                                    List_t *delayed,
                                    List_t *overflowDelayed) =
    ready != delayed &&
    ready != overflowDelayed &&
    delayed != overflowDelayed &&
    ReadyList(ready) &&
    DelayedList(delayed) &&
    DelayedList(overflowDelayed) &&
    Disjoint(ready, delayed) &&
    Disjoint(ready, overflowDelayed) &&
    Disjoint(delayed, overflowDelayed) &&
    DelayedTasksHaveValidEventListLinks(delayed, ready, overflowDelayed) &&
    DelayedTasksHaveValidEventListLinks(overflowDelayed, ready, delayed);

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
