/*
 * Verification overlay (reference) for xTaskIncrementTick.
 *
 * Standalone minimal extraction, mirroring reference/taskswitchcontext.c
 * but for a more complex function. The body is taken verbatim from
 * tasks.c:4391-4602, with surrounding scaffolding kept minimal.
 *
 * Assumed configuration (matches FreeRTOSConfig.h):
 *   configNUMBER_OF_CORES           == 1
 *   configUSE_PREEMPTION            == 1
 *   configUSE_TIME_SLICING          == 0
 *   configUSE_TICK_HOOK             == 0
 *   configGENERATE_RUN_TIME_STATS   == 0
 *   EDF_SCHEDULER                   == 1
 *
 * Under these flags a lot of the function body preprocesses out;
 * what's left is the tick-advance, the delayed-list drain, the EDF
 * preemption check, and the pending-yield check.
 *
 * Contract scope: the SUSPENDED branch is fully specified and meant
 * to prove. The RUNNING branch is left structural-only on purpose —
 * proving the substantive EDF claim there involves loop invariants
 * and multi-list reasoning, which is the next layer.
 */

#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "FreeRTOS.h"
#include "list.h"
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* Minimal TCB: needs xDeadline plus the two ListItem_t fields the
 * function dereferences. */
struct tskTaskControlBlock {
    ListItem_t  xStateListItem;
    ListItem_t  xEventListItem;
    TickType_t  xDeadline;
};
typedef struct tskTaskControlBlock TCB_t;

/* edf.h dereferences ->xDeadline; included after the TCB struct is
 * complete. */
#include "edf.h"

/* Hack: Frama-C can't handle volatile */
#ifdef __FRAMAC__
    TCB_t *           pxCurrentTCB;
    List_t            xReadyTasksList;
    UBaseType_t       uxSchedulerSuspended;
    TickType_t        xTickCount;
    TickType_t        xNextTaskUnblockTime;
    TickType_t        xPendedTicks;
    BaseType_t        xYieldPendings[1];
    BaseType_t        xNumOfOverflows;
    List_t            xDelayedTaskList1;
    List_t            xDelayedTaskList2;
    List_t *          pxDelayedTaskList;
    List_t *          pxOverflowDelayedTaskList;
#else
    volatile TCB_t *  pxCurrentTCB;
    List_t            xReadyTasksList;
    volatile UBaseType_t uxSchedulerSuspended;
    volatile TickType_t  xTickCount;
    volatile TickType_t  xNextTaskUnblockTime;
    volatile TickType_t  xPendedTicks;
    volatile BaseType_t  xYieldPendings[1];
    volatile BaseType_t  xNumOfOverflows;
    List_t            xDelayedTaskList1;
    List_t            xDelayedTaskList2;
    List_t * volatile pxDelayedTaskList;
    List_t * volatile pxOverflowDelayedTaskList;
#endif

