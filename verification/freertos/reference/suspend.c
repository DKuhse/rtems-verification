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
#include "freertos_volatile_instrumentation.h"

/* Route direct list calls through the scheduler model. */
#define uxListRemove(pxItemToRemove) vSchedulerListRemove_abs((pxItemToRemove))

/* EDF builds do not maintain priority ready bitmaps. */
#define taskRESET_READY_PRIORITY(uxPriority)

/* Critical sections are outside this sequential reference slice. */
#define taskENTER_CRITICAL()
#define taskEXIT_CRITICAL()

#define prvGetTCBFromHandle(pxHandle) (((pxHandle) == NULL) ? pxCurrentTCB : (pxHandle))

// 

/*@
  requires SchedulerSuspendedListContext(&xReadyTasksList,
                                         pxDelayedTaskList_ghost,
                                         pxOverflowDelayedTaskList_ghost,
                                         &xSuspendedTaskList);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns xNextTaskUnblockTime,
          xNextTaskUnblockTime_ghost;

  ensures SchedulerSuspendedListContext(&xReadyTasksList,
                                        pxDelayedTaskList_ghost,
                                        pxOverflowDelayedTaskList_ghost,
                                        &xSuspendedTaskList);
  ensures EDFProperty{Pre}(&xReadyTasksList, pxCurrentTCB_ghost) ==>
    EDFProperty(&xReadyTasksList, pxCurrentTCB_ghost);
*/
static void prvResetNextTaskUnblockTime(void);

/*@
  requires xReadyTasksList.uxNumberOfItems > (UBaseType_t)0U;
  requires ReadyList(&xReadyTasksList);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns pxCurrentTCB,
          pxCurrentTCB_ghost,
          xYieldPendings[0],
          xYieldPendings0_ghost;

  behavior suspended:
    assumes uxSchedulerSuspended_ghost != (UBaseType_t)0U;
    ensures xYieldPendings0_ghost == pdTRUE;
    ensures pxCurrentTCB_ghost == \old(pxCurrentTCB_ghost);

  behavior running:
    assumes uxSchedulerSuspended_ghost == (UBaseType_t)0U;
    ensures xYieldPendings0_ghost == pdFALSE;
    ensures pxCurrentTCB_ghost == (TCB_t *)Head(&xReadyTasksList)->pvOwner;
    ensures EDFProperty(&xReadyTasksList, pxCurrentTCB_ghost);

  complete behaviors;
  disjoint behaviors;
*/
void vTaskSwitchContext(void);

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
  requires uxSchedulerSuspended_ghost == (UBaseType_t)0U;
  requires xReadyTasksList.uxNumberOfItems > (UBaseType_t)0U;
  requires ReadyList(&xReadyTasksList);
  requires SchedulerSuspendedListContext(&xReadyTasksList,
                                         pxDelayedTaskList_ghost,
                                         pxOverflowDelayedTaskList_ghost,
                                         &xSuspendedTaskList);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns pxCurrentTCB,
          pxCurrentTCB_ghost,
          xYieldPendings[0],
          xYieldPendings0_ghost;

  ensures EDFProperty(&xReadyTasksList, pxCurrentTCB_ghost);
  ensures SchedulerSuspendedListContext(&xReadyTasksList,
                                        pxDelayedTaskList_ghost,
                                        pxOverflowDelayedTaskList_ghost,
                                        &xSuspendedTaskList);
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

