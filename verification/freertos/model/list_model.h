/*
 * Abstract list model for scheduler-facing proofs.
 *
 * This header is intentionally an ADT boundary.  Scheduler proofs should use
 * ListInv/In/HeadIsMinimum and the *_abs wrappers below instead of reasoning
 * about the concrete pxNext/pxPrevious sentinel representation.
 */

#ifndef VERIFICATION_FREERTOS_MODEL_LIST_MODEL_H
#define VERIFICATION_FREERTOS_MODEL_LIST_MODEL_H

#include "FreeRTOS.h"
#include "list.h"

#ifndef __FRAMAC__
    #error "list_model.h is a Frama-C verification-only model."
#endif

/*@
  axiomatic Abstract_List_Model {
    predicate ListRep{L}(List_t *list)
      reads list->uxNumberOfItems;

    predicate In{L}(ListItem_t *item, List_t *list)
      reads item->pxContainer, list->uxNumberOfItems;

    // Head is abstract.  Mutators that can change the head assign
    // uxNumberOfItems, which is enough to make Head(list) non-stable across
    // the call without exposing pxNext/pxPrevious to scheduler proofs.
    logic ListItem_t *Head{L}(List_t *list)
      reads list->uxNumberOfItems;

    predicate ListInv{L}(List_t *list) =
      \valid(list) && ListRep(list);

    predicate Detached{L}(ListItem_t *item) =
      \valid(item) && item->pxContainer == \null;

    predicate Disjoint{L}(List_t *left, List_t *right) =
      left != right &&
      ListInv(left) &&
      ListInv(right) &&
      \forall ListItem_t *item;
        \valid(item) ==> !(In(item, left) && In(item, right));

    predicate HeadIsMinimum{L}(List_t *list) =
      ListInv(list) &&
      (list->uxNumberOfItems > (UBaseType_t)0 ==>
        \valid(Head(list)) &&
        In(Head(list), list) &&
        \forall ListItem_t *item;
          \valid(item) && In(item, list) ==>
            Head(list)->xItemValue <= item->xItemValue);
  }
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
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) && item != pxListItem ==>
      item->xItemValue == \old(item->xItemValue);
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) ==> item->pvOwner == \old(item->pvOwner);

  ensures \forall List_t *list;
    \valid{Pre}(list) && ListInv{Pre}(list) ==> ListInv(list);
  ensures \forall List_t *list;
    \valid{Pre}(list) && HeadIsMinimum{Pre}(list) ==> HeadIsMinimum(list);
  ensures \forall List_t *list;
    \valid{Pre}(list) ==> Head{Pre}(list) == Head(list);
  ensures \forall List_t *list, ListItem_t *item;
    \valid{Pre}(list) && \valid{Pre}(item) ==>
      (In{Pre}(item, list) <==> In(item, list));
*/
void vListItemSetValue_abs(ListItem_t * const pxListItem,
                           TickType_t xValue);

