/*
 * Verification overlay (reference) for vTaskDelay and xTaskDelayUntil.
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

/* Minimal TCB fields dereferenced by the finite-delay paths. */
struct tskTaskControlBlock {
    ListItem_t xStateListItem;
    ListItem_t xEventListItem;
    TickType_t xDeadline;
};
typedef struct tskTaskControlBlock TCB_t;

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

/* Hack: Frama-C can't handle volatile. */
#ifdef __FRAMAC__
    TCB_t *     pxCurrentTCB;
    List_t      xReadyTasksList;
    List_t      xPendingReadyList;
    UBaseType_t uxSchedulerSuspended;
    UBaseType_t uxCurrentNumberOfTasks;
    TickType_t  xTickCount;
    TickType_t  xPendedTicks;
    TickType_t  xNextTaskUnblockTime;
    BaseType_t  xYieldPendings[1];
    List_t      xDelayedTaskList1;
    List_t      xDelayedTaskList2;
    List_t *    pxDelayedTaskList;
    List_t *    pxOverflowDelayedTaskList;
#else
    volatile TCB_t *     pxCurrentTCB;
    List_t               xReadyTasksList;
    List_t               xPendingReadyList;
    volatile UBaseType_t uxSchedulerSuspended;
    volatile UBaseType_t uxCurrentNumberOfTasks;
    volatile TickType_t  xTickCount;
    volatile TickType_t  xPendedTicks;
    volatile TickType_t  xNextTaskUnblockTime;
    volatile BaseType_t  xYieldPendings[1];
    List_t               xDelayedTaskList1;
    List_t               xDelayedTaskList2;
    List_t * volatile    pxDelayedTaskList;
    List_t * volatile    pxOverflowDelayedTaskList;
#endif

/*@
  requires xReadyTasksList.uxNumberOfItems > (UBaseType_t)0U;
  requires ReadyList(&xReadyTasksList);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns pxCurrentTCB, xYieldPendings[0];

  behavior suspended:
    assumes uxSchedulerSuspended != (UBaseType_t)0U;
    ensures xYieldPendings[0] == pdTRUE;
    ensures pxCurrentTCB == \old(pxCurrentTCB);

  behavior running:
    assumes uxSchedulerSuspended == (UBaseType_t)0U;
    ensures xYieldPendings[0] == pdFALSE;
    ensures pxCurrentTCB == (TCB_t *)Head(&xReadyTasksList)->pvOwner;
    ensures EDFProperty(&xReadyTasksList, pxCurrentTCB);

  complete behaviors;
  disjoint behaviors;
*/
void vTaskSwitchContext(void);

/*@
  requires uxSchedulerSuspended == (UBaseType_t)0U;

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns uxSchedulerSuspended;

  ensures uxSchedulerSuspended == (UBaseType_t)1U;
*/
void vTaskSuspendAll(void) {
    traceENTER_vTaskSuspendAll();
    uxSchedulerSuspended = (UBaseType_t)(uxSchedulerSuspended + 1U);
    traceRETURN_vTaskSuspendAll();
}

/* The MSP430 port's yield wrapper is assembly/context-save code around
 * vTaskSwitchContext.  Keep the call sequence visible and stub the port
 * operations at their contracts.
 */

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns \nothing;
*/
void vPortYieldPushStatusRegister(void);

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns \nothing;
*/
void vPortYieldDisableInterrupts(void);

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns \nothing;
*/
void vPortYieldSaveContext(void);

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns \nothing;
*/
void vPortYieldRestoreContext(void);

#undef portDISABLE_INTERRUPTS
#define portDISABLE_INTERRUPTS() vPortYieldDisableInterrupts()

#undef portSAVE_CONTEXT
#define portSAVE_CONTEXT() vPortYieldSaveContext()

#undef portRESTORE_CONTEXT
#define portRESTORE_CONTEXT() vPortYieldRestoreContext()