/*@
  // Owner well-formedness.
  predicate well_formed_item_owner(struct xLIST_ITEM *i) =
    i->pvOwner != \null &&
    \valid((TCB_t *)(i->pvOwner)) &&
    &((TCB_t *)(i->pvOwner))->xStateListItem == i;

  // List-level owner-back-link consistency. Structural soundness lives
  // separately in valid_list_model (list.h).x

  predicate well_formed_list(struct xLIST *L) =
    \valid(L) &&
    (\forall struct xLIST_ITEM *i;
      \valid(i) && in_list(i, L) ==> well_formed_item_owner(i));

  predicate task_list_model(struct xLIST *L) =
    valid_list_model(L) &&
    well_formed_list(L) &&
    sorted(L);

  predicate ready_list_model(struct xLIST *L) =
    task_list_model(L) &&
    xItemValue_matches_deadline(L);

  predicate delayed_list_model(struct xLIST *L) =
    task_list_model(L);

  predicate tick_lists_model(struct xLIST *ready,
                             struct xLIST *delayed,
                             struct xLIST *overflow) =
    ready != delayed &&
    ready != overflow &&
    delayed != overflow &&
    ready_list_model(ready) &&
    delayed_list_model(delayed) &&
    delayed_list_model(overflow) &&
    disjoint_lists(delayed, ready) &&
    disjoint_lists(overflow, ready) &&
    disjoint_lists(delayed, overflow);

  predicate state_item_value_separated_from_ready_deadlines(TCB_t *t,
                                                            struct xLIST *ready) =
    \valid(t) &&
    (\forall struct xLIST_ITEM *i;
      \valid(i) && in_list(i, ready) ==>
        \valid((TCB_t *)i->pvOwner) &&
        \separated(&(t->xStateListItem.xItemValue),
                   &((TCB_t *)i->pvOwner)->xDeadline));

  predicate insert_ready_frame_separated(TCB_t *t) =
    \valid(t) &&
    \separated(
      &pxCurrentTCB, &pxDelayedTaskList, &pxOverflowDelayedTaskList,
      &(t->xStateListItem.xItemValue),
      &(t->xStateListItem.pxNext),
      &(t->xStateListItem.pxPrevious),
      &(t->xStateListItem.pxContainer)
    ) &&
    \separated(
      t + (..),
      &pxCurrentTCB, &pxDelayedTaskList, &pxOverflowDelayedTaskList
    ) &&
    \separated(
      &pxCurrentTCB, &pxDelayedTaskList, &pxOverflowDelayedTaskList,
      { &item->pxNext | struct xLIST_ITEM *item; \valid(item) },
      { &item->pxPrevious | struct xLIST_ITEM *item; \valid(item) }
    );
*/


/* prvResetNextTaskUnblockTime is a static helper in tasks.c that
 * scans the delayed list and updates xNextTaskUnblockTime. We model
 * it as opaque — its exact effect doesn't matter for the
 * suspended-branch proof. */
/*@
  assigns xNextTaskUnblockTime;
*/
static void prvResetNextTaskUnblockTime(void);

/* Verification-only wrapper around the macro listGET_OWNER_OF_HEAD_ENTRY. */
/*@
  requires \valid(pxList);
  requires pxList->uxNumberOfItems != (UBaseType_t)0;
  requires well_formed_list(pxList);

  assigns \nothing;

  ensures \valid(\result);
  ensures well_formed_item_owner(&\result->xStateListItem);
  ensures \result->xStateListItem.pxContainer == pxList;
*/
TCB_t * prvGetOwnerOfHeadEntry(List_t * pxList);

/* Verification override of listGET_OWNER_OF_HEAD_ENTRY: route the
 * macro through prvGetOwnerOfHeadEntry so its contract applies at
 * every call site. Semantically equivalent to the production macro. */
#ifdef __FRAMAC__
    #undef  listGET_OWNER_OF_HEAD_ENTRY
    #define listGET_OWNER_OF_HEAD_ENTRY( pxList )    prvGetOwnerOfHeadEntry( pxList )
#endif

/* From tasks.c:271. */
#define taskSWITCH_DELAYED_LISTS()                                          \
    do {                                                                    \
        List_t *pxTemp;                                                     \
        pxTemp = pxDelayedTaskList;                                         \
        pxDelayedTaskList = pxOverflowDelayedTaskList;                      \
        pxOverflowDelayedTaskList = pxTemp;                                 \
        xNumOfOverflows = (BaseType_t)(xNumOfOverflows + 1);                \
        prvResetNextTaskUnblockTime();                                      \
    } while (0)

/* From tasks.c:294 — EDF version of prvAddTaskToReadyList. */
#define prvAddTaskToReadyList(pxTCB)                                        \
    do {                                                                    \
        listSET_LIST_ITEM_VALUE(&(pxTCB->xStateListItem), (pxTCB)->xDeadline); \
        vListInsert(&(xReadyTasksList), &((pxTCB)->xStateListItem));        \
    } while (0)

