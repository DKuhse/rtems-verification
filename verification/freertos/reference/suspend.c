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

/* Frama-C's Volatile plugin instruments the source-shaped volatile globals
 * through the ghost mirrors below.
 */
#ifdef __FRAMAC__
    TCB_t * volatile pxCurrentTCB;
    List_t      xReadyTasksList;
    List_t      xSuspendedTaskList;
    volatile UBaseType_t uxSchedulerSuspended;
    volatile TickType_t  xNextTaskUnblockTime;
    volatile BaseType_t  xSchedulerRunning;
    volatile UBaseType_t uxCurrentNumberOfTasks;
    volatile BaseType_t  xYieldPendings[1];
    List_t      xDelayedTaskList1;
    List_t      xDelayedTaskList2;
    List_t * volatile pxDelayedTaskList;
    List_t * volatile pxOverflowDelayedTaskList;
#else
    TCB_t * volatile     pxCurrentTCB;
    List_t               xReadyTasksList;
    List_t               xSuspendedTaskList;
    volatile UBaseType_t uxSchedulerSuspended;
    volatile TickType_t  xNextTaskUnblockTime;
    volatile BaseType_t  xSchedulerRunning;
    volatile UBaseType_t uxCurrentNumberOfTasks;
    volatile BaseType_t  xYieldPendings[1];
    List_t               xDelayedTaskList1;
    List_t               xDelayedTaskList2;
    List_t * volatile    pxDelayedTaskList;
    List_t * volatile    pxOverflowDelayedTaskList;
#endif

/*@ ghost TCB_t *pxCurrentTCB_ghost; */
/*@ ghost UBaseType_t uxSchedulerSuspended_ghost; */
/*@ ghost TickType_t xNextTaskUnblockTime_ghost; */
/*@ ghost BaseType_t xSchedulerRunning_ghost; */
/*@ ghost UBaseType_t uxCurrentNumberOfTasks_ghost; */
/*@ ghost BaseType_t xYieldPendings0_ghost; */
/*@ ghost List_t *pxDelayedTaskList_ghost; */
/*@ ghost List_t *pxOverflowDelayedTaskList_ghost; */

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns \result \from pxCurrentTCB_ghost;
  ensures \result == pxCurrentTCB_ghost;
*/
TCB_t *pxCurrentTCB_read(TCB_t * volatile *current);

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns *current, pxCurrentTCB_ghost, \result \from value;
  ensures \result == value;
  ensures pxCurrentTCB_ghost == value;
*/
TCB_t *pxCurrentTCB_write(TCB_t * volatile *current,
                          TCB_t *value);

/*@ volatile pxCurrentTCB
      reads pxCurrentTCB_read
      writes pxCurrentTCB_write;
*/

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns \result \from uxSchedulerSuspended_ghost;
  ensures \result == uxSchedulerSuspended_ghost;
*/
UBaseType_t uxSchedulerSuspended_read(volatile UBaseType_t *suspended);

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns *suspended, uxSchedulerSuspended_ghost, \result \from value;
  ensures \result == value;
  ensures uxSchedulerSuspended_ghost == value;
*/
UBaseType_t uxSchedulerSuspended_write(volatile UBaseType_t *suspended,
                                       UBaseType_t value);

/*@ volatile uxSchedulerSuspended
      reads uxSchedulerSuspended_read
      writes uxSchedulerSuspended_write;
*/

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns \result \from xNextTaskUnblockTime_ghost;
  ensures \result == xNextTaskUnblockTime_ghost;
*/
TickType_t xNextTaskUnblockTime_read(volatile TickType_t *unblockTime);

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns *unblockTime, xNextTaskUnblockTime_ghost, \result \from value;
  ensures \result == value;
  ensures xNextTaskUnblockTime_ghost == value;
*/
TickType_t xNextTaskUnblockTime_write(volatile TickType_t *unblockTime,
                                      TickType_t value);

/*@ volatile xNextTaskUnblockTime
      reads xNextTaskUnblockTime_read
      writes xNextTaskUnblockTime_write;
*/

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns \result \from xSchedulerRunning_ghost;
  ensures \result == xSchedulerRunning_ghost;
*/
BaseType_t xSchedulerRunning_read(volatile BaseType_t *schedulerRunning);

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns *schedulerRunning, xSchedulerRunning_ghost, \result \from value;
  ensures \result == value;
  ensures xSchedulerRunning_ghost == value;
*/
BaseType_t xSchedulerRunning_write(volatile BaseType_t *schedulerRunning,
                                   BaseType_t value);

/*@ volatile xSchedulerRunning
      reads xSchedulerRunning_read
      writes xSchedulerRunning_write;
*/

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns \result \from uxCurrentNumberOfTasks_ghost;
  ensures \result == uxCurrentNumberOfTasks_ghost;
*/
UBaseType_t uxCurrentNumberOfTasks_read(volatile UBaseType_t *numberOfTasks);

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns *numberOfTasks, uxCurrentNumberOfTasks_ghost, \result \from value;
  ensures \result == value;
  ensures uxCurrentNumberOfTasks_ghost == value;
*/
UBaseType_t uxCurrentNumberOfTasks_write(volatile UBaseType_t *numberOfTasks,
                                         UBaseType_t value);

/*@ volatile uxCurrentNumberOfTasks
      reads uxCurrentNumberOfTasks_read
      writes uxCurrentNumberOfTasks_write;
*/

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns \result \from xYieldPendings0_ghost;
  ensures \result == xYieldPendings0_ghost;
*/
BaseType_t xYieldPendings0_read(volatile BaseType_t *yieldPending);

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns *yieldPending, xYieldPendings0_ghost, \result \from value;
  ensures \result == value;
  ensures xYieldPendings0_ghost == value;
*/
BaseType_t xYieldPendings0_write(volatile BaseType_t *yieldPending,
                                 BaseType_t value);

/*@ volatile xYieldPendings[0]
      reads xYieldPendings0_read
      writes xYieldPendings0_write;
*/

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns \result \from pxDelayedTaskList_ghost;
  ensures \result == pxDelayedTaskList_ghost;
*/
List_t *pxDelayedTaskList_read(List_t * volatile *list);

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns *list, pxDelayedTaskList_ghost, \result \from value;
  ensures \result == value;
  ensures pxDelayedTaskList_ghost == value;
*/
List_t *pxDelayedTaskList_write(List_t * volatile *list,
                                List_t *value);

/*@ volatile pxDelayedTaskList
      reads pxDelayedTaskList_read
      writes pxDelayedTaskList_write;
*/

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns \result \from pxOverflowDelayedTaskList_ghost;
  ensures \result == pxOverflowDelayedTaskList_ghost;
*/
List_t *pxOverflowDelayedTaskList_read(List_t * volatile *list);

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns *list, pxOverflowDelayedTaskList_ghost, \result \from value;
  ensures \result == value;
  ensures pxOverflowDelayedTaskList_ghost == value;
*/
List_t *pxOverflowDelayedTaskList_write(List_t * volatile *list,
                                        List_t *value);

/*@ volatile pxOverflowDelayedTaskList
      reads pxOverflowDelayedTaskList_read
      writes pxOverflowDelayedTaskList_write;
*/

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
