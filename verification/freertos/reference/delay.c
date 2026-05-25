/*
 * Verification overlay (reference) for xTaskDelayUntil and
 * xTaskDelayUntilUnfixed (the legacy non-refresh variant).
 *
 * The public API is source-shaped, but the surrounding scheduler operations are
 * kept as small contracts.  The xTaskResumeAll body is source-shaped for the
 * empty-pending-ready/no-pended-ticks path used by this slice; pended-tick
 * draining is still owned by the xTaskIncrementTick effort.
 *
 * This slice targets the finite-delay move, delay-until blocking decision,
 * yield behavior, and scheduler-list context preservation.
 */

#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "FreeRTOS.h"
#include "list.h"
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

#include "freertos_volatile_instrumentation.h"

#define FREERTOS_USE_ABSTRACT_LIST_MODEL
#include "scheduler_model.h"

/* Route the helper's direct uxListRemove call through the scheduler model. */
#define uxListRemove(pxItemToRemove) vSchedulerListRemove_abs((pxItemToRemove))

/* EDF builds do not maintain priority ready bitmaps. */
#define portRESET_READY_PRIORITY(uxPriority, uxTopReadyPriority)

/* Critical sections and port locking are outside this sequential reference slice. */
#undef configASSERT
#define configASSERT(x) ((void)(x))
#define taskENTER_CRITICAL()
#define taskEXIT_CRITICAL()
#undef portGET_CORE_ID
#define portGET_CORE_ID() ((BaseType_t)0)
#undef portRELEASE_TASK_LOCK
#define portRELEASE_TASK_LOCK(xCoreID) ((void)(xCoreID))
#define portMEMORY_BARRIER()

/* From tasks.c:294, EDF version of prvAddTaskToReadyList. */
#define prvAddTaskToReadyList(pxTCB)                                        \
    do {                                                                    \
        listSET_LIST_ITEM_VALUE(&(pxTCB->xStateListItem), (pxTCB)->xDeadline); \
        vListInsert(&(xReadyTasksList), &((pxTCB)->xStateListItem));        \
    } while (0)

#include "freertos_scheduler_stubs.h"

/*@
  requires uxSchedulerSuspended_ghost == (UBaseType_t)0U;

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns uxSchedulerSuspended,
          uxSchedulerSuspended_ghost;

  ensures uxSchedulerSuspended_ghost == (UBaseType_t)1U;
*/
void vTaskSuspendAll(void) {
    traceENTER_vTaskSuspendAll();
    uxSchedulerSuspended = (UBaseType_t)(uxSchedulerSuspended + 1U);
    traceRETURN_vTaskSuspendAll();
}

#define taskYIELD_WITHIN_API() portYIELD_WITHIN_API()

#define taskYIELD_TASK_CORE_IF_USING_PREEMPTION(pxTCB) \
    do {                                               \
        (void)(pxTCB);                                 \
        portYIELD_WITHIN_API();                        \
    } while (0)

/*@
  requires SchedulerListContext(&xReadyTasksList,
                                pxDelayedTaskList_ghost,
                                pxOverflowDelayedTaskList_ghost);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns xNextTaskUnblockTime,
          xNextTaskUnblockTime_ghost;

  ensures SchedulerListContext(&xReadyTasksList,
                               pxDelayedTaskList_ghost,
                               pxOverflowDelayedTaskList_ghost);
*/
static void prvResetNextTaskUnblockTime(void);

/*@
  requires \false;

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns \nothing;
*/
BaseType_t xTaskIncrementTick(void);