/*@
  requires \valid(pxTCB);
  requires well_formed_item_owner(&pxTCB->xStateListItem);
  requires pxTCB->xStateListItem.pxContainer == pxDelayedTaskList;
  requires pxTCB->xEventListItem.pxContainer == \null;
  requires tick_lists_model(&xReadyTasksList, pxDelayedTaskList, pxOverflowDelayedTaskList);
  requires state_item_value_separated_from_ready_deadlines(pxTCB, &xReadyTasksList);
  requires \separated(pxTCB + (..), &pxCurrentTCB, &pxDelayedTaskList, &pxOverflowDelayedTaskList);

  assigns pxTCB->xStateListItem.pxContainer,
          pxDelayedTaskList->uxNumberOfItems,
          pxDelayedTaskList->pxIndex,
          pxTCB->xStateListItem.pxNext->pxPrevious,
          pxTCB->xStateListItem.pxPrevious->pxNext;

  ensures \valid(pxTCB);
  ensures pxTCB->xStateListItem.pxContainer == \null;
  ensures pxTCB->xEventListItem.pxContainer == \null;
  ensures well_formed_item_owner(&pxTCB->xStateListItem);
  ensures ready_list_model(&xReadyTasksList);
  ensures delayed_list_model(pxDelayedTaskList);
  ensures delayed_list_model(pxOverflowDelayedTaskList);
  ensures !in_list(&pxTCB->xStateListItem, &xReadyTasksList);
  ensures disjoint_lists(pxDelayedTaskList, &xReadyTasksList);
  ensures disjoint_lists(pxOverflowDelayedTaskList, &xReadyTasksList);
  ensures disjoint_lists(pxDelayedTaskList, pxOverflowDelayedTaskList);
  ensures state_item_value_separated_from_ready_deadlines(pxTCB, &xReadyTasksList);
  ensures \separated(pxTCB + (..), &pxCurrentTCB, &pxDelayedTaskList, &pxOverflowDelayedTaskList);
*/
static void prvDetachUnblockedTaskFromDelayedList(TCB_t *pxTCB) {
    listREMOVE_ITEM(&(pxTCB->xStateListItem));
}