/*@
  requires xSchedulerRunning_ghost != pdFALSE;
  requires uxSchedulerSuspended_ghost == (UBaseType_t)0U;
  requires \valid(pxCurrentTCB_ghost);
  requires EDFProperty(&xReadyTasksList, pxCurrentTCB_ghost);
  requires (xTaskToSuspend == \null || xTaskToSuspend == pxCurrentTCB_ghost) ==>
    xReadyTasksList.uxNumberOfItems > (UBaseType_t)1U;
  requires xTaskToSuspend == \null || \valid(xTaskToSuspend);
  requires \valid((xTaskToSuspend == \null) ? pxCurrentTCB_ghost : xTaskToSuspend);
  requires TaskItem(&((xTaskToSuspend == \null) ?
                      pxCurrentTCB_ghost : xTaskToSuspend)->xStateListItem);
  requires ((In(&((xTaskToSuspend == \null) ?
                  pxCurrentTCB_ghost : xTaskToSuspend)->xStateListItem,
                &xReadyTasksList)) ||
            (In(&((xTaskToSuspend == \null) ?
                  pxCurrentTCB_ghost : xTaskToSuspend)->xStateListItem,
                pxDelayedTaskList_ghost)) ||
            (In(&((xTaskToSuspend == \null) ?
                  pxCurrentTCB_ghost : xTaskToSuspend)->xStateListItem,
                pxOverflowDelayedTaskList_ghost)));
  requires ((xTaskToSuspend == \null) ?
            pxCurrentTCB_ghost : xTaskToSuspend)->xEventListItem.pxContainer == \null;
  requires SchedulerSuspendedListContext(&xReadyTasksList,
                                         pxDelayedTaskList_ghost,
                                         pxOverflowDelayedTaskList_ghost,
                                         &xSuspendedTaskList);
  requires \separated(&pxCurrentTCB,
                      &uxSchedulerSuspended,
                      &xSchedulerRunning,
                      &xNextTaskUnblockTime,
                      &xYieldPendings[0],
                      &xReadyTasksList.uxNumberOfItems,
                      &pxDelayedTaskList_ghost->uxNumberOfItems,
                      &pxOverflowDelayedTaskList_ghost->uxNumberOfItems,
                      &xSuspendedTaskList.uxNumberOfItems,
                      &((xTaskToSuspend == \null) ?
                        pxCurrentTCB_ghost : xTaskToSuspend)->xStateListItem.pxContainer);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns xReadyTasksList.uxNumberOfItems,
          pxDelayedTaskList_ghost->uxNumberOfItems,
          pxOverflowDelayedTaskList_ghost->uxNumberOfItems,
          xSuspendedTaskList.uxNumberOfItems,
          ((xTaskToSuspend == \null) ?
            pxCurrentTCB_ghost : xTaskToSuspend)->xStateListItem.pxContainer,
          xNextTaskUnblockTime,
          xNextTaskUnblockTime_ghost,
          pxCurrentTCB,
          pxCurrentTCB_ghost,
          xYieldPendings[0],
          xYieldPendings0_ghost;

  ensures uxSchedulerSuspended_ghost == \old(uxSchedulerSuspended_ghost);
  ensures SchedulerSuspendedListContext(&xReadyTasksList,
                                        pxDelayedTaskList_ghost,
                                        pxOverflowDelayedTaskList_ghost,
                                        &xSuspendedTaskList);
  ensures EDFProperty(&xReadyTasksList, pxCurrentTCB_ghost);
  ensures In(&((xTaskToSuspend == \null) ?
               \old(pxCurrentTCB_ghost) : xTaskToSuspend)->xStateListItem,
             &xSuspendedTaskList);
  ensures !In(&((xTaskToSuspend == \null) ?
                \old(pxCurrentTCB_ghost) : xTaskToSuspend)->xStateListItem,
              &xReadyTasksList);
  ensures !In(&((xTaskToSuspend == \null) ?
                \old(pxCurrentTCB_ghost) : xTaskToSuspend)->xStateListItem,
              pxDelayedTaskList_ghost);
  ensures !In(&((xTaskToSuspend == \null) ?
                \old(pxCurrentTCB_ghost) : xTaskToSuspend)->xStateListItem,
              pxOverflowDelayedTaskList_ghost);

  ensures \forall ListItem_t *item;
    \valid{Pre}(item) &&
    item != &((xTaskToSuspend == \null) ?
              \old(pxCurrentTCB_ghost) : xTaskToSuspend)->xStateListItem ==>
      (In(item, &xReadyTasksList) <==> In{Pre}(item, &xReadyTasksList));
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) &&
    item != &((xTaskToSuspend == \null) ?
              \old(pxCurrentTCB_ghost) : xTaskToSuspend)->xStateListItem ==>
      (In(item, pxDelayedTaskList_ghost) <==>
       In{Pre}(item, pxDelayedTaskList_ghost));
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) &&
    item != &((xTaskToSuspend == \null) ?
              \old(pxCurrentTCB_ghost) : xTaskToSuspend)->xStateListItem ==>
      (In(item, pxOverflowDelayedTaskList_ghost) <==>
       In{Pre}(item, pxOverflowDelayedTaskList_ghost));
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) &&
    item != &((xTaskToSuspend == \null) ?
              \old(pxCurrentTCB_ghost) : xTaskToSuspend)->xStateListItem ==>
      (In(item, &xSuspendedTaskList) <==> In{Pre}(item, &xSuspendedTaskList));

  behavior suspend_current:
    assumes xTaskToSuspend == \null || xTaskToSuspend == pxCurrentTCB_ghost;

  behavior suspend_other:
    assumes xTaskToSuspend != \null && xTaskToSuspend != pxCurrentTCB_ghost;
    ensures pxCurrentTCB_ghost == \old(pxCurrentTCB_ghost);

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
                   In(&pxTCB->xStateListItem, pxDelayedTaskList_ghost) ||
                   In(&pxTCB->xStateListItem, pxOverflowDelayedTaskList_ghost); */
        /*@ assert pxTCB->xStateListItem.pxContainer == &xReadyTasksList ||
                   pxTCB->xStateListItem.pxContainer == pxDelayedTaskList_ghost ||
                   pxTCB->xStateListItem.pxContainer == pxOverflowDelayedTaskList_ghost; */
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
                                                 pxDelayedTaskList_ghost,
                                                 pxOverflowDelayedTaskList_ghost,
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
            //@ assert In(&pxCurrentTCB_ghost->xStateListItem, &xReadyTasksList);
            //@ assert ListValueLowerBound(&xReadyTasksList, pxCurrentTCB_ghost->xDeadline);
        }
    }
#endif /* #if ( configNUMBER_OF_CORES == 1 ) */

    traceRETURN_vTaskSuspend();
}
