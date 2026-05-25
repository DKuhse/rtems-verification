/*
 * Verification overlay (reference) for vTaskResume.
 *
 * This slice covers the single-core path where a distinct suspended task is
 * resumed.  It proves the suspended-to-ready move, scheduler/suspended-list
 * context preservation, and EDF preservation across the optional yield.
 */

#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "FreeRTOS.h"
#include "list.h"
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

/* The notification loop in prvTaskIsTaskSuspended is outside this slice. */
#undef configUSE_TASK_NOTIFICATIONS
#define configUSE_TASK_NOTIFICATIONS 0

/* Keep assertions side-effect free in this reference slice. */
#undef configASSERT
#define configASSERT(x) ((void)(x))

#include "freertos_volatile_instrumentation.h"

#define FREERTOS_USE_ABSTRACT_LIST_MODEL
#include "scheduler_model.h"

/* Route direct list calls through the scheduler model. */
#define uxListRemove(pxItemToRemove) vSchedulerListRemove_abs((pxItemToRemove))

/* Critical sections are outside this sequential reference slice. */
#define taskENTER_CRITICAL()
#define taskEXIT_CRITICAL()

/* From tasks.c:292, EDF version of prvAddTaskToReadyList. */
#define prvAddTaskToReadyList(pxTCB)                                        \
    do {                                                                    \
        traceMOVED_TASK_TO_READY_STATE(pxTCB);                              \
        listSET_LIST_ITEM_VALUE(&(pxTCB->xStateListItem), (pxTCB)->xDeadline); \
        vListInsert(&(xReadyTasksList), &((pxTCB)->xStateListItem));        \
        tracePOST_MOVED_TASK_TO_READY_STATE(pxTCB);                         \
    } while (0)

#include "freertos_scheduler_stubs.h"

#define taskYIELD_ANY_CORE_IF_USING_PREEMPTION(pxTCB)       \
    do {                                                    \
        if (pxCurrentTCB->xDeadline > (pxTCB)->xDeadline) { \
            portYIELD_WITHIN_API();                         \
        }                                                   \
    } while (0)

/*@
  requires xTask != \null;
  requires \valid(xTask);
  requires TaskItem(&xTask->xStateListItem);
  requires \valid(&xTask->xEventListItem);
  requires In(&xTask->xStateListItem, &xSuspendedTaskList);
  requires xTask->xEventListItem.pxContainer == \null;
  requires SchedulerSuspendedListContext(&xReadyTasksList,
                                         pxDelayedTaskList_ghost,
                                         pxOverflowDelayedTaskList_ghost,
                                         &xSuspendedTaskList);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns \nothing;

  ensures \result == pdTRUE;
  ensures SchedulerSuspendedListContext(&xReadyTasksList,
                                        pxDelayedTaskList_ghost,
                                        pxOverflowDelayedTaskList_ghost,
                                        &xSuspendedTaskList);
*/
static BaseType_t prvTaskIsTaskSuspended(const TaskHandle_t xTask) {
    BaseType_t xReturn = pdFALSE;
    const TCB_t* const pxTCB = xTask;

    /* Accesses xPendingReadyList so must be called from a critical
     * section. */

    /* It does not make sense to check if the calling task is suspended. */
    configASSERT(xTask);

    /* Is the task being resumed actually in the suspended list? */
    if (listIS_CONTAINED_WITHIN(&xSuspendedTaskList, &(pxTCB->xStateListItem)) != pdFALSE) {
        /* Has the task already been resumed from within an ISR? */
        if (listIS_CONTAINED_WITHIN(&xPendingReadyList, &(pxTCB->xEventListItem)) == pdFALSE) {
            /* Is it in the suspended list because it is in the Suspended
             * state, or because it is blocked with no timeout? */
            if (listIS_CONTAINED_WITHIN(NULL, &(pxTCB->xEventListItem)) != pdFALSE) {
#if (configUSE_TASK_NOTIFICATIONS == 1)
                {
                    BaseType_t x;

                    /* The task does not appear on the event list item of
                     * any of the RTOS objects, but could still be in the
                     * blocked state if it is waiting on its notification
                     * rather than waiting on an object.  If not, it is
                     * suspended. */
                    xReturn = pdTRUE;

                    for (x = (BaseType_t)0; x < (BaseType_t)configTASK_NOTIFICATION_ARRAY_ENTRIES; x++) {
                        if (pxTCB->ucNotifyState[x] == taskWAITING_NOTIFICATION) {
                            xReturn = pdFALSE;
                            break;
                        }
                    }
                }
#else /* if ( configUSE_TASK_NOTIFICATIONS == 1 ) */
                {
                    xReturn = pdTRUE;
                }
#endif /* if ( configUSE_TASK_NOTIFICATIONS == 1 ) */
            } else {
                mtCOVERAGE_TEST_MARKER();
            }
        } else {
            mtCOVERAGE_TEST_MARKER();
        }
    } else {
        mtCOVERAGE_TEST_MARKER();
    }

    return xReturn;
}