/*@
  requires \valid(pxTCB);
  requires \valid(pxCurrentTCB);
  requires well_formed_item_owner(&pxTCB->xStateListItem);
  requires pxTCB->xStateListItem.pxContainer == \null;
  requires tick_lists_model(&xReadyTasksList, pxDelayedTaskList, pxOverflowDelayedTaskList);
  requires !in_list(&pxTCB->xStateListItem, &xReadyTasksList);
  requires state_item_value_separated_from_ready_deadlines(pxTCB, &xReadyTasksList);
  requires insert_ready_frame_separated(pxTCB);

  assigns pxTCB->xStateListItem.xItemValue,
          xReadyTasksList.uxNumberOfItems,
          pxTCB->xStateListItem.pxNext,
          pxTCB->xStateListItem.pxPrevious,
          pxTCB->xStateListItem.pxContainer,
          { item->pxNext     | struct xLIST_ITEM *item; \valid(item) },
          { item->pxPrevious | struct xLIST_ITEM *item; \valid(item) };

  ensures \valid(pxTCB);
  ensures \valid(pxCurrentTCB);
  ensures pxCurrentTCB == \old(pxCurrentTCB);
  ensures tick_lists_model(&xReadyTasksList, pxDelayedTaskList, pxOverflowDelayedTaskList);
*/
static void prvInsertUnblockedTaskIntoReadyList(TCB_t *pxTCB) {
BeforeSetValue:
    //@ assert xItemValue_matches_deadline(&xReadyTasksList);
    listSET_LIST_ITEM_VALUE(&(pxTCB->xStateListItem), pxTCB->xDeadline);
    //@ assert pxTCB->xStateListItem.pxContainer == \null;
    //@ assert pxTCB->xStateListItem.pvOwner == pxTCB;
    //@ assert well_formed_item_owner(&pxTCB->xStateListItem);
    //@ assert pxTCB->xStateListItem.xItemValue == ((TCB_t *)pxTCB->xStateListItem.pvOwner)->xDeadline;
    //@ assert !in_list(&pxTCB->xStateListItem, &xReadyTasksList);
    /*@ assert \forall struct xLIST_ITEM *i;
          in_list(i, &xReadyTasksList) <==> in_list{BeforeSetValue}(i, &xReadyTasksList);
    */
    /*@ assert \forall struct xLIST_ITEM *i;
          in_list(i, &xReadyTasksList) ==> i != &pxTCB->xStateListItem;
    */
    /*@ assert \forall struct xLIST_ITEM *i;
          \valid(i) &&
          in_list(i, &xReadyTasksList) ==>
            well_formed_item_owner(i);
    */
    /*@ assert \forall struct xLIST_ITEM *i;
          \valid(i) &&
          in_list(i, &xReadyTasksList) ==>
            i->pvOwner != pxTCB;
    */
    /*@ assert \forall struct xLIST_ITEM *i;
          \valid(i) &&
          in_list(i, &xReadyTasksList) ==>
            \separated(&(pxTCB->xStateListItem.xItemValue),
                       &((TCB_t *)i->pvOwner)->xDeadline);
    */
    /*@ assert \forall struct xLIST_ITEM *i;
          \valid(i) &&
          in_list(i, &xReadyTasksList) ==>
            i->xItemValue == ((TCB_t *)i->pvOwner)->xDeadline;
    */
    //@ assert disjoint_lists(&xReadyTasksList, pxDelayedTaskList);
    //@ assert disjoint_lists(&xReadyTasksList, pxOverflowDelayedTaskList);
BeforeReadyInsert:
    vListInsert(&(xReadyTasksList), &(pxTCB->xStateListItem));
    //@ assert in_list(&pxTCB->xStateListItem, &xReadyTasksList);
    //@ assert well_formed_item_owner(&pxTCB->xStateListItem);
    //@ assert pxTCB->xStateListItem.xItemValue == ((TCB_t *)pxTCB->xStateListItem.pvOwner)->xDeadline;
    /*@ assert \forall struct xLIST_ITEM *i;
          i != &pxTCB->xStateListItem ==>
            (in_list(i, &xReadyTasksList) <==> in_list{BeforeReadyInsert}(i, &xReadyTasksList));
    */
    /*@ assert \forall struct xLIST_ITEM *i;
          in_list(i, &xReadyTasksList) &&
          i != &pxTCB->xStateListItem ==>
            in_list{BeforeSetValue}(i, &xReadyTasksList);
    */
    /*@ assert \forall struct xLIST_ITEM *i;
          in_list{BeforeReadyInsert}(i, &xReadyTasksList) ==>
            i->xItemValue == ((TCB_t *)i->pvOwner)->xDeadline;
    */
    /*@ assert \forall struct xLIST_ITEM *i;
          \valid(i) &&
          in_list(i, &xReadyTasksList) &&
          i != &pxTCB->xStateListItem ==>
            well_formed_item_owner(i);
    */
    /*@ assert \forall struct xLIST_ITEM *i;
          in_list(i, &xReadyTasksList) &&
          i != &pxTCB->xStateListItem ==>
            i->xItemValue == ((TCB_t *)i->pvOwner)->xDeadline;
    */
}

/* Per-iteration body of the unblock loop, factored out so the substantive
 * proof obligation lives in a single helper contract instead of being
 * spread across the loop's invariants. The caller has already established
 * that pxTCB is at the head of pxDelayedTaskList and its deadline is
 * past — the helper just performs the four mutations (remove from state
 * list, optional event-list remove, add to ready list, optional
 * preemption-flag bump) and gives back the updated xSwitchRequired. */
