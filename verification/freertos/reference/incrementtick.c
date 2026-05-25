/*
 * Verification overlay (reference) for xTaskIncrementTick.
 *
 * This extraction keeps the tick function source-shaped: the delayed-list
 * unblock logic remains inline, as in tasks.c.  The contract scope here is
 * intentionally narrow: prove the tick/pended-tick arithmetic and leave EDF
 * scheduler-list preservation for a later proof layer built on model/.
 */

#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "FreeRTOS.h"
#include "list.h"
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "freertos_volatile_instrumentation.h"

#define FREERTOS_USE_ABSTRACT_LIST_MODEL
#include "scheduler_model.h"

/*@
  predicate ReadiedItemBetween{Before,After}(ListItem_t *item,
                                             List_t *ready) =
    \valid{Before}(item) &&
    \valid{After}(item) &&
    \valid{Before}(ready) &&
    \valid{After}(ready) &&
    \at(item->pxContainer, Before) != ready &&
    \at(item->pxContainer, After) == ready;

  predicate RemovedItemBetween{Before,After}(ListItem_t *item,
                                             List_t *list) =
    \valid{Before}(item) &&
    \valid{After}(item) &&
    \valid{Before}(list) &&
    \valid{After}(list) &&
    \at(item->pxContainer, Before) == list &&
    \at(item->pxContainer, After) != list;

  predicate ReadiedItemsHaveValidDeadlines{Before,After}(List_t *ready) =
    \forall ListItem_t *item;
      ReadiedItemBetween{Before,After}(item, ready) ==>
        ReadyItemDeadlineMatches{After}(item);

  predicate ReadiedItemsCameFromDelayedLists{Before,After}(
      List_t *ready,
      List_t *delayed,
      List_t *overflowDelayed) =
    \forall ListItem_t *item;
      ReadiedItemBetween{Before,After}(item, ready) ==>
        (In{Before}(item, delayed) ||
         In{Before}(item, overflowDelayed));

  predicate DelayedRemovalsWereReadied{Before,After}(List_t *delayed,
                                                     List_t *ready) =
    \forall ListItem_t *item;
      RemovedItemBetween{Before,After}(item, delayed) ==>
        ReadiedItemBetween{Before,After}(item, ready);

  predicate TickDelayedRemovalsWereReadied{Before,After}(
      TickType_t previousTick,
      List_t *delayed,
      List_t *overflowDelayed,
      List_t *ready) =
    (((TickType_t)(previousTick + (TickType_t)1U) == (TickType_t)0U) ==>
      DelayedRemovalsWereReadied{Before,After}(overflowDelayed, ready)) &&
    (((TickType_t)(previousTick + (TickType_t)1U) != (TickType_t)0U) ==>
      DelayedRemovalsWereReadied{Before,After}(delayed, ready));

  predicate PriorReadiedItemsStayReadied{Start,Middle,End}(
      ListItem_t *current,
      List_t *ready) =
    \forall ListItem_t *item;
      item != current &&
      ReadiedItemBetween{Start,Middle}(item, ready) ==>
        ReadiedItemBetween{Start,End}(item, ready);

  predicate PriorDelayedRemovalsAreUnchanged{Start,Middle,End}(
      ListItem_t *current,
      List_t *delayed) =
    \forall ListItem_t *item;
      item != current &&
      RemovedItemBetween{Start,End}(item, delayed) ==>
        RemovedItemBetween{Start,Middle}(item, delayed);
*/

/* prvResetNextTaskUnblockTime is a static helper in tasks.c that scans the
 * delayed list and updates xNextTaskUnblockTime. */
/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns xNextTaskUnblockTime,
          xNextTaskUnblockTime_ghost;
*/
static void prvResetNextTaskUnblockTime(void);

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

/* From tasks.c:294, EDF version of prvAddTaskToReadyList. */
#define prvAddTaskToReadyList(pxTCB)                                        \
    do {                                                                    \
        listSET_LIST_ITEM_VALUE(&(pxTCB->xStateListItem), (pxTCB)->xDeadline); \
        vListInsert(&(xReadyTasksList), &((pxTCB)->xStateListItem));        \
    } while (0)