/*@
  requires uxSchedulerSuspended_ghost == (UBaseType_t)0U;
  requires xTaskToResume != \null;
  requires xTaskToResume != pxCurrentTCB_ghost;
  requires \valid(xTaskToResume);
  requires \valid(pxCurrentTCB_ghost);
  requires TaskItem(&xTaskToResume->xStateListItem);
  requires In(&xTaskToResume->xStateListItem, &xSuspendedTaskList);
  requires xTaskToResume->xEventListItem.pxContainer == \null;
  requires xReadyTasksList.uxNumberOfItems > (UBaseType_t)0U;
  requires EDFProperty(&xReadyTasksList, pxCurrentTCB_ghost);
  requires SchedulerSuspendedListContext(&xReadyTasksList,
                                         pxDelayedTaskList_ghost,
                                         pxOverflowDelayedTaskList_ghost,
                                         &xSuspendedTaskList);
  requires \separated(xTaskToResume,
                      pxCurrentTCB_ghost,
                      &pxCurrentTCB,
                      &uxSchedulerSuspended,
                      &xYieldPendings[0],
                      &xReadyTasksList.uxNumberOfItems,
                      &xSuspendedTaskList.uxNumberOfItems);
  requires \separated(&pxCurrentTCB,
                      &uxSchedulerSuspended,
                      &xYieldPendings[0],
                      &xReadyTasksList.uxNumberOfItems,
                      &xSuspendedTaskList.uxNumberOfItems,
                      &xTaskToResume->xStateListItem.xItemValue,
                      &xTaskToResume->xStateListItem.pxContainer);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns xReadyTasksList.uxNumberOfItems,
          xSuspendedTaskList.uxNumberOfItems,
          xTaskToResume->xStateListItem.xItemValue,
          xTaskToResume->xStateListItem.pxContainer,
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
  ensures In(&xTaskToResume->xStateListItem, &xReadyTasksList);
  ensures !In(&xTaskToResume->xStateListItem, &xSuspendedTaskList);
  ensures xTaskToResume->xStateListItem.xItemValue ==
    xTaskToResume->xDeadline;

  ensures \forall ListItem_t *item;
    \valid{Pre}(item) && item != &xTaskToResume->xStateListItem ==>
      (In(item, &xReadyTasksList) <==> In{Pre}(item, &xReadyTasksList));
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) && item != &xTaskToResume->xStateListItem ==>
      (In(item, pxDelayedTaskList_ghost) <==>
       In{Pre}(item, pxDelayedTaskList_ghost));
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) && item != &xTaskToResume->xStateListItem ==>
      (In(item, pxOverflowDelayedTaskList_ghost) <==>
       In{Pre}(item, pxOverflowDelayedTaskList_ghost));
  ensures \forall ListItem_t *item;
    \valid{Pre}(item) && item != &xTaskToResume->xStateListItem ==>
      (In(item, &xSuspendedTaskList) <==> In{Pre}(item, &xSuspendedTaskList));

  behavior resumed_preempts:
    assumes pxCurrentTCB_ghost->xDeadline > xTaskToResume->xDeadline;
    ensures EDFProperty(&xReadyTasksList, pxCurrentTCB_ghost);

  behavior resumed_without_preempting:
    assumes pxCurrentTCB_ghost->xDeadline <= xTaskToResume->xDeadline;
    ensures pxCurrentTCB_ghost == \old(pxCurrentTCB_ghost);
    ensures EDFProperty(&xReadyTasksList, pxCurrentTCB_ghost);

  complete behaviors;
  disjoint behaviors;
*/
void vTaskResume(TaskHandle_t xTaskToResume) {
    TCB_t* const pxTCB = xTaskToResume;

    traceENTER_vTaskResume(xTaskToResume);

    /* It does not make sense to resume the calling task. */
    configASSERT(xTaskToResume);

#if (configNUMBER_OF_CORES == 1)

    /* The parameter cannot be NULL as it is impossible to resume the
     * currently executing task. */
    if ((pxTCB != pxCurrentTCB) && (pxTCB != NULL))
#else

    /* The parameter cannot be NULL as it is impossible to resume the
     * currently executing task. It is also impossible to resume a task
     * that is actively running on another core but it is not safe
     * to check their run state here. Therefore, we get into a critical
     * section and check if the task is actually suspended or not. */
    if (pxTCB != NULL)
#endif
    {
        taskENTER_CRITICAL();
        {
            if (prvTaskIsTaskSuspended(pxTCB) != pdFALSE) {
                traceTASK_RESUME(pxTCB);

                /* The ready list can be accessed even if the scheduler is
                 * suspended because this is inside a critical section. */
#ifndef SANITY_PROBE
                //@ assert In(&pxTCB->xStateListItem, &xSuspendedTaskList);
                //@ assert pxTCB->xEventListItem.pxContainer == \null;
                //@ assert ListInv(&xSuspendedTaskList);
                //@ assert pxTCB->xStateListItem.pxContainer == &xSuspendedTaskList;
                //@ assert xSuspendedTaskList.uxNumberOfItems > (UBaseType_t)0U;
#endif
                (void)uxListRemove(&(pxTCB->xStateListItem));

                //@ assert Detached(&pxTCB->xStateListItem);
                //@ assert TaskItem(&pxTCB->xStateListItem);
                prvAddTaskToReadyList(pxTCB);

#ifndef SANITY_PROBE
                //@ assert In(&pxTCB->xStateListItem, &xReadyTasksList);
                //@ assert pxTCB->xStateListItem.xItemValue == pxTCB->xDeadline;
                /*@ assert SchedulerSuspendedListContext(&xReadyTasksList,
                                                          pxDelayedTaskList_ghost,
                                                          pxOverflowDelayedTaskList_ghost,
                                                          &xSuspendedTaskList); */
                //@ assert xReadyTasksList.uxNumberOfItems > (UBaseType_t)0U;
#endif

                /* This yield may not cause the task just resumed to run,
                 * but will leave the lists in the correct state for the
                 * next yield. */
                taskYIELD_ANY_CORE_IF_USING_PREEMPTION(pxTCB);
            } else {
                mtCOVERAGE_TEST_MARKER();
            }
        }
        taskEXIT_CRITICAL();
    } else {
        mtCOVERAGE_TEST_MARKER();
    }

    traceRETURN_vTaskResume();
}
