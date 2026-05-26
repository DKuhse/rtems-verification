/*
 * Scheduler-facing list model.
 *
 * Scheduler proofs use ListInv/In/HeadIsMinimum and the *_abs wrappers below.
 * The model keeps scheduler membership tied to the FreeRTOS list item owner
 * field while also exposing a source-shaped traversal predicate over pxNext.
 * Head is the source-level list head entry through xListEnd.pxNext.
 */

#ifndef VERIFICATION_FREERTOS_MODEL_LIST_MODEL_H
#define VERIFICATION_FREERTOS_MODEL_LIST_MODEL_H

#include "FreeRTOS.h"
#include "list.h"

#ifndef __FRAMAC__
    #error "list_model.h is a Frama-C verification-only model."
#endif

/*@
  axiomatic Scheduler_List_Model {
    // we use a traversal based list membership for FreeRTOS.
    // axiomatic logic functions like in RTEMS mapping to a set are nicer
    // however the manual set accesses break them, because Frama-C
    // for some reason doesn't generate frame conditions, i.e.
    // even if lists don't overlap, properties are not preserverd
    // across them

    inductive TraversesFrom{L}(ListItem_t *item,
                               ListItem_t *cursor,
                               List_t *list) {
      case TraversesHere{L}: \forall ListItem_t *item,
                                      ListItem_t *cursor,
                                      List_t *list;
        \valid(list) &&
        \valid(item) &&
        cursor == item ==>
          TraversesFrom(item, cursor, list);

      case TraversesNext{L}: \forall ListItem_t *item,
                                      ListItem_t *cursor,
                                      List_t *list;
        \valid(list) &&
        \valid(cursor) &&
        TraversesFrom(item, cursor->pxNext, list) ==>
          TraversesFrom(item, cursor, list);
    }

    predicate InTraversal{L}(ListItem_t *item, List_t *list) =
      \valid(list) &&
      list->uxNumberOfItems > (UBaseType_t)0 &&
      TraversesFrom(item, list->xListEnd.pxNext, list);

    // Reasoning directly over pxContainer is easier,
    // so we use that as a membership predicate and tie it to the traversal predicate with an invariant.
    predicate In{L}(ListItem_t *item, List_t *list) =
      \valid{L}(item) && item->pxContainer == list;

    // Stuff about lists being well formed

    predicate ListMembershipConsistent{L}(List_t *list) =
      \valid(list) &&
      \forall ListItem_t *item;
        \valid(item) ==>
          (In(item, list) <==> InTraversal(item, list));

    logic ListItem_t *Head{L}(List_t *list)
      reads list->xListEnd.pxNext;

    axiom HeadIsListEndNext{L}: \forall List_t *list;
      \valid(list) ==> Head(list) == list->xListEnd.pxNext;

    predicate ContainerMembershipConsistent{L}(List_t *list) =
      \valid(list) &&
      \forall ListItem_t *item;
        \valid(item) ==>
          (In(item, list) <==> item->pxContainer == list);

    predicate ListItemFieldsSeparated{L}(ListItem_t *item) =
      \valid(item) &&
      \separated(&item->xItemValue,
                 &item->pvOwner,
                 &item->pxContainer);

    predicate ListItemStorageSeparatedFromList{L}(List_t *list,
                                                  ListItem_t *item) =
      \valid(list) &&
      ListItemFieldsSeparated(item) &&
      \separated(&list->uxNumberOfItems,
                 &item->xItemValue,
                 &item->pvOwner,
                 &item->pxContainer);

    predicate ListItemFieldsSeparatedFromValueWrite{L}(
        ListItem_t *writtenItem,
        ListItem_t *item) =
      \valid(writtenItem) &&
      ListItemFieldsSeparated(item) &&
      \separated(&writtenItem->xItemValue,
                 &item->xItemValue,
                 &item->pvOwner);

    predicate ListItemFieldsSeparatedFromListMutation{L}(
        List_t *list,
        ListItem_t *changedItem,
        ListItem_t *item) =
      \valid(list) &&
      \valid(changedItem) &&
      ListItemFieldsSeparated(item) &&
      \separated(&list->uxNumberOfItems,
                 &changedItem->pxContainer,
                 &item->xItemValue,
                 &item->pvOwner);

    predicate ListStorageSeparated{L}(List_t *list) =
      \valid(list) &&
      \forall ListItem_t *item;
        \valid(item) && In(item, list) ==>
          ListItemStorageSeparatedFromList(list, item);

    predicate ListMembershipCountPositive{L}(List_t *list) =
      \valid(list) &&
      \forall ListItem_t *item;
        \valid(item) && In(item, list) ==>
          list->uxNumberOfItems > (UBaseType_t)0;

    predicate ListInv{L}(List_t *list) =
      \valid(list) &&
      ContainerMembershipConsistent(list) &&
      ListMembershipConsistent(list) &&
      ListStorageSeparated(list) &&
      ListMembershipCountPositive(list);

    // Useful properties about lists

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

/* Mutating list wrappers are intentionally specified in scheduler_model.h.
 * They preserve scheduler-level predicates directly instead of forcing the
 * tick proof to rebuild those predicates from item-field frame facts. 
 * Here we only specify the non-mutating list queries which are generic.
 */

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
    #undef listLIST_IS_EMPTY
    #define listLIST_IS_EMPTY(pxList) \
        vListIsEmpty_abs((pxList))

    #undef listGET_OWNER_OF_HEAD_ENTRY
    #define listGET_OWNER_OF_HEAD_ENTRY(pxList) \
        vListHeadOwner_abs((pxList))
#endif /* FREERTOS_USE_ABSTRACT_LIST_MODEL */

#endif /* VERIFICATION_FREERTOS_MODEL_LIST_MODEL_H */
