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

/* prvResetNextTaskUnblockTime is a static helper in tasks.c that
 * scans the delayed list and updates xNextTaskUnblockTime. We model
 * it as opaque — its exact effect doesn't matter for the
 * suspended-branch proof. */
/*@
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

/* From tasks.c:294 — EDF version of prvAddTaskToReadyList. */
#define prvAddTaskToReadyList(pxTCB)                                        \
    do {                                                                    \
        listSET_LIST_ITEM_VALUE(&(pxTCB->xStateListItem), (pxTCB)->xDeadline); \
        vListInsert(&(xReadyTasksList), &((pxTCB)->xStateListItem));        \
    } while (0)

/*@
  behavior suspended:
    assumes uxSchedulerSuspended != (UBaseType_t)0U;
    assigns xPendedTicks;
    // Note the modular form: `xPendedTicks += 1U` on a uint16_t wraps.
    // The wrap is harmless in practice (xTaskResumeAll resets the
    // counter), so we don't add a no-overflow precondition.
    ensures xPendedTicks == (TickType_t)( \old(xPendedTicks) + 1U );
    ensures \result == pdFALSE;
    ensures pxCurrentTCB == \old(pxCurrentTCB);
    ensures xTickCount == \old(xTickCount);

  behavior running:
    assumes uxSchedulerSuspended == (UBaseType_t)0U;
    // Substantive postconditions deferred — see file header.
    // The pxCurrentTCB-unchanged claim is currently unprovable
    // because vListInsert (called via prvAddTaskToReadyList) has no
    // assigns clause and WP defaults to `assigns \everything`. A
    // contract on vListInsert (in list.h overlay) would close this.

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

                    listREMOVE_ITEM(&(pxTCB->xStateListItem));

                    if (listLIST_ITEM_CONTAINER(&(pxTCB->xEventListItem)) != NULL) {
                        listREMOVE_ITEM(&(pxTCB->xEventListItem));
                    } else {
                        mtCOVERAGE_TEST_MARKER();
                    }

                    prvAddTaskToReadyList(pxTCB);

#if (configUSE_PREEMPTION == 1)
                    {
                        if (pxTCB->xDeadline < pxCurrentTCB->xDeadline) {
                            xSwitchRequired = pdTRUE;
                        } else {
                            mtCOVERAGE_TEST_MARKER();
                        }
                    }
#endif /* configUSE_PREEMPTION */
                }
            }
        }

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
