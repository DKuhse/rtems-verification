/*
 * Verification overlay (reference) for vTaskSuspend.
 *
 * This slice covers the single-core, scheduler-running path.  It proves that
 * the suspended task's state item is removed from whichever scheduler-owned
 * state list contains it and is inserted into xSuspendedTaskList, while the
 * scheduler/suspended-list context is preserved.
 */

#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "FreeRTOS.h"
#include "list.h"
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* The notification loop is outside this first suspend-list proof slice. */
#undef configUSE_TASK_NOTIFICATIONS
#define configUSE_TASK_NOTIFICATIONS 0

/* Keep assertions side-effect free in this reference slice. */
#undef configASSERT
#define configASSERT(x) ((void)(x))

/* Minimal TCB fields dereferenced by vTaskSuspend's scheduler-running path. */
struct tskTaskControlBlock {
    ListItem_t xStateListItem;
    ListItem_t xEventListItem;
    TickType_t xDeadline;
};
typedef struct tskTaskControlBlock TCB_t;
typedef TCB_t * TaskHandle_t;

#define FREERTOS_USE_ABSTRACT_LIST_MODEL
#include "scheduler_model.h"

/* Route direct list calls through the scheduler model. */
#define uxListRemove(pxItemToRemove) vSchedulerListRemove_abs((pxItemToRemove))

/* EDF builds do not maintain priority ready bitmaps. */
#define taskRESET_READY_PRIORITY(uxPriority)

/* Critical sections are outside this sequential reference slice. */
#define taskENTER_CRITICAL()
#define taskEXIT_CRITICAL()

#define prvGetTCBFromHandle(pxHandle) (((pxHandle) == NULL) ? pxCurrentTCB : (pxHandle))

/* Hack: Frama-C can't handle volatile. */
#ifdef __FRAMAC__
    TCB_t *     pxCurrentTCB;
    List_t      xReadyTasksList;
    List_t      xSuspendedTaskList;
    UBaseType_t uxSchedulerSuspended;
    TickType_t  xNextTaskUnblockTime;
    BaseType_t  xSchedulerRunning;
    UBaseType_t uxCurrentNumberOfTasks;
    BaseType_t  xYieldWithinAPICalled;
    BaseType_t  xYieldPendings[1];
    List_t      xDelayedTaskList1;
    List_t      xDelayedTaskList2;
    List_t *    pxDelayedTaskList;
    List_t *    pxOverflowDelayedTaskList;
#else
    volatile TCB_t *     pxCurrentTCB;
    List_t               xReadyTasksList;
    List_t               xSuspendedTaskList;
    volatile UBaseType_t uxSchedulerSuspended;
    volatile TickType_t  xNextTaskUnblockTime;
    volatile BaseType_t  xSchedulerRunning;
    volatile UBaseType_t uxCurrentNumberOfTasks;
    volatile BaseType_t  xYieldWithinAPICalled;
    volatile BaseType_t  xYieldPendings[1];
    List_t               xDelayedTaskList1;
    List_t               xDelayedTaskList2;
    List_t * volatile    pxDelayedTaskList;
    List_t * volatile    pxOverflowDelayedTaskList;
#endif

/*@
  requires SchedulerSuspendedListContext(&xReadyTasksList,
                                         pxDelayedTaskList,
                                         pxOverflowDelayedTaskList,
                                         &xSuspendedTaskList);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns xNextTaskUnblockTime;

  ensures SchedulerSuspendedListContext(&xReadyTasksList,
                                        pxDelayedTaskList,
                                        pxOverflowDelayedTaskList,
                                        &xSuspendedTaskList);
  ensures EDFProperty{Pre}(&xReadyTasksList, pxCurrentTCB) ==>
    EDFProperty(&xReadyTasksList, pxCurrentTCB);
*/
static void prvResetNextTaskUnblockTime(void);

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
  requires xReadyTasksList.uxNumberOfItems > (UBaseType_t)0U;
  requires ReadyList(&xReadyTasksList);
  requires SchedulerSuspendedListContext(&xReadyTasksList,
                                         pxDelayedTaskList,
                                         pxOverflowDelayedTaskList,
                                         &xSuspendedTaskList);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns xYieldWithinAPICalled,
          pxCurrentTCB,
          xYieldPendings[0];

  ensures xYieldWithinAPICalled == pdTRUE;
  ensures EDFProperty(&xReadyTasksList, pxCurrentTCB);
  ensures SchedulerSuspendedListContext(&xReadyTasksList,
                                        pxDelayedTaskList,
                                        pxOverflowDelayedTaskList,
                                        &xSuspendedTaskList);
*/
static void vTaskSuspendYieldWithinAPI(void) {
    xYieldWithinAPICalled = pdTRUE;
    vTaskSwitchContext();
}