/*@
  requires ListInv(pxList);
  requires HeadIsMinimum(pxList);
  requires \valid(pxNewListItem);
  requires Detached(pxNewListItem);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns pxList->uxNumberOfItems,
          pxNewListItem->pxContainer;

  ensures ListInv(pxList);
  ensures HeadIsMinimum(pxList);
  ensures In(pxNewListItem, pxList);
  ensures pxNewListItem->pxContainer == pxList;
  ensures pxList->uxNumberOfItems ==
    (UBaseType_t)(\old(pxList->uxNumberOfItems) + 1U);
  ensures pxNewListItem->xItemValue == \old(pxNewListItem->xItemValue);
  ensures pxNewListItem->pvOwner == \old(pxNewListItem->pvOwner);
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) ==> item->xItemValue == \old(item->xItemValue);
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) ==> item->pvOwner == \old(item->pvOwner);

  ensures \forall ListItem_t *item;
    \valid{Pre}(item) && item != pxNewListItem ==>
      (In(item, pxList) <==> In{Pre}(item, pxList));

  ensures \forall List_t *other;
    \valid{Pre}(other) && other != pxList && ListInv{Pre}(other) ==>
      ListInv(other);
  ensures \forall List_t *other;
    \valid{Pre}(other) && other != pxList && HeadIsMinimum{Pre}(other) ==>
      HeadIsMinimum(other);
  ensures \forall List_t *other;
    \valid{Pre}(other) && other != pxList ==>
      Head(other) == Head{Pre}(other);
  ensures \forall List_t *other, ListItem_t *item;
    \valid{Pre}(other) && \valid{Pre}(item) && other != pxList ==>
      (In(item, other) <==> In{Pre}(item, other));
*/
void vListInsertSorted_abs(List_t * const pxList,
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
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) ==> item->xItemValue == \old(item->xItemValue);
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) ==> item->pvOwner == \old(item->pvOwner);
  // The count transition keeps Head unconstrained by its pre-state on the
  // changed list; HeadIsMinimum plus !In below rule out a stale removed head.
  ensures \at(pxItemToRemove->pxContainer, Pre)->uxNumberOfItems ==
    (UBaseType_t)(\old(\at(pxItemToRemove->pxContainer, Pre)->uxNumberOfItems) - 1U);
  ensures \result == \at(pxItemToRemove->pxContainer, Pre)->uxNumberOfItems;
  ensures ListInv(\at(pxItemToRemove->pxContainer, Pre));
  ensures HeadIsMinimum{Pre}(\at(pxItemToRemove->pxContainer, Pre)) ==>
    HeadIsMinimum(\at(pxItemToRemove->pxContainer, Pre));
  ensures !In(pxItemToRemove, \at(pxItemToRemove->pxContainer, Pre));

  ensures \forall ListItem_t *item;
    \valid{Pre}(item) && item != pxItemToRemove ==>
      (In(item, \at(pxItemToRemove->pxContainer, Pre)) <==>
       In{Pre}(item, \at(pxItemToRemove->pxContainer, Pre)));

  ensures \forall List_t *other;
    \valid{Pre}(other) &&
    other != \at(pxItemToRemove->pxContainer, Pre) &&
    ListInv{Pre}(other) ==>
      ListInv(other);
  ensures \forall List_t *other;
    \valid{Pre}(other) &&
    other != \at(pxItemToRemove->pxContainer, Pre) &&
    HeadIsMinimum{Pre}(other) ==>
      HeadIsMinimum(other);
  ensures \forall List_t *other;
    \valid{Pre}(other) &&
    other != \at(pxItemToRemove->pxContainer, Pre) ==>
      Head(other) == Head{Pre}(other);
  ensures \forall List_t *other, ListItem_t *item;
    \valid{Pre}(other) &&
    \valid{Pre}(item) &&
    other != \at(pxItemToRemove->pxContainer, Pre) ==>
      (In(item, other) <==> In{Pre}(item, other));
*/
UBaseType_t vListRemove_abs(ListItem_t * const pxItemToRemove);

/*@
  requires ListInv(pxList);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns \result \from pxList, pxList->uxNumberOfItems;

  ensures \result == pdTRUE || \result == pdFALSE;
  ensures \result == pdTRUE <==> pxList->uxNumberOfItems == (UBaseType_t)0;
  ensures \result == pdTRUE ==>
    \forall ListItem_t *item; \valid(item) ==> !In(item, pxList);
  ensures \result == pdFALSE ==> pxList->uxNumberOfItems > (UBaseType_t)0;
  ensures \result == pdFALSE ==> \valid(Head(pxList));
  ensures \result == pdFALSE ==> In(Head(pxList), pxList);
*/
BaseType_t vListIsEmpty_abs(List_t * const pxList);

/*@
  requires ListInv(pxList);
  requires HeadIsMinimum(pxList);
  requires pxList->uxNumberOfItems > (UBaseType_t)0;
  requires \valid(Head(pxList));

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns \result \from pxList, pxList->uxNumberOfItems, Head(pxList)->pvOwner;

  ensures In(Head(pxList), pxList);
  ensures \result == Head(pxList)->pvOwner;
*/
void *vListHeadOwner_abs(List_t * const pxList);

#ifdef FREERTOS_USE_ABSTRACT_LIST_MODEL
    #undef listSET_LIST_ITEM_VALUE
    #define listSET_LIST_ITEM_VALUE(pxListItem, xValue) \
        vListItemSetValue_abs((pxListItem), (xValue))

    #undef listREMOVE_ITEM
    #define listREMOVE_ITEM(pxItemToRemove) \
        ((void)vListRemove_abs((pxItemToRemove)))

    #undef vListInsert
    #define vListInsert(pxList, pxNewListItem) \
        vListInsertSorted_abs((pxList), (pxNewListItem))

    #undef listLIST_IS_EMPTY
    #define listLIST_IS_EMPTY(pxList) \
        vListIsEmpty_abs((pxList))

    #undef listGET_OWNER_OF_HEAD_ENTRY
    #define listGET_OWNER_OF_HEAD_ENTRY(pxList) \
        vListHeadOwner_abs((pxList))
#endif /* FREERTOS_USE_ABSTRACT_LIST_MODEL */

#endif /* VERIFICATION_FREERTOS_MODEL_LIST_MODEL_H */
