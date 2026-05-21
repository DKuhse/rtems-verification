/*
 * Verification overlay (reference) for vTaskDelay.
 *
 * The public API is source-shaped, but the surrounding scheduler operations are
 * kept as small contracts.  xTaskResumeAll deliberately does not model the
 * pended-tick drain; that path is owned by the xTaskIncrementTick effort.
 *
 * This slice proves the finite-delay move, yield behavior, and scheduler-list
 * context preservation.
 */

#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "FreeRTOS.h"
#include "list.h"
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* Minimal TCB fields dereferenced by vTaskDelay's finite-delay path. */
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

/* Hack: Frama-C can't handle volatile. */
#ifdef __FRAMAC__
    TCB_t *     pxCurrentTCB;
    List_t      xReadyTasksList;
    UBaseType_t uxSchedulerSuspended;
    TickType_t  xTickCount;
    TickType_t  xNextTaskUnblockTime;
    BaseType_t  xYieldPendings[1];
    List_t      xDelayedTaskList1;
    List_t      xDelayedTaskList2;
    List_t *    pxDelayedTaskList;
    List_t *    pxOverflowDelayedTaskList;
    BaseType_t  xTaskResumeAllReturn;
    BaseType_t  xYieldWithinAPICalled;
#else
    volatile TCB_t *     pxCurrentTCB;
    List_t               xReadyTasksList;
    volatile UBaseType_t uxSchedulerSuspended;
    volatile TickType_t  xTickCount;
    volatile TickType_t  xNextTaskUnblockTime;
    volatile BaseType_t  xYieldPendings[1];
    List_t               xDelayedTaskList1;
    List_t               xDelayedTaskList2;
    List_t * volatile    pxDelayedTaskList;
    List_t * volatile    pxOverflowDelayedTaskList;
    volatile BaseType_t  xTaskResumeAllReturn;
    volatile BaseType_t  xYieldWithinAPICalled;
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

/*@
  requires uxSchedulerSuspended == (UBaseType_t)1U;
  requires xTaskResumeAllReturn == pdTRUE ||
           xTaskResumeAllReturn == pdFALSE;
  requires SchedulerListContext(&xReadyTasksList,
                                pxDelayedTaskList,
                                pxOverflowDelayedTaskList);
  requires xTaskResumeAllReturn == pdFALSE ||
           xReadyTasksList.uxNumberOfItems > (UBaseType_t)0U;
  requires xTaskResumeAllReturn == pdFALSE ||
           ReadyList(&xReadyTasksList);
  requires \separated(&pxCurrentTCB,
                      &uxSchedulerSuspended,
                      &xTaskResumeAllReturn,
                      &xYieldPendings[0]);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns uxSchedulerSuspended,
          pxCurrentTCB,
          xYieldPendings[0];

  ensures uxSchedulerSuspended == (UBaseType_t)0U;
  ensures \result == xTaskResumeAllReturn;
  ensures \result == pdTRUE || \result == pdFALSE;
  ensures SchedulerListContext(&xReadyTasksList,
                               pxDelayedTaskList,
                               pxOverflowDelayedTaskList);

  behavior yield_performed:
    assumes xTaskResumeAllReturn == pdTRUE;
    ensures EDFProperty(&xReadyTasksList, pxCurrentTCB);

  behavior no_yield:
    assumes xTaskResumeAllReturn == pdFALSE;
    ensures pxCurrentTCB == \old(pxCurrentTCB);

  complete behaviors;
  disjoint behaviors;
*/
BaseType_t xTaskResumeAll(void) {
    BaseType_t xAlreadyYielded = xTaskResumeAllReturn;

    traceENTER_xTaskResumeAll();
    uxSchedulerSuspended = (UBaseType_t)(uxSchedulerSuspended - 1U);

    if (xAlreadyYielded != pdFALSE) {
        vTaskSwitchContext();
    } else {
        mtCOVERAGE_TEST_MARKER();
    }

    traceRETURN_xTaskResumeAll(xAlreadyYielded);

    return xAlreadyYielded;
}

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

  assigns xYieldWithinAPICalled,
          pxCurrentTCB,
          xYieldPendings[0];

  ensures xYieldWithinAPICalled == pdTRUE;
  ensures EDFProperty(&xReadyTasksList, pxCurrentTCB);
  ensures SchedulerListContext(&xReadyTasksList,
                               pxDelayedTaskList,
                               pxOverflowDelayedTaskList);