/*@
  requires uxSchedulerSuspended_ghost == (UBaseType_t)1U;
  requires uxCurrentNumberOfTasks_ghost > (UBaseType_t)0U;
  requires xYieldPendings0_ghost == pdTRUE || xYieldPendings0_ghost == pdFALSE;
  requires xPendedTicks_ghost == (TickType_t)0U;
  requires ListInv(&xPendingReadyList);
  requires xPendingReadyList.uxNumberOfItems == (UBaseType_t)0U;
  requires SchedulerListContext(&xReadyTasksList,
                                pxDelayedTaskList_ghost,
                                pxOverflowDelayedTaskList_ghost);
  requires xReadyTasksList.uxNumberOfItems > (UBaseType_t)0U;
  requires \separated(&pxCurrentTCB,
                      &uxSchedulerSuspended,
                      &uxCurrentNumberOfTasks,
                      &xPendedTicks,
                      &xNextTaskUnblockTime,
                      &xYieldPendings[0],
                      &xPendingReadyList.uxNumberOfItems,
                      &xReadyTasksList.uxNumberOfItems);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns uxSchedulerSuspended,
          uxSchedulerSuspended_ghost,
          pxCurrentTCB,
          pxCurrentTCB_ghost,
          xYieldPendings[0],
          xYieldPendings0_ghost;

  ensures uxSchedulerSuspended_ghost == (UBaseType_t)0U;
  ensures \result == pdTRUE || \result == pdFALSE;
  ensures SchedulerListContext(&xReadyTasksList,
                               pxDelayedTaskList_ghost,
                               pxOverflowDelayedTaskList_ghost);

  behavior yield_performed:
    assumes xYieldPendings0_ghost != pdFALSE;
    ensures \result == pdTRUE;
    ensures EDFProperty(&xReadyTasksList, pxCurrentTCB_ghost);

  behavior no_yield:
    assumes xYieldPendings0_ghost == pdFALSE;
    ensures \result == pdFALSE;
    ensures pxCurrentTCB_ghost == \old(pxCurrentTCB_ghost);

  complete behaviors;
  disjoint behaviors;
*/
BaseType_t xTaskResumeAll(void) {
    TCB_t* pxTCB = NULL;
    BaseType_t xAlreadyYielded = pdFALSE;

    traceENTER_xTaskResumeAll();

    /* It is possible that an ISR caused a task to be removed from an event
     * list while the scheduler was suspended.  If this was the case then the
     * removed task will have been added to the xPendingReadyList.  Once the
     * scheduler has been resumed it is safe to move all the pending ready
     * tasks from this list into their appropriate ready list. */
    taskENTER_CRITICAL();
    {
        const BaseType_t xCoreID = (BaseType_t)portGET_CORE_ID();

        /* If uxSchedulerSuspended is zero then this function does not match a
         * previous call to vTaskSuspendAll(). */
        configASSERT(uxSchedulerSuspended != 0U);

        uxSchedulerSuspended = (UBaseType_t)(uxSchedulerSuspended - 1U);
        portRELEASE_TASK_LOCK(xCoreID);

        if (uxSchedulerSuspended == (UBaseType_t)0U) {
            if (uxCurrentNumberOfTasks > (UBaseType_t)0U) {
                /* Move any readied tasks from the pending list into the
                 * appropriate ready list. */
                //@ assert ListInv(&xPendingReadyList);
                //@ assert xPendingReadyList.uxNumberOfItems == (UBaseType_t)0U;
                /*@
                  loop invariant ListInv(&xPendingReadyList);
                  loop invariant xPendingReadyList.uxNumberOfItems == (UBaseType_t)0U;
                  loop invariant pxTCB == \null;
                  loop assigns \nothing;
                  loop variant 0;
                */
                while (listLIST_IS_EMPTY(&xPendingReadyList) == pdFALSE) {
                    /* MISRA Ref 11.5.3 [Void pointer assignment] */
                    /* coverity[misra_c_2012_rule_11_5_violation] */
                    pxTCB = listGET_OWNER_OF_HEAD_ENTRY((&xPendingReadyList));
                    listREMOVE_ITEM(&(pxTCB->xEventListItem));
                    portMEMORY_BARRIER();
                    listREMOVE_ITEM(&(pxTCB->xStateListItem));
                    prvAddTaskToReadyList(pxTCB);

                    {
                        /* If the moved task has a priority higher than the current
                         * task then a yield must be performed. */
                        if (pxTCB->xDeadline < pxCurrentTCB->xDeadline) {
                            xYieldPendings[xCoreID] = pdTRUE;
                        } else {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
                }

                //@ assert pxTCB == \null;
                if (pxTCB != NULL) {
                    /* A task was unblocked while the scheduler was suspended,
                     * which may have prevented the next unblock time from being
                     * re-calculated, in which case re-calculate it now. */
                    prvResetNextTaskUnblockTime();
                }

                /* If any ticks occurred while the scheduler was suspended then
                 * they should be processed now. */
                {
                    TickType_t xPendedCounts = xPendedTicks; /* Non-volatile copy. */

                    //@ assert xPendedCounts == (TickType_t)0U;
                    if (xPendedCounts > (TickType_t)0U) {
                        /*@
                          loop assigns xPendedCounts,
                                       xYieldPendings[0],
                                       xYieldPendings0_ghost;
                          loop variant xPendedCounts;
                        */
                        do {
                            if (xTaskIncrementTick() != pdFALSE) {
                                xYieldPendings[xCoreID] = pdTRUE;
                            } else {
                                mtCOVERAGE_TEST_MARKER();
                            }

                            --xPendedCounts;
                        } while (xPendedCounts > (TickType_t)0U);

                        xPendedTicks = 0;
                    } else {
                        mtCOVERAGE_TEST_MARKER();
                    }
                }

                if (xYieldPendings[xCoreID] != pdFALSE) {
#if (configUSE_PREEMPTION != 0)
                    {
                        xAlreadyYielded = pdTRUE;
                    }
#endif /* #if ( configUSE_PREEMPTION != 0 ) */

                    {
                        //@ assert uxSchedulerSuspended_ghost == (UBaseType_t)0U;
                        //@ assert xReadyTasksList.uxNumberOfItems > (UBaseType_t)0U;
                        //@ assert ReadyList(&xReadyTasksList);
                        /*@ assert SchedulerListContext(&xReadyTasksList,
                                                         pxDelayedTaskList_ghost,
                                                         pxOverflowDelayedTaskList_ghost); */
                        taskYIELD_TASK_CORE_IF_USING_PREEMPTION(pxCurrentTCB);
                    }
                } else {
                    mtCOVERAGE_TEST_MARKER();
                }
            }
        } else {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    taskEXIT_CRITICAL();

    traceRETURN_xTaskResumeAll(xAlreadyYielded);

    return xAlreadyYielded;
}

/*@
  requires xTicksToWait > (TickType_t)0U;
  requires xCanBlockIndefinitely == pdFALSE;
  requires SchedulerListContextIgnoringReadyItemDeadline(
                                &xReadyTasksList,
                                pxDelayedTaskList_ghost,
                                pxOverflowDelayedTaskList_ghost,
                                &pxCurrentTCB_ghost->xStateListItem);
  requires ListInv(&xPendingReadyList);
  requires xPendingReadyList.uxNumberOfItems == (UBaseType_t)0U;
  requires &xPendingReadyList != &xReadyTasksList;
  requires &xPendingReadyList != pxDelayedTaskList_ghost;
  requires &xPendingReadyList != pxOverflowDelayedTaskList_ghost;
  requires \valid(pxCurrentTCB_ghost);
  requires xReadyTasksList.uxNumberOfItems > (UBaseType_t)0U;
  requires pxCurrentTCB_ghost->xEventListItem.pxContainer == \null;
  requires \separated(&pxCurrentTCB,
                      &xTickCount,
                      &xNextTaskUnblockTime,
                      &pxDelayedTaskList,
                      &pxOverflowDelayedTaskList,
                      &xReadyTasksList.uxNumberOfItems,
                      &pxDelayedTaskList_ghost->uxNumberOfItems,
                      &pxOverflowDelayedTaskList_ghost->uxNumberOfItems,
                      &pxCurrentTCB_ghost->xStateListItem.xItemValue,
                      &pxCurrentTCB_ghost->xStateListItem.pxContainer);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns xReadyTasksList.uxNumberOfItems,
          pxDelayedTaskList_ghost->uxNumberOfItems,
          pxOverflowDelayedTaskList_ghost->uxNumberOfItems,
          pxCurrentTCB_ghost->xStateListItem.xItemValue,
          pxCurrentTCB_ghost->xStateListItem.pxContainer,
          xNextTaskUnblockTime,
          xNextTaskUnblockTime_ghost;

  ensures SchedulerListContext(&xReadyTasksList,
                               pxDelayedTaskList_ghost,
                               pxOverflowDelayedTaskList_ghost);
  ensures ListInv(&xPendingReadyList);
  ensures xPendingReadyList.uxNumberOfItems ==
    \old(xPendingReadyList.uxNumberOfItems);
  ensures !In(&pxCurrentTCB_ghost->xStateListItem, &xReadyTasksList);
  ensures xReadyTasksList.uxNumberOfItems ==
    (UBaseType_t)(\old(xReadyTasksList.uxNumberOfItems) - 1U);
  ensures pxCurrentTCB_ghost->xStateListItem.xItemValue ==
            (TickType_t)(\old(xTickCount_ghost) + xTicksToWait);

  ensures \forall ListItem_t *item;
    \valid{Pre}(item) && item != &pxCurrentTCB_ghost->xStateListItem ==>
      (In(item, &xReadyTasksList) <==> In{Pre}(item, &xReadyTasksList));
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) && item != &pxCurrentTCB_ghost->xStateListItem ==>
      (In(item, pxDelayedTaskList_ghost) <==>
       In{Pre}(item, pxDelayedTaskList_ghost));
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) && item != &pxCurrentTCB_ghost->xStateListItem ==>
      (In(item, pxOverflowDelayedTaskList_ghost) <==>
       In{Pre}(item, pxOverflowDelayedTaskList_ghost));

  behavior wake_time_overflows:
    assumes (TickType_t)(xTickCount_ghost + xTicksToWait) < xTickCount_ghost;
    ensures In(&pxCurrentTCB_ghost->xStateListItem, pxOverflowDelayedTaskList_ghost);
    ensures !In(&pxCurrentTCB_ghost->xStateListItem, pxDelayedTaskList_ghost);
    ensures xNextTaskUnblockTime_ghost == \old(xNextTaskUnblockTime_ghost);

  behavior wake_time_current_tick_window:
    assumes (TickType_t)(xTickCount_ghost + xTicksToWait) >= xTickCount_ghost;
    ensures In(&pxCurrentTCB_ghost->xStateListItem, pxDelayedTaskList_ghost);
    ensures !In(&pxCurrentTCB_ghost->xStateListItem, pxOverflowDelayedTaskList_ghost);
    ensures ((TickType_t)(\old(xTickCount_ghost) + xTicksToWait) <
             \old(xNextTaskUnblockTime_ghost)) ==>
              xNextTaskUnblockTime_ghost ==
                (TickType_t)(\old(xTickCount_ghost) + xTicksToWait);
    ensures ((TickType_t)(\old(xTickCount_ghost) + xTicksToWait) >=
             \old(xNextTaskUnblockTime_ghost)) ==>
              xNextTaskUnblockTime_ghost == \old(xNextTaskUnblockTime_ghost);

  complete behaviors;
  disjoint behaviors;
*/
static void prvAddCurrentTaskToDelayedList(TickType_t xTicksToWait,
                                           const BaseType_t xCanBlockIndefinitely) {
    TickType_t xTimeToWake;
    const TickType_t xConstTickCount = xTickCount;
    List_t* const pxDelayedList = pxDelayedTaskList;
    List_t* const pxOverflowDelayedList = pxOverflowDelayedTaskList;

    /* Remove the task from the ready list before adding it to the blocked list
     * as the same list item is used for both lists. */
    if (uxListRemove(&(pxCurrentTCB->xStateListItem)) == (UBaseType_t)0) {
        /* EDF does not maintain a priority bitmap to reset here. */
        portRESET_READY_PRIORITY(pxCurrentTCB->uxPriority, uxTopReadyPriority);
    } else {
        mtCOVERAGE_TEST_MARKER();
    }

    /* xTaskDelayUntil[Unfixed] always requests a finite delay, so the
     * suspended-list branch of the general helper is outside this extraction. */
    xTimeToWake = xConstTickCount + xTicksToWait;

    /* The list item will be inserted in wake time order. */
    listSET_LIST_ITEM_VALUE(&(pxCurrentTCB->xStateListItem), xTimeToWake);

    if (xTimeToWake < xConstTickCount) {
        traceMOVED_TASK_TO_OVERFLOW_DELAYED_LIST();
        vListInsert(pxOverflowDelayedList, &(pxCurrentTCB->xStateListItem));
    } else {
        traceMOVED_TASK_TO_DELAYED_LIST();
        vListInsert(pxDelayedList, &(pxCurrentTCB->xStateListItem));

        if (xTimeToWake < xNextTaskUnblockTime) {
            xNextTaskUnblockTime = xTimeToWake;
        } else {
            mtCOVERAGE_TEST_MARKER();
        }
    }

    (void)xCanBlockIndefinitely;
}

/*@
  predicate DelayUntilShouldDelay(TickType_t previousWakeTime,
                                  TickType_t timeIncrement,
                                  TickType_t tickCount) =
    (tickCount < previousWakeTime &&
     (TickType_t)(previousWakeTime + timeIncrement) < previousWakeTime &&
     (TickType_t)(previousWakeTime + timeIncrement) > tickCount) ||
    (tickCount >= previousWakeTime &&
     ((TickType_t)(previousWakeTime + timeIncrement) < previousWakeTime ||
      (TickType_t)(previousWakeTime + timeIncrement) > tickCount));

  predicate TickSeparatedFromListItem(TickType_t *tick, ListItem_t *item) =
    \separated(tick, item) &&
    \separated(tick, &item->xItemValue) &&
    \separated(tick, &item->pvOwner) &&
    \separated(tick, &item->pxContainer);

  predicate TickSeparatedFromTask(TickType_t *tick, TCB_t *task) =
    \separated(tick, task) &&
    \separated(tick, &task->xStateListItem) &&
    \separated(tick, &task->xEventListItem) &&
    \separated(tick, &task->xDeadline);
*/

/*@
  requires uxSchedulerSuspended_ghost == (UBaseType_t)0U;
  requires SchedulerListContext(&xReadyTasksList,
                                pxDelayedTaskList_ghost,
                                pxOverflowDelayedTaskList_ghost);
  requires \valid(pxCurrentTCB_ghost);
  requires \valid(pxPreviousWakeTime);
  requires xTimeIncrement > (TickType_t)0U;
  requires DelayUntilShouldDelay(*pxPreviousWakeTime,
                                 xTimeIncrement,
                                 xTickCount_ghost);
  requires uxCurrentNumberOfTasks_ghost > (UBaseType_t)0U;
  requires xYieldPendings0_ghost == pdTRUE || xYieldPendings0_ghost == pdFALSE;
  requires xPendedTicks_ghost == (TickType_t)0U;
  requires ListInv(&xPendingReadyList);
  requires xPendingReadyList.uxNumberOfItems == (UBaseType_t)0U;
  requires &xPendingReadyList != &xReadyTasksList;
  requires &xPendingReadyList != pxDelayedTaskList_ghost;
  requires &xPendingReadyList != pxOverflowDelayedTaskList_ghost;
  requires xReadyTasksList.uxNumberOfItems > (UBaseType_t)1U;
  requires In(&pxCurrentTCB_ghost->xStateListItem, &xReadyTasksList);
  requires pxCurrentTCB_ghost->xEventListItem.pxContainer == \null;
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, &xReadyTasksList) &&
    item != &pxCurrentTCB_ghost->xStateListItem ==>
      \separated(&pxCurrentTCB_ghost->xDeadline,
                 &((TCB_t *)item->pvOwner)->xDeadline);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, &xReadyTasksList) &&
    item != &pxCurrentTCB_ghost->xStateListItem ==>
      \separated(pxPreviousWakeTime,
                 &((TCB_t *)item->pvOwner)->xDeadline);
  requires \forall ListItem_t *item;
    \valid(item) ==>
      TickSeparatedFromListItem(pxPreviousWakeTime, item);
  requires \forall ListItem_t *item;
    \valid(item) ==>
      TickSeparatedFromListItem(&pxCurrentTCB_ghost->xDeadline, item);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, &xReadyTasksList) &&
    item != &pxCurrentTCB_ghost->xStateListItem ==>
      TickSeparatedFromTask(&pxCurrentTCB_ghost->xDeadline,
                            (TCB_t *)item->pvOwner);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, pxDelayedTaskList_ghost) ==>
      TickSeparatedFromTask(&pxCurrentTCB_ghost->xDeadline,
                            (TCB_t *)item->pvOwner);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, pxOverflowDelayedTaskList_ghost) ==>
      TickSeparatedFromTask(&pxCurrentTCB_ghost->xDeadline,
                            (TCB_t *)item->pvOwner);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, &xReadyTasksList) ==>
      TickSeparatedFromTask(pxPreviousWakeTime,
                            (TCB_t *)item->pvOwner);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, pxDelayedTaskList_ghost) ==>
      TickSeparatedFromTask(pxPreviousWakeTime,
                            (TCB_t *)item->pvOwner);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, pxOverflowDelayedTaskList_ghost) ==>
      TickSeparatedFromTask(pxPreviousWakeTime,
                            (TCB_t *)item->pvOwner);
  requires \separated(&pxCurrentTCB_ghost->xDeadline, &xPendingReadyList);
  requires \separated(&pxCurrentTCB_ghost->xDeadline, &xReadyTasksList);
  requires \separated(&pxCurrentTCB_ghost->xDeadline, pxDelayedTaskList_ghost);
  requires \separated(&pxCurrentTCB_ghost->xDeadline, pxOverflowDelayedTaskList_ghost);
  requires \separated(&pxCurrentTCB,
                      &uxSchedulerSuspended,
                      &uxCurrentNumberOfTasks,
                      &xTickCount,
                      &xPendedTicks,
                      &xNextTaskUnblockTime,
                      &pxDelayedTaskList,
                      &pxOverflowDelayedTaskList,
                      &xYieldPendings[0],
                      &xPendingReadyList.uxNumberOfItems,
                      &xReadyTasksList.uxNumberOfItems,
                      &pxDelayedTaskList_ghost->uxNumberOfItems,
                      &pxOverflowDelayedTaskList_ghost->uxNumberOfItems,
                      &pxCurrentTCB_ghost->xDeadline,
                      &pxCurrentTCB_ghost->xStateListItem.xItemValue,
                      &pxCurrentTCB_ghost->xStateListItem.pxContainer);
  requires \separated(pxPreviousWakeTime, &xPendingReadyList);
  requires \separated(pxPreviousWakeTime, &xReadyTasksList);
  requires \separated(pxPreviousWakeTime, pxDelayedTaskList_ghost);
  requires \separated(pxPreviousWakeTime, pxOverflowDelayedTaskList_ghost);
  requires \separated(pxPreviousWakeTime, &pxCurrentTCB_ghost->xEventListItem);
  requires \separated(pxPreviousWakeTime, &pxCurrentTCB_ghost->xStateListItem);
  requires \separated(pxPreviousWakeTime,
                      &pxCurrentTCB,
                      &uxSchedulerSuspended,
                      &uxCurrentNumberOfTasks,
                      &xTickCount,
                      &xPendedTicks,
                      &xNextTaskUnblockTime,
                      &pxDelayedTaskList,
                      &pxOverflowDelayedTaskList,
                      &xYieldPendings[0],
                      &xPendingReadyList.uxNumberOfItems,
                      &xReadyTasksList.uxNumberOfItems,
                      &pxDelayedTaskList_ghost->uxNumberOfItems,
                      &pxOverflowDelayedTaskList_ghost->uxNumberOfItems,
                      &pxCurrentTCB_ghost->xDeadline,
                      &pxCurrentTCB_ghost->xStateListItem.xItemValue,
                      &pxCurrentTCB_ghost->xStateListItem.pxContainer);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns uxSchedulerSuspended,
          uxSchedulerSuspended_ghost,
          pxCurrentTCB,
          pxCurrentTCB_ghost,
          xYieldPendings[0],
          xYieldPendings0_ghost,
          *pxPreviousWakeTime,
          \old(pxCurrentTCB_ghost)->xDeadline,
          xReadyTasksList.uxNumberOfItems,
          pxDelayedTaskList_ghost->uxNumberOfItems,
          pxOverflowDelayedTaskList_ghost->uxNumberOfItems,
          \old(pxCurrentTCB_ghost)->xStateListItem.xItemValue,
          \old(pxCurrentTCB_ghost)->xStateListItem.pxContainer,
          xNextTaskUnblockTime,
          xNextTaskUnblockTime_ghost;

  ensures uxSchedulerSuspended_ghost == (UBaseType_t)0U;
  ensures \result == pdTRUE;
  ensures *pxPreviousWakeTime ==
            (TickType_t)(\old(*pxPreviousWakeTime) + xTimeIncrement);
  ensures \old(pxCurrentTCB_ghost)->xDeadline ==
            (TickType_t)(\old(*pxPreviousWakeTime) +
                         xTimeIncrement +
                         xTimeIncrement);
  ensures SchedulerListContext(&xReadyTasksList,
                               pxDelayedTaskList_ghost,
                               pxOverflowDelayedTaskList_ghost);
  ensures EDFProperty(&xReadyTasksList, pxCurrentTCB_ghost);
  ensures !In(&\old(pxCurrentTCB_ghost)->xStateListItem, &xReadyTasksList);

  behavior tick_count_overflowed_and_wake_is_future:
    assumes xTickCount_ghost < *pxPreviousWakeTime;
    assumes (TickType_t)(*pxPreviousWakeTime + xTimeIncrement) <
              *pxPreviousWakeTime;
    assumes (TickType_t)(*pxPreviousWakeTime + xTimeIncrement) >
              xTickCount_ghost;
    ensures In(&\old(pxCurrentTCB_ghost)->xStateListItem, pxDelayedTaskList_ghost);
    ensures !In(&\old(pxCurrentTCB_ghost)->xStateListItem,
                pxOverflowDelayedTaskList_ghost);

  behavior wake_time_overflows:
    assumes xTickCount_ghost >= *pxPreviousWakeTime;
    assumes (TickType_t)(*pxPreviousWakeTime + xTimeIncrement) <
              *pxPreviousWakeTime;
    ensures In(&\old(pxCurrentTCB_ghost)->xStateListItem,
               pxOverflowDelayedTaskList_ghost);
    ensures !In(&\old(pxCurrentTCB_ghost)->xStateListItem, pxDelayedTaskList_ghost);

  behavior wake_time_is_future:
    assumes xTickCount_ghost >= *pxPreviousWakeTime;
    assumes (TickType_t)(*pxPreviousWakeTime + xTimeIncrement) >
              xTickCount_ghost;
    ensures In(&\old(pxCurrentTCB_ghost)->xStateListItem, pxDelayedTaskList_ghost);
    ensures !In(&\old(pxCurrentTCB_ghost)->xStateListItem,
                pxOverflowDelayedTaskList_ghost);

  complete behaviors;
  disjoint behaviors;
*/
BaseType_t xTaskDelayUntilUnfixed(TickType_t* const pxPreviousWakeTime,
                           const TickType_t xTimeIncrement) {
    TickType_t xTimeToWake;
    BaseType_t xAlreadyYielded, xShouldDelay = pdFALSE;

    traceENTER_xTaskDelayUntil(pxPreviousWakeTime, xTimeIncrement);

    configASSERT(pxPreviousWakeTime);
    configASSERT((xTimeIncrement > 0U));

    vTaskSuspendAll();
    {
        /* Minor optimisation.  The tick count cannot change in this
         * block. */
        const TickType_t xConstTickCount = xTickCount;

        configASSERT(uxSchedulerSuspended == 1U);
        //@ assert uxSchedulerSuspended_ghost == (UBaseType_t)1U;

#if (EDF_SCHEDULER == 1)
        // new deadline = last wake time + period + rel. deadline (we only support implicit deadline, hence rel. deadline = period => xTimeIncrement*2)
        pxCurrentTCB->xDeadline = *pxPreviousWakeTime + xTimeIncrement + xTimeIncrement;
#endif

        /* Generate the tick time at which the task wants to wake. */
        xTimeToWake = *pxPreviousWakeTime + xTimeIncrement;

        if (xConstTickCount < *pxPreviousWakeTime) {
            if ((xTimeToWake < *pxPreviousWakeTime) && (xTimeToWake > xConstTickCount)) {
                xShouldDelay = pdTRUE;
            } else {
                mtCOVERAGE_TEST_MARKER();
            }
        } else {
            if ((xTimeToWake < *pxPreviousWakeTime) || (xTimeToWake > xConstTickCount)) {
                xShouldDelay = pdTRUE;
            } else {
                mtCOVERAGE_TEST_MARKER();
            }
        }

        /* Update the wake time ready for the next call. */
        *pxPreviousWakeTime = xTimeToWake;

        if (xShouldDelay != pdFALSE) {
            traceTASK_DELAY_UNTIL(xTimeToWake);

            /* prvAddCurrentTaskToDelayedList() needs the block time, not
             * the time to wake, so subtract the current tick count. */
            //@ assert (TickType_t)(xTimeToWake - xConstTickCount) > (TickType_t)0U;
            prvAddCurrentTaskToDelayedList(xTimeToWake - xConstTickCount, pdFALSE);
        } else {
            mtCOVERAGE_TEST_MARKER();
        }
    }
    xAlreadyYielded = xTaskResumeAll();

    /* Force a reschedule if xTaskResumeAll has not already done so, we may
     * have put ourselves to sleep. */
    if (xAlreadyYielded == pdFALSE) {
        taskYIELD_WITHIN_API();
    } else {
        mtCOVERAGE_TEST_MARKER();
    }

    traceRETURN_xTaskDelayUntil(xShouldDelay);

    return xShouldDelay;
}

/*@
  requires uxSchedulerSuspended_ghost == (UBaseType_t)0U;
  requires SchedulerListContext(&xReadyTasksList,
                                pxDelayedTaskList_ghost,
                                pxOverflowDelayedTaskList_ghost);
  requires \valid(pxCurrentTCB_ghost);
  requires \valid(pxPreviousWakeTime);
  requires xTimeIncrement > (TickType_t)0U;
  requires uxCurrentNumberOfTasks_ghost > (UBaseType_t)0U;
  requires xYieldPendings0_ghost == pdTRUE || xYieldPendings0_ghost == pdFALSE;
  requires xPendedTicks_ghost == (TickType_t)0U;
  requires ListInv(&xPendingReadyList);
  requires xPendingReadyList.uxNumberOfItems == (UBaseType_t)0U;
  requires &xPendingReadyList != &xReadyTasksList;
  requires &xPendingReadyList != pxDelayedTaskList_ghost;
  requires &xPendingReadyList != pxOverflowDelayedTaskList_ghost;
  requires xReadyTasksList.uxNumberOfItems > (UBaseType_t)1U;
  requires In(&pxCurrentTCB_ghost->xStateListItem, &xReadyTasksList);
  requires pxCurrentTCB_ghost->xEventListItem.pxContainer == \null;
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, &xReadyTasksList) &&
    item != &pxCurrentTCB_ghost->xStateListItem ==>
      \separated(&pxCurrentTCB_ghost->xDeadline,
                 &((TCB_t *)item->pvOwner)->xDeadline);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, &xReadyTasksList) &&
    item != &pxCurrentTCB_ghost->xStateListItem ==>
      \separated(pxPreviousWakeTime,
                 &((TCB_t *)item->pvOwner)->xDeadline);
  requires \forall ListItem_t *item;
    \valid(item) ==>
      TickSeparatedFromListItem(pxPreviousWakeTime, item);
  requires \forall ListItem_t *item;
    \valid(item) ==>
      TickSeparatedFromListItem(&pxCurrentTCB_ghost->xDeadline, item);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, &xReadyTasksList) &&
    item != &pxCurrentTCB_ghost->xStateListItem ==>
      TickSeparatedFromTask(&pxCurrentTCB_ghost->xDeadline,
                            (TCB_t *)item->pvOwner);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, pxDelayedTaskList_ghost) ==>
      TickSeparatedFromTask(&pxCurrentTCB_ghost->xDeadline,
                            (TCB_t *)item->pvOwner);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, pxOverflowDelayedTaskList_ghost) ==>
      TickSeparatedFromTask(&pxCurrentTCB_ghost->xDeadline,
                            (TCB_t *)item->pvOwner);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, &xReadyTasksList) ==>
      TickSeparatedFromTask(pxPreviousWakeTime,
                            (TCB_t *)item->pvOwner);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, pxDelayedTaskList_ghost) ==>
      TickSeparatedFromTask(pxPreviousWakeTime,
                            (TCB_t *)item->pvOwner);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, pxOverflowDelayedTaskList_ghost) ==>
      TickSeparatedFromTask(pxPreviousWakeTime,
                            (TCB_t *)item->pvOwner);
  requires \separated(&pxCurrentTCB_ghost->xDeadline, &xPendingReadyList);
  requires \separated(&pxCurrentTCB_ghost->xDeadline, &xReadyTasksList);
  requires \separated(&pxCurrentTCB_ghost->xDeadline, pxDelayedTaskList_ghost);
  requires \separated(&pxCurrentTCB_ghost->xDeadline, pxOverflowDelayedTaskList_ghost);
  requires \separated(&pxCurrentTCB,
                      &uxSchedulerSuspended,
                      &uxCurrentNumberOfTasks,
                      &xTickCount,
                      &xPendedTicks,
                      &xNextTaskUnblockTime,
                      &pxDelayedTaskList,
                      &pxOverflowDelayedTaskList,
                      &xYieldPendings[0],
                      &xPendingReadyList.uxNumberOfItems,
                      &xReadyTasksList.uxNumberOfItems,
                      &pxDelayedTaskList_ghost->uxNumberOfItems,
                      &pxOverflowDelayedTaskList_ghost->uxNumberOfItems,
                      &pxCurrentTCB_ghost->xDeadline,
                      &pxCurrentTCB_ghost->xStateListItem.xItemValue,
                      &pxCurrentTCB_ghost->xStateListItem.pxContainer);
  requires \separated(pxPreviousWakeTime, &xPendingReadyList);
  requires \separated(pxPreviousWakeTime, &xReadyTasksList);
  requires \separated(pxPreviousWakeTime, pxDelayedTaskList_ghost);
  requires \separated(pxPreviousWakeTime, pxOverflowDelayedTaskList_ghost);
  requires \separated(pxPreviousWakeTime, &pxCurrentTCB_ghost->xEventListItem);
  requires \separated(pxPreviousWakeTime, &pxCurrentTCB_ghost->xStateListItem);
  requires \separated(pxPreviousWakeTime,
                      &pxCurrentTCB,
                      &uxSchedulerSuspended,
                      &uxCurrentNumberOfTasks,
                      &xTickCount,
                      &xPendedTicks,
                      &xNextTaskUnblockTime,
                      &pxDelayedTaskList,
                      &pxOverflowDelayedTaskList,
                      &xYieldPendings[0],
                      &xPendingReadyList.uxNumberOfItems,
                      &xReadyTasksList.uxNumberOfItems,
                      &pxDelayedTaskList_ghost->uxNumberOfItems,
                      &pxOverflowDelayedTaskList_ghost->uxNumberOfItems,
                      &pxCurrentTCB_ghost->xDeadline,
                      &pxCurrentTCB_ghost->xStateListItem.xItemValue,
                      &pxCurrentTCB_ghost->xStateListItem.pxContainer);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns uxSchedulerSuspended,
          uxSchedulerSuspended_ghost,
          pxCurrentTCB,
          pxCurrentTCB_ghost,
          xYieldPendings[0],
          xYieldPendings0_ghost,
          *pxPreviousWakeTime,
          \old(pxCurrentTCB_ghost)->xDeadline,
          xReadyTasksList.uxNumberOfItems,
          pxDelayedTaskList_ghost->uxNumberOfItems,
          pxOverflowDelayedTaskList_ghost->uxNumberOfItems,
          \old(pxCurrentTCB_ghost)->xStateListItem.xItemValue,
          \old(pxCurrentTCB_ghost)->xStateListItem.pxContainer,
          xNextTaskUnblockTime,
          xNextTaskUnblockTime_ghost;

  ensures uxSchedulerSuspended_ghost == (UBaseType_t)0U;
  ensures \result == pdTRUE || \result == pdFALSE;
  ensures *pxPreviousWakeTime ==
            (TickType_t)(\old(*pxPreviousWakeTime) + xTimeIncrement);
  ensures \old(pxCurrentTCB_ghost)->xDeadline ==
            (TickType_t)(\old(*pxPreviousWakeTime) +
                         xTimeIncrement +
                         xTimeIncrement);
  ensures SchedulerListContext(&xReadyTasksList,
                               pxDelayedTaskList_ghost,
                               pxOverflowDelayedTaskList_ghost);
  ensures EDFProperty(&xReadyTasksList, pxCurrentTCB_ghost);

  ensures \forall ListItem_t *item;
    \valid{Pre}(item) &&
    item != &\old(pxCurrentTCB_ghost)->xStateListItem ==>
      (In(item, &xReadyTasksList) <==> In{Pre}(item, &xReadyTasksList));
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) &&
    item != &\old(pxCurrentTCB_ghost)->xStateListItem ==>
      (In(item, pxDelayedTaskList_ghost) <==>
       In{Pre}(item, pxDelayedTaskList_ghost));
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) &&
    item != &\old(pxCurrentTCB_ghost)->xStateListItem ==>
      (In(item, pxOverflowDelayedTaskList_ghost) <==>
       In{Pre}(item, pxOverflowDelayedTaskList_ghost));

  behavior tick_count_overflowed_and_wake_is_future:
    assumes xTickCount_ghost < *pxPreviousWakeTime;
    assumes (TickType_t)(*pxPreviousWakeTime + xTimeIncrement) <
              *pxPreviousWakeTime;
    assumes (TickType_t)(*pxPreviousWakeTime + xTimeIncrement) >
              xTickCount_ghost;
    ensures \result == pdTRUE;
    ensures !In(&\old(pxCurrentTCB_ghost)->xStateListItem, &xReadyTasksList);
    ensures In(&\old(pxCurrentTCB_ghost)->xStateListItem, pxDelayedTaskList_ghost);
    ensures !In(&\old(pxCurrentTCB_ghost)->xStateListItem,
                pxOverflowDelayedTaskList_ghost);

  behavior wake_time_overflows:
    assumes xTickCount_ghost >= *pxPreviousWakeTime;
    assumes (TickType_t)(*pxPreviousWakeTime + xTimeIncrement) <
              *pxPreviousWakeTime;
    ensures \result == pdTRUE;
    ensures !In(&\old(pxCurrentTCB_ghost)->xStateListItem, &xReadyTasksList);
    ensures In(&\old(pxCurrentTCB_ghost)->xStateListItem,
               pxOverflowDelayedTaskList_ghost);
    ensures !In(&\old(pxCurrentTCB_ghost)->xStateListItem, pxDelayedTaskList_ghost);

  behavior wake_time_is_future:
    assumes xTickCount_ghost >= *pxPreviousWakeTime;
    assumes (TickType_t)(*pxPreviousWakeTime + xTimeIncrement) >
              xTickCount_ghost;
    ensures \result == pdTRUE;
    ensures !In(&\old(pxCurrentTCB_ghost)->xStateListItem, &xReadyTasksList);
    ensures In(&\old(pxCurrentTCB_ghost)->xStateListItem, pxDelayedTaskList_ghost);
    ensures !In(&\old(pxCurrentTCB_ghost)->xStateListItem,
                pxOverflowDelayedTaskList_ghost);

  behavior no_delay:
    assumes !DelayUntilShouldDelay(*pxPreviousWakeTime,
                                   xTimeIncrement,
                                   xTickCount_ghost);
    ensures \result == pdFALSE;
    ensures In(&\old(pxCurrentTCB_ghost)->xStateListItem, &xReadyTasksList);
    ensures !In(&\old(pxCurrentTCB_ghost)->xStateListItem, pxDelayedTaskList_ghost);
    ensures !In(&\old(pxCurrentTCB_ghost)->xStateListItem,
                pxOverflowDelayedTaskList_ghost);
    ensures xReadyTasksList.uxNumberOfItems ==
              \old(xReadyTasksList.uxNumberOfItems);
    ensures \old(pxCurrentTCB_ghost)->xStateListItem.xItemValue ==
              \old(pxCurrentTCB_ghost)->xDeadline;

  complete behaviors;
  disjoint behaviors;
*/
BaseType_t xTaskDelayUntil(TickType_t* const pxPreviousWakeTime,
                                       const TickType_t xTimeIncrement) {
    TickType_t xTimeToWake;
    BaseType_t xAlreadyYielded, xShouldDelay = pdFALSE;

    traceENTER_xTaskDelayUntil(pxPreviousWakeTime, xTimeIncrement);

    configASSERT(pxPreviousWakeTime);
    configASSERT((xTimeIncrement > 0U));

    vTaskSuspendAll();
    {
        /* Minor optimisation.  The tick count cannot change in this
         * block. */
        const TickType_t xConstTickCount = xTickCount;

        configASSERT(uxSchedulerSuspended == 1U);
        //@ assert uxSchedulerSuspended_ghost == (UBaseType_t)1U;

#if (EDF_SCHEDULER == 1)
        // new deadline = last wake time + period + rel. deadline (we only support implicit deadline, hence rel. deadline = period => xTimeIncrement*2)
        pxCurrentTCB->xDeadline = *pxPreviousWakeTime + xTimeIncrement + xTimeIncrement;
#endif

        /* Generate the tick time at which the task wants to wake. */
        xTimeToWake = *pxPreviousWakeTime + xTimeIncrement;

        if (xConstTickCount < *pxPreviousWakeTime) {
            if ((xTimeToWake < *pxPreviousWakeTime) && (xTimeToWake > xConstTickCount)) {
                xShouldDelay = pdTRUE;
            } else {
                mtCOVERAGE_TEST_MARKER();
            }
        } else {
            if ((xTimeToWake < *pxPreviousWakeTime) || (xTimeToWake > xConstTickCount)) {
                xShouldDelay = pdTRUE;
            } else {
                mtCOVERAGE_TEST_MARKER();
            }
        }

        /* Update the wake time ready for the next call. */
        *pxPreviousWakeTime = xTimeToWake;

        if (xShouldDelay != pdFALSE) {
            traceTASK_DELAY_UNTIL(xTimeToWake);

            /* prvAddCurrentTaskToDelayedList() needs the block time, not
             * the time to wake, so subtract the current tick count. */
            //@ assert (TickType_t)(xTimeToWake - xConstTickCount) > (TickType_t)0U;
            prvAddCurrentTaskToDelayedList(xTimeToWake - xConstTickCount, pdFALSE);
        } else {
            mtCOVERAGE_TEST_MARKER();
#if (EDF_SCHEDULER == 1)
            /*@ assert SchedulerListContextIgnoringReadyItemDeadline(
                           &xReadyTasksList,
                           pxDelayedTaskList_ghost,
                           pxOverflowDelayedTaskList_ghost,
                           &pxCurrentTCB_ghost->xStateListItem); */
            uxListRemove(&(pxCurrentTCB->xStateListItem));
            prvAddTaskToReadyList(pxCurrentTCB);
#endif
        }
    }
    xAlreadyYielded = xTaskResumeAll();

    /* Force a reschedule if xTaskResumeAll has not already done so, we may
     * have put ourselves to sleep. */
    if (xAlreadyYielded == pdFALSE) {
        taskYIELD_WITHIN_API();
    } else {
        mtCOVERAGE_TEST_MARKER();
    }

    traceRETURN_xTaskDelayUntil(xShouldDelay);

    return xShouldDelay;
}

