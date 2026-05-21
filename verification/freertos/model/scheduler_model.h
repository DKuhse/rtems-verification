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

  predicate TaskListInsertPre{L}(List_t *list, ListItem_t *item) =
    TaskList(list) &&
    Detached(item) &&
    TaskItem(item);

  predicate ListValueLowerBound{L}(List_t *list, TickType_t bound) =
    \forall ListItem_t *item;
      \valid(item) && In(item, list) ==>
        bound <= item->xItemValue;

  // EDFProperty is intentionally stated over ready-list item values.
  // ReadyListDeadlineMatches is the separate invariant that ties those
  // values back to each task owner's xDeadline.
  predicate EDFProperty{L}(List_t *ready,
                           struct tskTaskControlBlock *running) =
    \valid(running) &&
    In(&running->xStateListItem, ready) &&
    ListValueLowerBound(ready, running->xDeadline);

  predicate ListValueFrame{Before,After}(List_t *list) =
    (\forall ListItem_t *item;
      \valid{After}(item) ==> \valid{Before}(item)) &&
    (\forall ListItem_t *item;
      \valid{After}(item) ==>
        (In{After}(item, list) <==> In{Before}(item, list))) &&
    (\forall ListItem_t *item;
      \valid{After}(item) && In{After}(item, list) ==>
        \at(item->xItemValue, After) ==
          \at(item->xItemValue, Before));

  predicate ListInsertValueFrame{Before,After}(List_t *list,
                                               ListItem_t *inserted) =
    \valid{Before}(inserted) &&
    \valid{After}(inserted) &&
    In{After}(inserted, list) &&
    \at(inserted->xItemValue, After) ==
      \at(inserted->xItemValue, Before) &&
    \at(inserted->pvOwner, After) == \at(inserted->pvOwner, Before) &&
    (\forall ListItem_t *item;
      \valid{After}(item) && item != inserted && In{After}(item, list) ==>
        \valid{Before}(item) &&
        In{Before}(item, list) &&
        \at(item->xItemValue, After) ==
          \at(item->xItemValue, Before));

  // Every list-level predicate is preserved over every list (no list was
  // modified). Used by mutators that only touch detached items.
  predicate ListPredicatesPreserved{Before,After} =
    (\forall List_t *list;
      ListInv{Before}(list) ==> ListInv{After}(list)) &&
    (\forall List_t *list;
      HeadIsMinimum{Before}(list) ==> HeadIsMinimum{After}(list)) &&
    (\forall List_t *list;
      ReadyList{Before}(list) ==> ReadyList{After}(list)) &&
    (\forall List_t *list;
      DelayedList{Before}(list) ==> DelayedList{After}(list)) &&
    (\forall List_t *l, *r;
      Disjoint{Before}(l, r) ==> Disjoint{After}(l, r)) &&
    (\forall List_t *delayed, *ready, *overflowDelayed;
      DelayedTasksHaveValidEventListLinks{Before}(delayed,
                                                  ready,
                                                  overflowDelayed) ==>
        DelayedTasksHaveValidEventListLinks{After}(delayed,
                                                   ready,
                                                   overflowDelayed));

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

  // pxListItem is detached, so no list's membership, count, or contents
  // change — every list-level fact is uniformly framed.
  ensures ListPredicatesPreserved{Pre,Here};
  ensures \forall List_t *list, ListItem_t *item;
    ListInv{Pre}(list) &&
    \valid{Pre}(item) &&
    \valid(item) ==>
      (In(item, list) <==> In{Pre}(item, list));
  ensures \forall List_t *list, ListItem_t *item;
    ListInv{Pre}(list) &&
    \valid{Pre}(item) &&
    In{Pre}(item, list) ==>
      \valid(item) &&
      In(item, list) &&
      item->xItemValue == \at(item->xItemValue, Pre);
  ensures \forall List_t *list, TickType_t bound;
    ListInv{Pre}(list) &&
    ListValueLowerBound{Pre}(list, bound) ==>
      ListValueLowerBound(list, bound);
  ensures \forall List_t *list;
    ReadyList{Pre}(list) &&
    ReadyList(list) ==>
      ListValueFrame{Pre,Here}(list);
  ensures \forall struct tskTaskControlBlock *task;
    \valid{Pre}(task) ==>
      \valid(task) &&
      task->xDeadline == \at(task->xDeadline, Pre);
*/
void vSchedulerListItemSetValue_abs(ListItem_t * const pxListItem,
                                    TickType_t xValue);