*/
static void vTaskDelayYieldWithinAPI(void) {
    xYieldWithinAPICalled = pdTRUE;
    vTaskSwitchContext();
}

#define taskYIELD_WITHIN_API() vTaskDelayYieldWithinAPI()

/*@
  requires xTicksToWait > (TickType_t)0U;
  requires xCanBlockIndefinitely == pdFALSE;
  requires SchedulerListContext(&xReadyTasksList,
                                pxDelayedTaskList,
                                pxOverflowDelayedTaskList);
  requires \valid(pxCurrentTCB);
  requires xReadyTasksList.uxNumberOfItems > (UBaseType_t)0U;
  requires In(&pxCurrentTCB->xStateListItem, &xReadyTasksList);
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
  requires uxSchedulerSuspended == (UBaseType_t)0U;
  requires xTaskResumeAllReturn == pdTRUE ||
           xTaskResumeAllReturn == pdFALSE;
  requires SchedulerListContext(&xReadyTasksList,
                                pxDelayedTaskList,
                                pxOverflowDelayedTaskList);
  requires \valid(pxCurrentTCB);
  requires xReadyTasksList.uxNumberOfItems > (UBaseType_t)0U;
  requires xTicksToDelay == (TickType_t)0U ||
           xReadyTasksList.uxNumberOfItems > (UBaseType_t)1U;
  requires In(&pxCurrentTCB->xStateListItem, &xReadyTasksList);
  requires pxCurrentTCB->xEventListItem.pxContainer == \null;
  requires \separated(&pxCurrentTCB,
                      &uxSchedulerSuspended,
                      &xTickCount,
                      &xNextTaskUnblockTime,
                      &pxDelayedTaskList,
                      &pxOverflowDelayedTaskList,
                      &xTaskResumeAllReturn,
                      &xYieldWithinAPICalled,
                      &xYieldPendings[0],
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
          xYieldWithinAPICalled,
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
    ensures xYieldWithinAPICalled == pdTRUE;
    ensures xNextTaskUnblockTime == \old(xNextTaskUnblockTime);

  behavior finite_delay_resume_yielded:
    assumes xTicksToDelay > (TickType_t)0U;
    assumes xTaskResumeAllReturn == pdTRUE;
    ensures !In(&\old(pxCurrentTCB)->xStateListItem, &xReadyTasksList);
    ensures ((TickType_t)(\old(xTickCount) + xTicksToDelay) <
             \old(xTickCount)) ==>
              In(&\old(pxCurrentTCB)->xStateListItem, pxOverflowDelayedTaskList);
    ensures ((TickType_t)(\old(xTickCount) + xTicksToDelay) >=
             \old(xTickCount)) ==>
              In(&\old(pxCurrentTCB)->xStateListItem, pxDelayedTaskList);

  behavior finite_delay_needs_yield:
    assumes xTicksToDelay > (TickType_t)0U;
    assumes xTaskResumeAllReturn == pdFALSE;
    ensures !In(&\old(pxCurrentTCB)->xStateListItem, &xReadyTasksList);
    ensures ((TickType_t)(\old(xTickCount) + xTicksToDelay) <
             \old(xTickCount)) ==>
              In(&\old(pxCurrentTCB)->xStateListItem, pxOverflowDelayedTaskList);
    ensures ((TickType_t)(\old(xTickCount) + xTicksToDelay) >=
             \old(xTickCount)) ==>
              In(&\old(pxCurrentTCB)->xStateListItem, pxDelayedTaskList);
    ensures xYieldWithinAPICalled == pdTRUE;

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
