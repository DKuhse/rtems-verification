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

/* Minimal TCB fields dereferenced by the configured xTaskIncrementTick body. */
struct tskTaskControlBlock {
    ListItem_t xStateListItem;
    ListItem_t xEventListItem;
    TickType_t xDeadline;
};
typedef struct tskTaskControlBlock TCB_t;

#define FREERTOS_USE_ABSTRACT_LIST_MODEL
#include "scheduler_model.h"

/* Hack: Frama-C can't handle volatile. */
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

/* prvResetNextTaskUnblockTime is a static helper in tasks.c that scans the
 * delayed list and updates xNextTaskUnblockTime. */
/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns xNextTaskUnblockTime;
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
                                pxDelayedTaskList,
                                pxOverflowDelayedTaskList);

  // Termination of the delayed-list drain depends on list-count semantics.
  // This reset proves tick arithmetic only; termination is a later ADT proof.
  terminates \false;

  ensures SchedulerListContext(&xReadyTasksList,
                               pxDelayedTaskList,
                               pxOverflowDelayedTaskList);

  behavior suspended:
    assumes uxSchedulerSuspended != (UBaseType_t)0U;
    exits \false;
    assigns xPendedTicks;
    ensures xPendedTicks == (TickType_t)(\old(xPendedTicks) + 1U);
    ensures xTickCount == \old(xTickCount);
    ensures \result == pdFALSE;

  behavior running:
    assumes uxSchedulerSuspended == (UBaseType_t)0U;
    requires \valid(pxCurrentTCB);

    exits \false;

    ensures xTickCount == (TickType_t)(\old(xTickCount) + 1U);
    ensures \result == pdTRUE || \result == pdFALSE;

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
            /*@
              loop invariant xTickCount == (TickType_t)(\at(xTickCount, Pre) + 1U);
              loop invariant pxCurrentTCB == \at(pxCurrentTCB, Pre);
              loop invariant \valid(pxCurrentTCB);
              loop invariant SchedulerListContext(&xReadyTasksList,
                                                  pxDelayedTaskList,
                                                  pxOverflowDelayedTaskList);
              loop invariant xSwitchRequired == pdTRUE || xSwitchRequired == pdFALSE;

              loop assigns pxTCB,
                           xItemValue,
                           xSwitchRequired,
                           xNextTaskUnblockTime,
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
                    listREMOVE_ITEM(&(pxTCB->xStateListItem));

                    /* Is the task waiting on an event also?  If so remove it from
                     * the event list. */
                    if (listLIST_ITEM_CONTAINER(&(pxTCB->xEventListItem)) != NULL) {
                        listREMOVE_ITEM(&(pxTCB->xEventListItem));
                    } else {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    /* Place the unblocked task into the appropriate ready list. */
#ifdef SANITY_PROBE
                    /* Branch-local sanity probe - must NOT prove.  This catches
                     * contradictory assumptions specifically on the unblock path. */
                    //@ assert \false;
#endif
                    prvAddTaskToReadyList(pxTCB);

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
        //@ assert \false;
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