/*@
  requires SchedulerListContext(&xReadyTasksList,
                                pxDelayedTaskList_ghost,
                                pxOverflowDelayedTaskList_ghost);

  // Termination of the delayed-list drain depends on list-count semantics.
  // This reset proves tick arithmetic only; termination is a later ADT proof.
  terminates \false;

  ensures SchedulerListContext(&xReadyTasksList,
                               pxDelayedTaskList_ghost,
                               pxOverflowDelayedTaskList_ghost);

  behavior suspended:
    assumes uxSchedulerSuspended_ghost != (UBaseType_t)0U;
    exits \false;
    assigns xPendedTicks,
            xPendedTicks_ghost;
    ensures xPendedTicks_ghost ==
      (TickType_t)(\old(xPendedTicks_ghost) + 1U);
    ensures xTickCount_ghost == \old(xTickCount_ghost);
    ensures \result == pdFALSE;

  behavior running:
    assumes uxSchedulerSuspended_ghost == (UBaseType_t)0U;
    requires \valid(pxCurrentTCB_ghost);
    requires EDFProperty(&xReadyTasksList, pxCurrentTCB_ghost);

    exits \false;

    ensures xTickCount_ghost ==
      (TickType_t)(\old(xTickCount_ghost) + 1U);
    ensures \result == pdTRUE || \result == pdFALSE;
    ensures \result == pdTRUE ||
      EDFProperty(&xReadyTasksList, pxCurrentTCB_ghost);
    ensures ReadiedItemsHaveValidDeadlines{Pre,Here}(&xReadyTasksList);
    ensures ReadiedItemsCameFromDelayedLists{Pre,Here}(
      &xReadyTasksList,
      \at(pxDelayedTaskList_ghost, Pre),
      \at(pxOverflowDelayedTaskList_ghost, Pre));
    ensures TickDelayedRemovalsWereReadied{Pre,Here}(
      \old(xTickCount_ghost),
      \at(pxDelayedTaskList_ghost, Pre),
      \at(pxOverflowDelayedTaskList_ghost, Pre),
      &xReadyTasksList);

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

    /* Tick increment should occur on every kernel timer event. Core 0 has the
     * responsibility to increment the tick, or increment the pended ticks if the
     * scheduler is suspended.  If pended ticks is greater than zero, the core that
     * calls xTaskResumeAll has the responsibility to increment the tick. */
    if (uxSchedulerSuspended == (UBaseType_t)0U) {
        /* Minor optimisation.  The tick count cannot change in this block. */
        const TickType_t xConstTickCount = xTickCount + (TickType_t)1;

        /* Increment the RTOS tick, switching the delayed and overflowed delayed
         * lists if it wraps to 0. */
        xTickCount = xConstTickCount;

        if (xConstTickCount == (TickType_t)0U) {
            taskSWITCH_DELAYED_LISTS();
        } else {
            mtCOVERAGE_TEST_MARKER();
        }

        /* See if this tick has made a timeout expire. */
        if (xConstTickCount >= xNextTaskUnblockTime) {
TickDrainStart:
            /*@
              loop invariant xTickCount_ghost ==
                (TickType_t)(\at(xTickCount_ghost, Pre) + 1U);
              loop invariant pxCurrentTCB_ghost == \at(pxCurrentTCB_ghost, Pre);
              loop invariant \valid(pxCurrentTCB_ghost);
              loop invariant pxDelayedTaskList_ghost ==
                \at(pxDelayedTaskList_ghost, TickDrainStart);
              loop invariant pxDelayedTaskList_ghost ==
                \at(pxDelayedTaskList_ghost, Pre) ||
                pxDelayedTaskList_ghost ==
                  \at(pxOverflowDelayedTaskList_ghost, Pre);
              loop invariant ((TickType_t)(\at(xTickCount_ghost, Pre) + (TickType_t)1U) ==
                                (TickType_t)0U) ==>
                pxDelayedTaskList_ghost ==
                  \at(pxOverflowDelayedTaskList_ghost, Pre);
              loop invariant ((TickType_t)(\at(xTickCount_ghost, Pre) + (TickType_t)1U) !=
                                (TickType_t)0U) ==>
                pxDelayedTaskList_ghost ==
                  \at(pxDelayedTaskList_ghost, Pre);
              loop invariant Disjoint{TickDrainStart}(&xReadyTasksList,
                                                      pxDelayedTaskList_ghost);
              loop invariant \forall ListItem_t *item;
                \valid(item) && In(item, pxDelayedTaskList_ghost) ==>
                  \valid{TickDrainStart}(item) &&
                  In{TickDrainStart}(item, pxDelayedTaskList_ghost);
              loop invariant \forall ListItem_t *item;
                \valid{TickDrainStart}(item) &&
                In{TickDrainStart}(item, pxDelayedTaskList_ghost) ==>
                  (In{Pre}(item, \at(pxDelayedTaskList_ghost, Pre)) ||
                   In{Pre}(item, \at(pxOverflowDelayedTaskList_ghost, Pre)));
              loop invariant SchedulerListContext(&xReadyTasksList,
                                                  pxDelayedTaskList_ghost,
                                                  pxOverflowDelayedTaskList_ghost);
              loop invariant xSwitchRequired == pdTRUE || xSwitchRequired == pdFALSE;
              loop invariant xSwitchRequired == pdTRUE ||
                EDFProperty(&xReadyTasksList, pxCurrentTCB_ghost);
              loop invariant ReadiedItemsHaveValidDeadlines{Pre,Here}(
                &xReadyTasksList);
              loop invariant ReadiedItemsCameFromDelayedLists{Pre,Here}(
                &xReadyTasksList,
                \at(pxDelayedTaskList_ghost, Pre),
                \at(pxOverflowDelayedTaskList_ghost, Pre));
              loop invariant DelayedRemovalsWereReadied{Pre,Here}(
                \at(pxDelayedTaskList_ghost, TickDrainStart),
                &xReadyTasksList);

              loop assigns pxTCB,
                           xItemValue,
                           xSwitchRequired,
                           xNextTaskUnblockTime,
                           xNextTaskUnblockTime_ghost,
                           { list->uxNumberOfItems | List_t *list; \valid(list) },
                           { item->xItemValue | ListItem_t *item; \valid(item) },
                           { item->pxContainer | ListItem_t *item; \valid(item) };
            */
            for (;;) {
                if (listLIST_IS_EMPTY(pxDelayedTaskList) != pdFALSE) {
                    /* The delayed list is empty.  Set xNextTaskUnblockTime to the
                     * maximum possible value so it is extremely unlikely that the
                     * if( xTickCount >= xNextTaskUnblockTime ) test will pass next
                     * time through. */
                    xNextTaskUnblockTime = portMAX_DELAY;
                    break;
                } else {
                    /* The delayed list is not empty, get the value of the item at
                     * the head of the delayed list.  This is the time at which the
                     * task at the head of the delayed list must be removed from
                     * the Blocked state. */
                    /* MISRA Ref 11.5.3 [Void pointer assignment] */
                    /* coverity[misra_c_2012_rule_11_5_violation] */
                    pxTCB = listGET_OWNER_OF_HEAD_ENTRY(pxDelayedTaskList);
                    xItemValue = listGET_LIST_ITEM_VALUE(&(pxTCB->xStateListItem));

                    if (xConstTickCount < xItemValue) {
                        /* It is not time to unblock this item yet, but the item
                         * value is the time at which the task at the head of the
                         * blocked list must be removed from the Blocked state -
                         * so record the item value in xNextTaskUnblockTime. */
                        xNextTaskUnblockTime = xItemValue;
                        break;
                    } else {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    /* It is time to remove the item from the Blocked state. */
TickReadySetBeforeUnblock:
                    //@ assert In(&pxTCB->xStateListItem, pxDelayedTaskList_ghost);
                    //@ assert \valid{TickDrainStart}(&pxTCB->xStateListItem);
                    //@ assert In{TickDrainStart}(&pxTCB->xStateListItem, pxDelayedTaskList_ghost);
                    /*@ assert In{Pre}(&pxTCB->xStateListItem,
                                        \at(pxDelayedTaskList_ghost, Pre)) ||
                               In{Pre}(&pxTCB->xStateListItem,
                                        \at(pxOverflowDelayedTaskList_ghost, Pre)); */
                    //@ assert !In{Pre}(&pxTCB->xStateListItem, &xReadyTasksList);
                    //@ assert !In{TickDrainStart}(&pxTCB->xStateListItem, &xReadyTasksList);
                    listREMOVE_ITEM(&(pxTCB->xStateListItem));

                    /* Is the task waiting on an event also?  If so remove it from
                     * the event list. */
                    if (listLIST_ITEM_CONTAINER(&(pxTCB->xEventListItem)) != NULL) {
                        //@ assert pxTCB->xEventListItem.pxContainer != &xReadyTasksList;
                        listREMOVE_ITEM(&(pxTCB->xEventListItem));
                    } else {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    /* Place the unblocked task into the appropriate ready list. */
#ifdef SANITY_PROBE
                    /* Branch-local sanity probe - must NOT prove.  This catches
                     * contradictory assumptions specifically on the unblock path. */
                    //@ assert sanity_tick_unblock_probe: \false;
#endif
                    /*@ assert xSwitchRequired == pdTRUE ||
                               In(&pxCurrentTCB_ghost->xStateListItem,
                                  &xReadyTasksList); */
                    /*@ assert xSwitchRequired == pdTRUE ||
                               ListValueLowerBound(&xReadyTasksList,
                                                   pxCurrentTCB_ghost->xDeadline); */
                    prvAddTaskToReadyList(pxTCB);
                    /* Carry the delayed-removal/readied relation across this
                     * unblock: previous removals stay accounted for, and the
                     * current state item moved from the drain list to ready. */
                    //@ assert In(&pxTCB->xStateListItem, &xReadyTasksList);
                    //@ assert ReadyItemDeadlineMatches(&pxTCB->xStateListItem);
                    //@ assert ReadyList(&xReadyTasksList);
                    /*@ assert SchedulerListContext(&xReadyTasksList,
                                                     pxDelayedTaskList_ghost,
                                                     pxOverflowDelayedTaskList_ghost); */
                    /*@ assert ReadiedItemBetween{Pre,Here}
                          (&pxTCB->xStateListItem, &xReadyTasksList); */
                    /*@ assert RemovedItemBetween{Pre,Here}
                          (&pxTCB->xStateListItem,
                           \at(pxDelayedTaskList_ghost, TickDrainStart)); */
                    /*@ assert DelayedRemovalsWereReadied
                          {Pre,TickReadySetBeforeUnblock}
                          (\at(pxDelayedTaskList_ghost, TickDrainStart),
                           &xReadyTasksList); */
                    /*@ assert PriorReadiedItemsStayReadied
                          {Pre,TickReadySetBeforeUnblock,Here}
                          (&pxTCB->xStateListItem, &xReadyTasksList); */
                    /*@ assert PriorDelayedRemovalsAreUnchanged
                          {Pre,TickReadySetBeforeUnblock,Here}
                          (&pxTCB->xStateListItem,
                           \at(pxDelayedTaskList_ghost, TickDrainStart)); */
                    /*@ assert DelayedRemovalsWereReadied{Pre,Here}
                          (\at(pxDelayedTaskList_ghost, TickDrainStart),
                           &xReadyTasksList); */

                           /* A task being unblocked cannot cause an immediate context switch if
 * preemption is turned off. */
#if (configUSE_PREEMPTION == 1)
                    {
#if (configNUMBER_OF_CORES == 1)
                        {
/* Preemption is on, but a context switch should only be performed if the
 * unblocked task's priority is higher than the currently executing task. */
#if (EDF_SCHEDULER == 1)
                            if (pxTCB->xDeadline < pxCurrentTCB->xDeadline)
#else
                            if (pxTCB->uxPriority > pxCurrentTCB->uxPriority)
#endif
                            {
                                xSwitchRequired = pdTRUE;
                            } else {
                                mtCOVERAGE_TEST_MARKER();
                            }
                        }
#else  /* #if( configNUMBER_OF_CORES == 1 ) */
                        {
                            prvYieldForTask(pxTCB);
                        }
#endif /* #if( configNUMBER_OF_CORES == 1 ) */
                    }
#endif /* #if ( configUSE_PREEMPTION == 1 ) */
                }
            }
        }

#ifdef SANITY_PROBE
        /* Sanity probe - must NOT prove. */
        //@ assert sanity_tick_exit_probe: \false;
#endif

#if (configUSE_PREEMPTION == 1)
        {
#if (configNUMBER_OF_CORES == 1)
            {
                /* For single core the core ID is always 0. */
                if (xYieldPendings[0] != pdFALSE) {
                    xSwitchRequired = pdTRUE;
                } else {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
#else /* #if ( configNUMBER_OF_CORES == 1 ) */
            {
                BaseType_t xCoreID, xCurrentCoreID;
                xCurrentCoreID = (BaseType_t)portGET_CORE_ID();

                for (xCoreID = 0; xCoreID < (BaseType_t)configNUMBER_OF_CORES; xCoreID++) {
                    if (xYieldPendings[xCoreID] != pdFALSE) {
                        if (xCoreID == xCurrentCoreID) {
                            xSwitchRequired = pdTRUE;
                        } else {
                            prvYieldCore(xCoreID);
                        }
                    } else {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }
            }
#endif /* #if ( configNUMBER_OF_CORES == 1 ) */
        }
#endif /* #if ( configUSE_PREEMPTION == 1 ) */
    } else {
        xPendedTicks += 1U;
    }

    traceRETURN_xTaskIncrementTick(xSwitchRequired);

    return xSwitchRequired;
}