/*@
  requires TaskListInsertPre(pxList, pxNewListItem);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns pxList->uxNumberOfItems,
          pxNewListItem->pxContainer;

  ensures TaskList(pxList);
  ensures DelayedList(pxList);
  ensures ReadyList{Pre}(pxList) &&
          ReadyItemDeadlineMatches{Pre}(pxNewListItem) ==>
            ReadyList(pxList);
  ensures In(pxNewListItem, pxList);
  ensures pxNewListItem->pxContainer == pxList;
  ensures pxList->uxNumberOfItems ==
    (UBaseType_t)(\old(pxList->uxNumberOfItems) + 1U);
  ensures pxNewListItem->xItemValue == \old(pxNewListItem->xItemValue);
  ensures pxNewListItem->pvOwner == \old(pxNewListItem->pvOwner);

  ensures \forall ListItem_t *item;
    \valid{Pre}(item) && item != pxNewListItem ==>
      (In(item, pxList) <==> In{Pre}(item, pxList));
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) &&
    In{Pre}(item, pxList) ==>
      \valid(item) &&
      In(item, pxList) &&
      item->xItemValue == \at(item->xItemValue, Pre);
  ensures \forall TickType_t bound;
    ListValueLowerBound{Pre}(pxList, bound) &&
    bound <= \at(pxNewListItem->xItemValue, Pre) ==>
      ListValueLowerBound(pxList, bound);
  ensures \forall List_t *other, ListItem_t *item;
    ListInv{Pre}(other) && other != pxList && \valid{Pre}(item) ==>
      (In(item, other) <==> In{Pre}(item, other));

  ensures \forall List_t *other;
    other != pxList && ListInv{Pre}(other) ==> ListInv(other);
  ensures \forall List_t *other;
    other != pxList && HeadIsMinimum{Pre}(other) ==> HeadIsMinimum(other);
  ensures \forall List_t *other;
    other != pxList && ReadyList{Pre}(other) ==> ReadyList(other);
  ensures \forall List_t *other;
    other != pxList && DelayedList{Pre}(other) ==> DelayedList(other);

  ensures \forall List_t *other;
    other != pxList && Disjoint{Pre}(pxList, other) ==> Disjoint(pxList, other);
  ensures \forall List_t *other;
    other != pxList && Disjoint{Pre}(other, pxList) ==> Disjoint(other, pxList);
  ensures \forall List_t *o1, *o2;
    o1 != pxList && o2 != pxList && Disjoint{Pre}(o1, o2) ==> Disjoint(o1, o2);

  ensures \forall List_t *delayed, *ready, *overflowDelayed;
    delayed != pxList && ready != pxList && overflowDelayed != pxList &&
    DelayedTasksHaveValidEventListLinks{Pre}(delayed,
                                             ready,
                                             overflowDelayed) ==>
      DelayedTasksHaveValidEventListLinks(delayed,
                                          ready,
                                          overflowDelayed);
  ensures \forall List_t *delayed, *overflowDelayed;
    delayed != pxList && overflowDelayed != pxList &&
    DelayedTasksHaveValidEventListLinks{Pre}(delayed,
                                             pxList,
                                             overflowDelayed) ==>
      DelayedTasksHaveValidEventListLinks(delayed,
                                          pxList,
                                          overflowDelayed);
  ensures \forall List_t *ready, *overflowDelayed;
    ready != pxList && overflowDelayed != pxList &&
    DelayedTasksHaveValidEventListLinks{Pre}(pxList,
                                             ready,
                                             overflowDelayed) &&
    TaskEventListLinkValid{Pre}(
      (struct tskTaskControlBlock *)pxNewListItem->pvOwner,
      ready,
      pxList,
      overflowDelayed) ==>
      DelayedTasksHaveValidEventListLinks(pxList,
                                          ready,
                                          overflowDelayed);
  ensures \forall List_t *delayed, *ready;
    delayed != pxList && ready != pxList &&
    DelayedTasksHaveValidEventListLinks{Pre}(delayed,
                                             ready,
                                             pxList) ==>
      DelayedTasksHaveValidEventListLinks(delayed,
                                          ready,
                                          pxList);

  ensures ListInsertValueFrame{Pre,Here}(pxList, pxNewListItem);
  ensures \forall struct tskTaskControlBlock *task;
    \valid{Pre}(task) ==>
      \valid(task) &&
      task->xDeadline == \at(task->xDeadline, Pre);
*/
void vSchedulerListInsert_abs(List_t * const pxList,
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
  ensures TaskItem{Pre}(pxItemToRemove) ==> TaskItem(pxItemToRemove);
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

  // Item-removal is monotone: the modified list only shrinks and the
  // removed item becomes Detached, so every list-level predicate that
  // held before still holds. No conflation here — the universals over
  // arbitrary lists in ListPredicatesPreserved are stating uniform
  // preservation, not a modified-list obligation in disguise.
  ensures ListPredicatesPreserved{Pre,Here};

  ensures \forall List_t *list;
    ReadyList{Pre}(list) &&
    ReadyList(list) &&
    list != \at(pxItemToRemove->pxContainer, Pre) ==>
      ListValueFrame{Pre,Here}(list);
  ensures \forall struct tskTaskControlBlock *task;
    \valid{Pre}(task) ==>
      \valid(task) &&
      task->xDeadline == \at(task->xDeadline, Pre);
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
        vSchedulerListInsert_abs((pxList), (pxNewListItem))

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