/*@
  requires uxSchedulerSuspended == (UBaseType_t)0U;
  requires xReadyTasksList.uxNumberOfItems > (UBaseType_t)0U;
  requires ReadyList(&xReadyTasksList);
  requires SchedulerListContext(&xReadyTasksList,
                                pxDelayedTaskList,
                                pxOverflowDelayedTaskList);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns pxCurrentTCB,
          xYieldPendings[0];

  ensures EDFProperty(&xReadyTasksList, pxCurrentTCB);
  ensures SchedulerListContext(&xReadyTasksList,
                               pxDelayedTaskList,
                               pxOverflowDelayedTaskList);
*/
void vPortYield(void) {
#ifdef __FRAMAC__
    vPortYieldPushStatusRegister();
#else
    asm volatile("push.w  sr");
#endif

    portDISABLE_INTERRUPTS();
    portSAVE_CONTEXT();

    vTaskSwitchContext();

    portRESTORE_CONTEXT();
}

#undef portYIELD
#define portYIELD() vPortYield()

#undef portYIELD_WITHIN_API
#define portYIELD_WITHIN_API() portYIELD()

#define taskYIELD_WITHIN_API() portYIELD_WITHIN_API()

#define taskYIELD_TASK_CORE_IF_USING_PREEMPTION(pxTCB) \
    do {                                               \
        (void)(pxTCB);                                 \
        portYIELD_WITHIN_API();                        \
    } while (0)