/*@
  requires \valid(pxTCB);
  requires \valid(pxCurrentTCB);
  requires well_formed_item_owner(&pxTCB->xStateListItem);
  requires pxTCB->xStateListItem.pxContainer == pxDelayedTaskList;

  // simplificaiton: event lists out of scope (for now? TODO)
  requires pxTCB->xEventListItem.pxContainer == \null;

  requires tick_lists_model(&xReadyTasksList, pxDelayedTaskList, pxOverflowDelayedTaskList);
  requires state_item_value_separated_from_ready_deadlines(pxTCB, &xReadyTasksList);
  requires insert_ready_frame_separated(pxTCB);

  assigns pxTCB->xStateListItem.pxContainer,
          pxDelayedTaskList->uxNumberOfItems,
          pxDelayedTaskList->pxIndex,
          pxTCB->xStateListItem.pxNext->pxPrevious,
          pxTCB->xStateListItem.pxPrevious->pxNext,
          pxTCB->xStateListItem.xItemValue,
          xReadyTasksList.uxNumberOfItems,
          pxTCB->xStateListItem.pxNext,
          pxTCB->xStateListItem.pxPrevious,
          { item->pxNext     | struct xLIST_ITEM *item; \valid(item) },
          { item->pxPrevious | struct xLIST_ITEM *item; \valid(item) };

  ensures \valid(pxTCB);
  ensures \valid(pxCurrentTCB);
  ensures pxCurrentTCB == \old(pxCurrentTCB);
  ensures ready_list_model(&xReadyTasksList);
  ensures delayed_list_model(pxDelayedTaskList);
  ensures delayed_list_model(pxOverflowDelayedTaskList);
  ensures disjoint_lists(pxDelayedTaskList, &xReadyTasksList);
  ensures disjoint_lists(pxOverflowDelayedTaskList, &xReadyTasksList);
  ensures disjoint_lists(pxDelayedTaskList, pxOverflowDelayedTaskList);
  ensures tick_lists_model(&xReadyTasksList, pxDelayedTaskList, pxOverflowDelayedTaskList);

*/
static BaseType_t prvProcessUnblockedTask(TCB_t *pxTCB,
                                          BaseType_t xSwitchRequired) {
    prvDetachUnblockedTaskFromDelayedList(pxTCB);

    if (listLIST_ITEM_CONTAINER(&(pxTCB->xEventListItem)) != NULL) {
        listREMOVE_ITEM(&(pxTCB->xEventListItem));
    } else {
        mtCOVERAGE_TEST_MARKER();
    }

    prvInsertUnblockedTaskIntoReadyList(pxTCB);
#ifdef SANITY_PROBE
    /* Sanity probe — must NOT prove. Checks that the hypothesis set
     * inside the unblock helper is not vacuous. Enabled only by
     * sanity-check.sh. */
    //@ assert \false;
#endif

#if (configUSE_PREEMPTION == 1)
    {
        if (pxTCB->xDeadline < pxCurrentTCB->xDeadline) {
            xSwitchRequired = pdTRUE;
        } else {
            mtCOVERAGE_TEST_MARKER();
        }
    }
#endif /* configUSE_PREEMPTION */

    return xSwitchRequired;
}

