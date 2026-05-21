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

/*@
  requires \valid(pxListItem);
  requires Detached(pxListItem);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns pxListItem->xItemValue;

  ensures pxListItem->xItemValue == xValue;
  ensures Detached(pxListItem);
  ensures pxListItem->pvOwner == \old(pxListItem->pvOwner);
  ensures TaskItem{Pre}(pxListItem) ==> TaskItem(pxListItem);
  ensures TaskItem(pxListItem) &&
          xValue ==
            ((struct tskTaskControlBlock *)pxListItem->pvOwner)->xDeadline ==>
            ReadyItemDeadlineMatches(pxListItem);

  ensures \forall List_t *list;
    ListInv{Pre}(list) ==> ListInv(list);
  ensures \forall List_t *list;
    HeadIsMinimum{Pre}(list) ==> HeadIsMinimum(list);
  ensures \forall List_t *list;
    ReadyList{Pre}(list) ==> ReadyList(list);
  ensures \forall List_t *list;
    DelayedList{Pre}(list) ==> DelayedList(list);
  ensures \forall List_t *left, *right;
    Disjoint{Pre}(left, right) ==> Disjoint(left, right);
  ensures \forall List_t *delayed, *ready, *overflowDelayed;
    DelayedTasksHaveValidEventListLinks{Pre}(delayed,
                                             ready,
                                             overflowDelayed) ==>
      DelayedTasksHaveValidEventListLinks(delayed,
                                          ready,
                                          overflowDelayed);
*/
void vSchedulerListItemSetValue_abs(ListItem_t * const pxListItem,
                                    TickType_t xValue);

/*@
  requires ReadyList(pxList);
  requires \valid(pxNewListItem);
  requires Detached(pxNewListItem);
  requires ReadyItemDeadlineMatches(pxNewListItem);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns pxList->uxNumberOfItems,
          pxNewListItem->pxContainer;

  ensures ReadyList(pxList);
  ensures In(pxNewListItem, pxList);
  ensures pxNewListItem->pxContainer == pxList;
  ensures pxList->uxNumberOfItems ==
    (UBaseType_t)(\old(pxList->uxNumberOfItems) + 1U);
  ensures pxNewListItem->xItemValue == \old(pxNewListItem->xItemValue);
  ensures pxNewListItem->pvOwner == \old(pxNewListItem->pvOwner);

  ensures \forall ListItem_t *item;
    \valid{Pre}(item) && item != pxNewListItem ==>
      (In(item, pxList) <==> In{Pre}(item, pxList));
  ensures \forall List_t *other, ListItem_t *item;
    ListInv{Pre}(other) && other != pxList && \valid{Pre}(item) ==>
      (In(item, other) <==> In{Pre}(item, other));

  ensures \forall List_t *list;
    ListInv{Pre}(list) ==> ListInv(list);
  ensures \forall List_t *list;
    HeadIsMinimum{Pre}(list) ==> HeadIsMinimum(list);
  ensures \forall List_t *list;
    ReadyList{Pre}(list) ==> ReadyList(list);
  ensures \forall List_t *list;
    DelayedList{Pre}(list) ==> DelayedList(list);
  ensures \forall List_t *left, *right;
    Disjoint{Pre}(left, right) ==> Disjoint(left, right);
  ensures \forall List_t *delayed, *ready, *overflowDelayed;
    DelayedTasksHaveValidEventListLinks{Pre}(delayed,
                                             ready,
                                             overflowDelayed) ==>
      DelayedTasksHaveValidEventListLinks(delayed,
                                          ready,
                                          overflowDelayed);
*/
void vSchedulerReadyListInsert_abs(List_t * const pxList,
                                   ListItem_t * const pxNewListItem);