#undef portYIELD_WITHIN_API
#define portYIELD_WITHIN_API() vTaskSuspendYieldWithinAPI()

/*@
  requires xSchedulerRunning != pdFALSE;
  requires uxSchedulerSuspended == (UBaseType_t)0U;
  requires xYieldWithinAPICalled == pdFALSE ||
           xYieldWithinAPICalled == pdTRUE;
  requires \valid(pxCurrentTCB);
  requires EDFProperty(&xReadyTasksList, pxCurrentTCB);
  requires (xTaskToSuspend == \null || xTaskToSuspend == pxCurrentTCB) ==>
    xReadyTasksList.uxNumberOfItems > (UBaseType_t)1U;
  requires xTaskToSuspend == \null || \valid(xTaskToSuspend);
  requires \valid((xTaskToSuspend == \null) ? pxCurrentTCB : xTaskToSuspend);
  requires TaskItem(&((xTaskToSuspend == \null) ?
                      pxCurrentTCB : xTaskToSuspend)->xStateListItem);
  requires ((In(&((xTaskToSuspend == \null) ?
                  pxCurrentTCB : xTaskToSuspend)->xStateListItem,
                &xReadyTasksList)) ||
            (In(&((xTaskToSuspend == \null) ?
                  pxCurrentTCB : xTaskToSuspend)->xStateListItem,
                pxDelayedTaskList)) ||
            (In(&((xTaskToSuspend == \null) ?
                  pxCurrentTCB : xTaskToSuspend)->xStateListItem,
                pxOverflowDelayedTaskList)));
  requires ((xTaskToSuspend == \null) ?
            pxCurrentTCB : xTaskToSuspend)->xEventListItem.pxContainer == \null;
  requires SchedulerSuspendedListContext(&xReadyTasksList,
                                         pxDelayedTaskList,
                                         pxOverflowDelayedTaskList,
                                         &xSuspendedTaskList);
  requires \separated(&pxCurrentTCB,
                      &uxSchedulerSuspended,
                      &xSchedulerRunning,
                      &xNextTaskUnblockTime,
                      &xYieldWithinAPICalled,
                      &xYieldPendings[0],
                      &xReadyTasksList.uxNumberOfItems,
                      &pxDelayedTaskList->uxNumberOfItems,
                      &pxOverflowDelayedTaskList->uxNumberOfItems,
                      &xSuspendedTaskList.uxNumberOfItems,
                      &((xTaskToSuspend == \null) ?
                        pxCurrentTCB : xTaskToSuspend)->xStateListItem.pxContainer);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns xReadyTasksList.uxNumberOfItems,
          pxDelayedTaskList->uxNumberOfItems,
          pxOverflowDelayedTaskList->uxNumberOfItems,
          xSuspendedTaskList.uxNumberOfItems,
          ((xTaskToSuspend == \null) ?
            pxCurrentTCB : xTaskToSuspend)->xStateListItem.pxContainer,
          xNextTaskUnblockTime,
          xYieldWithinAPICalled,
          pxCurrentTCB,
          xYieldPendings[0];

  ensures uxSchedulerSuspended == \old(uxSchedulerSuspended);
  ensures SchedulerSuspendedListContext(&xReadyTasksList,
                                        pxDelayedTaskList,
                                        pxOverflowDelayedTaskList,
                                        &xSuspendedTaskList);
  ensures EDFProperty(&xReadyTasksList, pxCurrentTCB);
  ensures In(&((xTaskToSuspend == \null) ?
               \old(pxCurrentTCB) : xTaskToSuspend)->xStateListItem,
             &xSuspendedTaskList);
  ensures !In(&((xTaskToSuspend == \null) ?
                \old(pxCurrentTCB) : xTaskToSuspend)->xStateListItem,
              &xReadyTasksList);
  ensures !In(&((xTaskToSuspend == \null) ?
                \old(pxCurrentTCB) : xTaskToSuspend)->xStateListItem,
              pxDelayedTaskList);
  ensures !In(&((xTaskToSuspend == \null) ?
                \old(pxCurrentTCB) : xTaskToSuspend)->xStateListItem,
              pxOverflowDelayedTaskList);

  behavior suspend_current:
    assumes xTaskToSuspend == \null || xTaskToSuspend == pxCurrentTCB;
    ensures xYieldWithinAPICalled == pdTRUE;

  behavior suspend_other:
    assumes xTaskToSuspend != \null && xTaskToSuspend != pxCurrentTCB;
    ensures pxCurrentTCB == \old(pxCurrentTCB);
    ensures xYieldWithinAPICalled == \old(xYieldWithinAPICalled);

  complete behaviors;
  disjoint behaviors;
*/
void vTaskSuspend(TaskHandle_t xTaskToSuspend) {
    TCB_t* pxTCB;

    traceENTER_vTaskSuspend(xTaskToSuspend);

    taskENTER_CRITICAL();
    {
        /* If null is passed in here then it is the running task that is
         * being suspended. */
        pxTCB = prvGetTCBFromHandle(xTaskToSuspend);
        configASSERT(pxTCB != NULL);

        traceTASK_SUSPEND(pxTCB);

        /* Remove task from the ready/delayed list and place in the
         * suspended list. */
#ifndef SANITY_PROBE
        /*@ assert In(&pxTCB->xStateListItem, &xReadyTasksList) ||
                   In(&pxTCB->xStateListItem, pxDelayedTaskList) ||
                   In(&pxTCB->xStateListItem, pxOverflowDelayedTaskList); */
        /*@ assert pxTCB->xStateListItem.pxContainer == &xReadyTasksList ||
                   pxTCB->xStateListItem.pxContainer == pxDelayedTaskList ||
                   pxTCB->xStateListItem.pxContainer == pxOverflowDelayedTaskList; */
        //@ assert ListInv(pxTCB->xStateListItem.pxContainer);
        //@ assert pxTCB->xStateListItem.pxContainer->uxNumberOfItems > (UBaseType_t)0U;
#endif
        if (uxListRemove(&(pxTCB->xStateListItem)) == (UBaseType_t)0) {
            taskRESET_READY_PRIORITY(pxTCB->uxPriority);
        } else {
            mtCOVERAGE_TEST_MARKER();
        }

        /* Is the task waiting on an event also? */
        if (listLIST_ITEM_CONTAINER(&(pxTCB->xEventListItem)) != NULL) {
            (void)uxListRemove(&(pxTCB->xEventListItem));
        } else {
            mtCOVERAGE_TEST_MARKER();
        }

        //@ assert Detached(&pxTCB->xStateListItem);
        //@ assert TaskItem(&pxTCB->xStateListItem);
        vListInsertEnd(&xSuspendedTaskList, &(pxTCB->xStateListItem));
#ifndef SANITY_PROBE
        //@ assert In(&pxTCB->xStateListItem, &xSuspendedTaskList);
        /*@ assert SchedulerSuspendedListContext(&xReadyTasksList,
                                                 pxDelayedTaskList,
                                                 pxOverflowDelayedTaskList,
                                                 &xSuspendedTaskList); */
#endif

#if (configUSE_TASK_NOTIFICATIONS == 1)
        {
            BaseType_t x;

            for (x = (BaseType_t)0; x < (BaseType_t)configTASK_NOTIFICATION_ARRAY_ENTRIES; x++) {
                if (pxTCB->ucNotifyState[x] == taskWAITING_NOTIFICATION) {
                    /* The task was blocked to wait for a notification, but is
                     * now suspended, so no notification was received. */
                    pxTCB->ucNotifyState[x] = taskNOT_WAITING_NOTIFICATION;
                }
            }
        }
#endif /* if ( configUSE_TASK_NOTIFICATIONS == 1 ) */
    }
    taskEXIT_CRITICAL();

#if (configNUMBER_OF_CORES == 1)
    {
        UBaseType_t uxCurrentListLength;

        if (xSchedulerRunning != pdFALSE) {
            /* Reset the next expected unblock time in case it referred to the
             * task that is now in the Suspended state. */
            taskENTER_CRITICAL();
            {
                prvResetNextTaskUnblockTime();
            }
            taskEXIT_CRITICAL();
        } else {
            mtCOVERAGE_TEST_MARKER();
        }

        if (pxTCB == pxCurrentTCB) {
            if (xSchedulerRunning != pdFALSE) {
                /* The current task has just been suspended. */
                configASSERT(uxSchedulerSuspended == 0);
                portYIELD_WITHIN_API();
            } else {
                /* The scheduler is not running, but the task that was pointed
                 * to by pxCurrentTCB has just been suspended and pxCurrentTCB
                 * must be adjusted to point to a different task. */

                /* Use a temp variable as a distinct sequence point for reading
                 * volatile variables prior to a comparison to ensure compliance
                 * with MISRA C 2012 Rule 13.2. */
                uxCurrentListLength = listCURRENT_LIST_LENGTH(&xSuspendedTaskList);

                if (uxCurrentListLength == uxCurrentNumberOfTasks) {
                    /* No other tasks are ready, so set pxCurrentTCB back to
                     * NULL so when the next task is created pxCurrentTCB will
                     * be set to point to it no matter what its relative priority
                     * is. */
                    pxCurrentTCB = NULL;
                } else {
                    vTaskSwitchContext();
                }
            }
        } else {
            mtCOVERAGE_TEST_MARKER();
        }
    }
#endif /* #if ( configNUMBER_OF_CORES == 1 ) */

    traceRETURN_vTaskSuspend();
}