/*@
  behavior suspended:
    assumes uxSchedulerSuspended != (UBaseType_t)0U;
    assigns xPendedTicks;
    // casting to account for wrapping
    ensures xPendedTicks == (TickType_t)( \old(xPendedTicks) + 1U );
    ensures \result == pdFALSE;
    ensures pxCurrentTCB == \old(pxCurrentTCB);
    ensures xTickCount == \old(xTickCount);

  behavior running:
    assumes uxSchedulerSuspended == (UBaseType_t)0U;
    requires \valid(pxCurrentTCB);
    requires \forall TCB_t *t; \valid(t) ==> insert_ready_frame_separated(t);

    requires tick_lists_model(&xReadyTasksList, pxDelayedTaskList, pxOverflowDelayedTaskList);

    ensures pxCurrentTCB == \old(pxCurrentTCB);
    ensures sorted(&xReadyTasksList);
    // Tick advances by 1, modulo wrap.
    ensures xTickCount == (TickType_t)( \old(xTickCount) + 1U );

    // ensures \result == pdTRUE || edf_property(&xReadyTasksList, pxCurrentTCB);

  complete behaviors suspended, running;
  disjoint behaviors suspended, running;
*/
BaseType_t xTaskIncrementTick(void) {
    TCB_t* pxTCB;
    TickType_t xItemValue;
    BaseType_t xSwitchRequired = pdFALSE;

    traceENTER_xTaskIncrementTick();

    /* Called by the portable layer each time a tick interrupt occurs.
     * Increments the tick then checks to see if the new tick value will cause any
     * tasks to be unblocked. */
    traceTASK_INCREMENT_TICK(xTickCount);

    if (uxSchedulerSuspended == (UBaseType_t)0U) {
        const TickType_t xConstTickCount = xTickCount + (TickType_t)1;

        xTickCount = xConstTickCount;

        if (xConstTickCount == (TickType_t)0U) {
            taskSWITCH_DELAYED_LISTS();
        } else {
            mtCOVERAGE_TEST_MARKER();
        }

        if (xConstTickCount >= xNextTaskUnblockTime) {
            /*@
                loop invariant pxCurrentTCB == \at(pxCurrentTCB, Pre);
                loop invariant \valid(pxCurrentTCB);

                loop invariant \valid(pxDelayedTaskList);
                loop invariant ready_list_model(&xReadyTasksList);
                loop invariant delayed_list_model(pxDelayedTaskList);
                loop invariant disjoint_lists(pxDelayedTaskList, &xReadyTasksList);

                loop invariant xSwitchRequired == pdTRUE || xSwitchRequired == pdFALSE;
                loop invariant xSwitchRequired == pdTRUE ||
                    edf_property(&xReadyTasksList, pxCurrentTCB);

                loop invariant xTickCount == (TickType_t)(\at(xTickCount, Pre) + 1U);
            */
            for (;;) {
                if (listLIST_IS_EMPTY(pxDelayedTaskList) != pdFALSE) {
                    xNextTaskUnblockTime = portMAX_DELAY;
                    break;
                } else {
                    /* MISRA Ref 11.5.3 [Void pointer assignment] */
                    /* coverity[misra_c_2012_rule_11_5_violation] */
                    pxTCB = listGET_OWNER_OF_HEAD_ENTRY(pxDelayedTaskList);
                    xItemValue = listGET_LIST_ITEM_VALUE(&(pxTCB->xStateListItem));

                    if (xConstTickCount < xItemValue) {
                        xNextTaskUnblockTime = xItemValue;
                        break;
                    } else {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    xSwitchRequired = prvProcessUnblockedTask(pxTCB, xSwitchRequired);
                }
            }
        }

#ifdef SANITY_PROBE
        /* Sanity probe — must NOT prove. Checks that the hypothesis set
         * at the function level (after the unblock loop, before the
         * yield-pending check) is not vacuous. Catches conflicts in
         * xTaskIncrementTick's own contract that the helper's probe
         * wouldn't see. */
        //@ assert \false;
#endif

#if (configUSE_PREEMPTION == 1)
        {
            if (xYieldPendings[0] != pdFALSE) {
                xSwitchRequired = pdTRUE;
            } else {
                mtCOVERAGE_TEST_MARKER();
            }
        }
#endif /* configUSE_PREEMPTION */
    } else {
        xPendedTicks += 1U;
    }

    traceRETURN_xTaskIncrementTick(xSwitchRequired);

    return xSwitchRequired;
}