/*@
  requires SchedulerListContext(&xReadyTasksList,
                                pxDelayedTaskList,
                                pxOverflowDelayedTaskList);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns xNextTaskUnblockTime;

  ensures SchedulerListContext(&xReadyTasksList,
                               pxDelayedTaskList,
                               pxOverflowDelayedTaskList);
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
  requires uxSchedulerSuspended == (UBaseType_t)1U;
  requires uxCurrentNumberOfTasks > (UBaseType_t)0U;
  requires xYieldPendings[0] == pdTRUE || xYieldPendings[0] == pdFALSE;
  requires xPendedTicks == (TickType_t)0U;
  requires ListInv(&xPendingReadyList);
  requires xPendingReadyList.uxNumberOfItems == (UBaseType_t)0U;
  requires SchedulerListContext(&xReadyTasksList,
                                pxDelayedTaskList,
                                pxOverflowDelayedTaskList);
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
          pxCurrentTCB,
          xYieldPendings[0];

  ensures uxSchedulerSuspended == (UBaseType_t)0U;
  ensures \result == pdTRUE || \result == pdFALSE;
  ensures SchedulerListContext(&xReadyTasksList,
                               pxDelayedTaskList,
                               pxOverflowDelayedTaskList);

  behavior yield_performed:
    assumes xYieldPendings[0] != pdFALSE;
    ensures \result == pdTRUE;
    ensures EDFProperty(&xReadyTasksList, pxCurrentTCB);

  behavior no_yield:
    assumes xYieldPendings[0] == pdFALSE;
    ensures \result == pdFALSE;
    ensures pxCurrentTCB == \old(pxCurrentTCB);

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
                          loop assigns xPendedCounts, xYieldPendings[0];
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
                        //@ assert uxSchedulerSuspended == (UBaseType_t)0U;
                        //@ assert xReadyTasksList.uxNumberOfItems > (UBaseType_t)0U;
                        //@ assert ReadyList(&xReadyTasksList);
                        /*@ assert SchedulerListContext(&xReadyTasksList,
                                                         pxDelayedTaskList,
                                                         pxOverflowDelayedTaskList); */
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
                                pxDelayedTaskList,
                                pxOverflowDelayedTaskList,
                                &pxCurrentTCB->xStateListItem);
  requires ListInv(&xPendingReadyList);
  requires xPendingReadyList.uxNumberOfItems == (UBaseType_t)0U;
  requires &xPendingReadyList != &xReadyTasksList;
  requires &xPendingReadyList != pxDelayedTaskList;
  requires &xPendingReadyList != pxOverflowDelayedTaskList;
  requires \valid(pxCurrentTCB);
  requires xReadyTasksList.uxNumberOfItems > (UBaseType_t)0U;
  requires pxCurrentTCB->xEventListItem.pxContainer == \null;
  requires \separated(&pxCurrentTCB,
                      &xTickCount,
                      &xNextTaskUnblockTime,
                      &pxDelayedTaskList,
                      &pxOverflowDelayedTaskList,
                      &xReadyTasksList.uxNumberOfItems,
                      &pxDelayedTaskList->uxNumberOfItems,
                      &pxOverflowDelayedTaskList->uxNumberOfItems,
                      &pxCurrentTCB->xStateListItem.xItemValue,
                      &pxCurrentTCB->xStateListItem.pxContainer);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns xReadyTasksList.uxNumberOfItems,
          pxDelayedTaskList->uxNumberOfItems,
          pxOverflowDelayedTaskList->uxNumberOfItems,
          pxCurrentTCB->xStateListItem.xItemValue,
          pxCurrentTCB->xStateListItem.pxContainer,
          xNextTaskUnblockTime;

  ensures SchedulerListContext(&xReadyTasksList,
                               pxDelayedTaskList,
                               pxOverflowDelayedTaskList);
  ensures ListInv(&xPendingReadyList);
  ensures xPendingReadyList.uxNumberOfItems ==
    \old(xPendingReadyList.uxNumberOfItems);
  ensures !In(&pxCurrentTCB->xStateListItem, &xReadyTasksList);
  ensures xReadyTasksList.uxNumberOfItems ==
    (UBaseType_t)(\old(xReadyTasksList.uxNumberOfItems) - 1U);
  ensures pxCurrentTCB->xStateListItem.xItemValue ==
            (TickType_t)(\old(xTickCount) + xTicksToWait);

  behavior wake_time_overflows:
    assumes (TickType_t)(xTickCount + xTicksToWait) < xTickCount;
    ensures In(&pxCurrentTCB->xStateListItem, pxOverflowDelayedTaskList);
    ensures !In(&pxCurrentTCB->xStateListItem, pxDelayedTaskList);
    ensures xNextTaskUnblockTime == \old(xNextTaskUnblockTime);

  behavior wake_time_current_tick_window:
    assumes (TickType_t)(xTickCount + xTicksToWait) >= xTickCount;
    ensures In(&pxCurrentTCB->xStateListItem, pxDelayedTaskList);
    ensures !In(&pxCurrentTCB->xStateListItem, pxOverflowDelayedTaskList);
    ensures ((TickType_t)(\old(xTickCount) + xTicksToWait) <
             \old(xNextTaskUnblockTime)) ==>
              xNextTaskUnblockTime ==
                (TickType_t)(\old(xTickCount) + xTicksToWait);
    ensures ((TickType_t)(\old(xTickCount) + xTicksToWait) >=
             \old(xNextTaskUnblockTime)) ==>
              xNextTaskUnblockTime == \old(xNextTaskUnblockTime);

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

    /* vTaskDelay always requests a finite delay, so the suspended-list branch
     * of the general helper is outside this extraction. */
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
  requires uxSchedulerSuspended == (UBaseType_t)0U;
  requires SchedulerListContext(&xReadyTasksList,
                                pxDelayedTaskList,
                                pxOverflowDelayedTaskList);
  requires \valid(pxCurrentTCB);
  requires \valid(pxPreviousWakeTime);
  requires xTimeIncrement > (TickType_t)0U;
  requires DelayUntilShouldDelay(*pxPreviousWakeTime,
                                 xTimeIncrement,
                                 xTickCount);
  requires uxCurrentNumberOfTasks > (UBaseType_t)0U;
  requires xYieldPendings[0] == pdTRUE || xYieldPendings[0] == pdFALSE;
  requires xPendedTicks == (TickType_t)0U;
  requires ListInv(&xPendingReadyList);
  requires xPendingReadyList.uxNumberOfItems == (UBaseType_t)0U;
  requires &xPendingReadyList != &xReadyTasksList;
  requires &xPendingReadyList != pxDelayedTaskList;
  requires &xPendingReadyList != pxOverflowDelayedTaskList;
  requires xReadyTasksList.uxNumberOfItems > (UBaseType_t)1U;
  requires In(&pxCurrentTCB->xStateListItem, &xReadyTasksList);
  requires pxCurrentTCB->xEventListItem.pxContainer == \null;
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, &xReadyTasksList) &&
    item != &pxCurrentTCB->xStateListItem ==>
      \separated(&pxCurrentTCB->xDeadline,
                 &((TCB_t *)item->pvOwner)->xDeadline);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, &xReadyTasksList) &&
    item != &pxCurrentTCB->xStateListItem ==>
      \separated(pxPreviousWakeTime,
                 &((TCB_t *)item->pvOwner)->xDeadline);
  requires \forall ListItem_t *item;
    \valid(item) ==>
      TickSeparatedFromListItem(pxPreviousWakeTime, item);
  requires \forall ListItem_t *item;
    \valid(item) ==>
      TickSeparatedFromListItem(&pxCurrentTCB->xDeadline, item);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, &xReadyTasksList) &&
    item != &pxCurrentTCB->xStateListItem ==>
      TickSeparatedFromTask(&pxCurrentTCB->xDeadline,
                            (TCB_t *)item->pvOwner);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, pxDelayedTaskList) ==>
      TickSeparatedFromTask(&pxCurrentTCB->xDeadline,
                            (TCB_t *)item->pvOwner);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, pxOverflowDelayedTaskList) ==>
      TickSeparatedFromTask(&pxCurrentTCB->xDeadline,
                            (TCB_t *)item->pvOwner);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, &xReadyTasksList) ==>
      TickSeparatedFromTask(pxPreviousWakeTime,
                            (TCB_t *)item->pvOwner);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, pxDelayedTaskList) ==>
      TickSeparatedFromTask(pxPreviousWakeTime,
                            (TCB_t *)item->pvOwner);
  requires \forall ListItem_t *item;
    \valid(item) &&
    In(item, pxOverflowDelayedTaskList) ==>
      TickSeparatedFromTask(pxPreviousWakeTime,
                            (TCB_t *)item->pvOwner);
  requires \separated(&pxCurrentTCB->xDeadline, &xPendingReadyList);
  requires \separated(&pxCurrentTCB->xDeadline, &xReadyTasksList);
  requires \separated(&pxCurrentTCB->xDeadline, pxDelayedTaskList);
  requires \separated(&pxCurrentTCB->xDeadline, pxOverflowDelayedTaskList);
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
                      &pxDelayedTaskList->uxNumberOfItems,
                      &pxOverflowDelayedTaskList->uxNumberOfItems,
                      &pxCurrentTCB->xDeadline,
                      &pxCurrentTCB->xStateListItem.xItemValue,
                      &pxCurrentTCB->xStateListItem.pxContainer);
  requires \separated(pxPreviousWakeTime, &xPendingReadyList);
  requires \separated(pxPreviousWakeTime, &xReadyTasksList);
  requires \separated(pxPreviousWakeTime, pxDelayedTaskList);
  requires \separated(pxPreviousWakeTime, pxOverflowDelayedTaskList);
  requires \separated(pxPreviousWakeTime, &pxCurrentTCB->xEventListItem);
  requires \separated(pxPreviousWakeTime, &pxCurrentTCB->xStateListItem);
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
                      &pxDelayedTaskList->uxNumberOfItems,
                      &pxOverflowDelayedTaskList->uxNumberOfItems,
                      &pxCurrentTCB->xDeadline,
                      &pxCurrentTCB->xStateListItem.xItemValue,
                      &pxCurrentTCB->xStateListItem.pxContainer);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns uxSchedulerSuspended,
          pxCurrentTCB,
          xYieldPendings[0],
          *pxPreviousWakeTime,
          \old(pxCurrentTCB)->xDeadline,
          xReadyTasksList.uxNumberOfItems,
          pxDelayedTaskList->uxNumberOfItems,
          pxOverflowDelayedTaskList->uxNumberOfItems,
          \old(pxCurrentTCB)->xStateListItem.xItemValue,
          \old(pxCurrentTCB)->xStateListItem.pxContainer,
          xNextTaskUnblockTime;

  ensures uxSchedulerSuspended == (UBaseType_t)0U;
  ensures \result == pdTRUE;
  ensures *pxPreviousWakeTime ==
            (TickType_t)(\old(*pxPreviousWakeTime) + xTimeIncrement);
  ensures \old(pxCurrentTCB)->xDeadline ==
            (TickType_t)(\old(*pxPreviousWakeTime) +
                         xTimeIncrement +
                         xTimeIncrement);
  ensures SchedulerListContext(&xReadyTasksList,
                               pxDelayedTaskList,
                               pxOverflowDelayedTaskList);
  ensures EDFProperty(&xReadyTasksList, pxCurrentTCB);
  ensures !In(&\old(pxCurrentTCB)->xStateListItem, &xReadyTasksList);

  behavior tick_count_overflowed_and_wake_is_future:
    assumes xTickCount < *pxPreviousWakeTime;
    assumes (TickType_t)(*pxPreviousWakeTime + xTimeIncrement) <
              *pxPreviousWakeTime;
    assumes (TickType_t)(*pxPreviousWakeTime + xTimeIncrement) >
              xTickCount;
    ensures In(&\old(pxCurrentTCB)->xStateListItem, pxDelayedTaskList);
    ensures !In(&\old(pxCurrentTCB)->xStateListItem,
                pxOverflowDelayedTaskList);

  behavior wake_time_overflows:
    assumes xTickCount >= *pxPreviousWakeTime;
    assumes (TickType_t)(*pxPreviousWakeTime + xTimeIncrement) <
              *pxPreviousWakeTime;
    ensures In(&\old(pxCurrentTCB)->xStateListItem,
               pxOverflowDelayedTaskList);
    ensures !In(&\old(pxCurrentTCB)->xStateListItem, pxDelayedTaskList);

  behavior wake_time_is_future:
    assumes xTickCount >= *pxPreviousWakeTime;
    assumes (TickType_t)(*pxPreviousWakeTime + xTimeIncrement) >
              xTickCount;
    ensures In(&\old(pxCurrentTCB)->xStateListItem, pxDelayedTaskList);
    ensures !In(&\old(pxCurrentTCB)->xStateListItem,
                pxOverflowDelayedTaskList);

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
  requires uxSchedulerSuspended == (UBaseType_t)0U;
  requires SchedulerListContext(&xReadyTasksList,
                                pxDelayedTaskList,
                                pxOverflowDelayedTaskList);
  requires \valid(pxCurrentTCB);
  requires uxCurrentNumberOfTasks > (UBaseType_t)0U;
  requires xYieldPendings[0] == pdTRUE || xYieldPendings[0] == pdFALSE;
  requires xPendedTicks == (TickType_t)0U;
  requires ListInv(&xPendingReadyList);
  requires xPendingReadyList.uxNumberOfItems == (UBaseType_t)0U;
  requires &xPendingReadyList != &xReadyTasksList;
  requires &xPendingReadyList != pxDelayedTaskList;
  requires &xPendingReadyList != pxOverflowDelayedTaskList;
  requires xReadyTasksList.uxNumberOfItems > (UBaseType_t)0U;
  requires xTicksToDelay == (TickType_t)0U ||
           xReadyTasksList.uxNumberOfItems > (UBaseType_t)1U;
  requires In(&pxCurrentTCB->xStateListItem, &xReadyTasksList);
  requires pxCurrentTCB->xEventListItem.pxContainer == \null;
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
                      &pxDelayedTaskList->uxNumberOfItems,
                      &pxOverflowDelayedTaskList->uxNumberOfItems,
                      &pxCurrentTCB->xStateListItem.xItemValue,
                      &pxCurrentTCB->xStateListItem.pxContainer);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns uxSchedulerSuspended,
          pxCurrentTCB,
          xYieldPendings[0],
          xReadyTasksList.uxNumberOfItems,
          pxDelayedTaskList->uxNumberOfItems,
          pxOverflowDelayedTaskList->uxNumberOfItems,
          pxCurrentTCB->xStateListItem.xItemValue,
          pxCurrentTCB->xStateListItem.pxContainer,
          xNextTaskUnblockTime;

  ensures uxSchedulerSuspended == (UBaseType_t)0U;
  ensures SchedulerListContext(&xReadyTasksList,
                               pxDelayedTaskList,
                               pxOverflowDelayedTaskList);
  ensures EDFProperty(&xReadyTasksList, pxCurrentTCB);

  behavior zero_delay:
    assumes xTicksToDelay == (TickType_t)0U;
    ensures In(&pxCurrentTCB->xStateListItem, &xReadyTasksList);
    ensures xNextTaskUnblockTime == \old(xNextTaskUnblockTime);

  behavior finite_delay_resume_yielded:
    assumes xTicksToDelay > (TickType_t)0U;
    assumes xYieldPendings[0] != pdFALSE;
    ensures !In(&\old(pxCurrentTCB)->xStateListItem, &xReadyTasksList);
    ensures ((TickType_t)(\old(xTickCount) + xTicksToDelay) <
             \old(xTickCount)) ==>
              In(&\old(pxCurrentTCB)->xStateListItem, pxOverflowDelayedTaskList);
    ensures ((TickType_t)(\old(xTickCount) + xTicksToDelay) >=
             \old(xTickCount)) ==>
              In(&\old(pxCurrentTCB)->xStateListItem, pxDelayedTaskList);

  behavior finite_delay_needs_yield:
    assumes xTicksToDelay > (TickType_t)0U;
    assumes xYieldPendings[0] == pdFALSE;
    ensures !In(&\old(pxCurrentTCB)->xStateListItem, &xReadyTasksList);
    ensures ((TickType_t)(\old(xTickCount) + xTicksToDelay) <
             \old(xTickCount)) ==>
              In(&\old(pxCurrentTCB)->xStateListItem, pxOverflowDelayedTaskList);
    ensures ((TickType_t)(\old(xTickCount) + xTicksToDelay) >=
             \old(xTickCount)) ==>
              In(&\old(pxCurrentTCB)->xStateListItem, pxDelayedTaskList);

  complete behaviors;
  disjoint behaviors;
*/
void vTaskDelay(const TickType_t xTicksToDelay) {
    BaseType_t xAlreadyYielded = pdFALSE;

    traceENTER_vTaskDelay(xTicksToDelay);

    /* A delay time of zero just forces a reschedule. */
    if (xTicksToDelay > (TickType_t)0U) {
        vTaskSuspendAll();
        {
            //@ assert uxSchedulerSuspended == (UBaseType_t)1U;

            traceTASK_DELAY();

            /* A task that is removed from the event list while the
             * scheduler is suspended will not get placed in the ready
             * list or removed from the blocked list until the scheduler
             * is resumed.
             *
             * This task cannot be in an event list as it is the currently
             * executing task. */
            prvAddCurrentTaskToDelayedList(xTicksToDelay, pdFALSE);
        }
        xAlreadyYielded = xTaskResumeAll();
    } else {
        mtCOVERAGE_TEST_MARKER();
    }

    /* Force a reschedule if xTaskResumeAll has not already done so, we may
     * have put ourselves to sleep. */
    if (xAlreadyYielded == pdFALSE) {
        taskYIELD_WITHIN_API();
    } else {
        mtCOVERAGE_TEST_MARKER();
    }

    traceRETURN_vTaskDelay();
}