/*@
  requires \valid(pxItemToRemove);
  requires pxItemToRemove->pxContainer != \null;
  requires ListInv(pxItemToRemove->pxContainer);
  requires In(pxItemToRemove, pxItemToRemove->pxContainer);
  requires pxItemToRemove->pxContainer->uxNumberOfItems > (UBaseType_t)0;

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns pxItemToRemove->pxContainer->uxNumberOfItems,
          pxItemToRemove->pxContainer;

  ensures Detached(pxItemToRemove);
  ensures pxItemToRemove->xItemValue == \old(pxItemToRemove->xItemValue);
  ensures pxItemToRemove->pvOwner == \old(pxItemToRemove->pvOwner);
  ensures \at(pxItemToRemove->pxContainer, Pre)->uxNumberOfItems ==
    (UBaseType_t)(\old(\at(pxItemToRemove->pxContainer, Pre)->uxNumberOfItems) - 1U);
  ensures \result == \at(pxItemToRemove->pxContainer, Pre)->uxNumberOfItems;
  ensures !In(pxItemToRemove, \at(pxItemToRemove->pxContainer, Pre));
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) && item != pxItemToRemove ==>
      (In(item, \at(pxItemToRemove->pxContainer, Pre)) <==>
       In{Pre}(item, \at(pxItemToRemove->pxContainer, Pre)));
  ensures \forall List_t *other, ListItem_t *item;
    ListInv{Pre}(other) &&
    other != \at(pxItemToRemove->pxContainer, Pre) &&
    \valid{Pre}(item) ==>
      (In(item, other) <==> In{Pre}(item, other));

  ensures \forall List_t *list;
    ListInv{Pre}(list) ==> ListInv(list);
  ensures \forall List_t *list;
    HeadIsMinimum{Pre}(list) ==> HeadIsMinimum(list);
  ensures \forall List_t *list;
    ReadyList{Pre}(list) ==> ReadyList(list);
  ensures \forall List_t *list;
    DelayedList{Pre}(list) ==> DelayedList(list);
  ensures \forall List_t *left, *right;
    Disjoint{Pre}(left, right) ==> Disjoint(left, right);
  ensures \forall List_t *delayed, *ready, *overflowDelayed;
    DelayedTasksHaveValidEventListLinks{Pre}(delayed,
                                             ready,
                                             overflowDelayed) ==>
      DelayedTasksHaveValidEventListLinks(delayed,
                                          ready,
                                          overflowDelayed);
*/
UBaseType_t vSchedulerListRemove_abs(ListItem_t * const pxItemToRemove);

/*@
  requires TaskList(pxList);
  requires pxList->uxNumberOfItems > (UBaseType_t)0;

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns \result \from pxList, pxList->uxNumberOfItems, Head(pxList)->pvOwner;

  ensures \valid(\result);
  ensures \result == (struct tskTaskControlBlock *)Head(pxList)->pvOwner;
  ensures Head(pxList) == &\result->xStateListItem;
  ensures TaskItem(&\result->xStateListItem);
  ensures In(&\result->xStateListItem, pxList);
*/
struct tskTaskControlBlock *vTaskListHeadOwner_abs(List_t * const pxList);

#ifdef FREERTOS_USE_ABSTRACT_LIST_MODEL
    #undef listSET_LIST_ITEM_VALUE
    #define listSET_LIST_ITEM_VALUE(pxListItem, xValue) \
        vSchedulerListItemSetValue_abs((pxListItem), (xValue))

    #undef listREMOVE_ITEM
    #define listREMOVE_ITEM(pxItemToRemove) \
        ((void)vSchedulerListRemove_abs((pxItemToRemove)))

    #undef vListInsert
    #define vListInsert(pxList, pxNewListItem) \
        vSchedulerReadyListInsert_abs((pxList), (pxNewListItem))

    /*
     * Temporary proof-performance wrapper.  The typed owner fact is derivable
     * from TaskList(pxList), In(Head(pxList), pxList), and TaskItem(Head(pxList));
     * the generic void * list wrapper discharges the local tick proof with a
     * 30s timeout, but times out at 10s while reconstructing the cast/quantifier
     * chain.  Replace this with a lemma once that derivation is stable enough
     * to keep listGET_OWNER generic at the normal timeout.
     */
    #undef listGET_OWNER_OF_HEAD_ENTRY
    #define listGET_OWNER_OF_HEAD_ENTRY(pxList) \
        vTaskListHeadOwner_abs((pxList))
#endif /* FREERTOS_USE_ABSTRACT_LIST_MODEL */

#endif /* VERIFICATION_FREERTOS_MODEL_SCHEDULER_MODEL_H */
