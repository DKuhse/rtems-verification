#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "FreeRTOS.h"
#include "list.h"
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

struct tskTaskControlBlock {
    ListItem_t xStateListItem;
    TickType_t xDeadline;
};
typedef struct tskTaskControlBlock TCB_t;

#define FREERTOS_USE_ABSTRACT_LIST_MODEL
#include "scheduler_model.h"

/* Hack: Frama-C can't handle volatile */
#ifdef __FRAMAC__
    TCB_t * pxCurrentTCB;
    List_t xReadyTasksList;
    UBaseType_t uxSchedulerSuspended;
    BaseType_t xYieldPendings[1];
#else
    volatile TCB_t * pxCurrentTCB;
    List_t xReadyTasksList;
    volatile UBaseType_t uxSchedulerSuspended;
    volatile BaseType_t xYieldPendings[1];
#endif

#define taskSELECT_HIGHEST_PRIORITY_TASK() \
    pxCurrentTCB = listGET_OWNER_OF_HEAD_ENTRY( &xReadyTasksList )

/*@
  requires xReadyTasksList.uxNumberOfItems > 0;
  requires ReadyList( &xReadyTasksList );

  assigns pxCurrentTCB, xYieldPendings[ 0 ];

  behavior suspended:
    assumes uxSchedulerSuspended != (UBaseType_t)0U;
    assigns xYieldPendings[0];
    ensures xYieldPendings[0] == pdTRUE;
    ensures pxCurrentTCB == \old( pxCurrentTCB );

  behavior running:
    assumes uxSchedulerSuspended == (UBaseType_t)0U;
    assigns pxCurrentTCB, xYieldPendings[ 0 ];
    ensures xYieldPendings[0] == pdFALSE;
    ensures pxCurrentTCB ==
              (TCB_t *) Head( &xReadyTasksList )->pvOwner;
    ensures EDFProperty( &xReadyTasksList, pxCurrentTCB );

  complete behaviors suspended, running;
  disjoint behaviors suspended, running;
*/
#if (configNUMBER_OF_CORES == 1)
void vTaskSwitchContext(void) {
    traceENTER_vTaskSwitchContext();
    if (uxSchedulerSuspended != (UBaseType_t)0U) {
        /* The scheduler is currently suspended - do not allow a context
         * switch. */
        xYieldPendings[0] = pdTRUE;

    } else {
        xYieldPendings[0] = pdFALSE;
        traceTASK_SWITCHED_OUT();

#if (configGENERATE_RUN_TIME_STATS == 1)
        {
#ifdef portALT_GET_RUN_TIME_COUNTER_VALUE
            portALT_GET_RUN_TIME_COUNTER_VALUE(ulTotalRunTime[0]);
#else
            ulTotalRunTime[0] = portGET_RUN_TIME_COUNTER_VALUE();
#endif

            /* Add the amount of time the task has been running to the
             * accumulated time so far.  The time the task started running was
             * stored in ulTaskSwitchedInTime.  Note that there is no overflow
             * protection here so count values are only valid until the timer
             * overflows.  The guard against negative values is to protect
             * against suspect run time stat counter implementations - which
             * are provided by the application, not the kernel. */
            if (ulTotalRunTime[0] > ulTaskSwitchedInTime[0]) {
                pxCurrentTCB->ulRunTimeCounter += (ulTotalRunTime[0] - ulTaskSwitchedInTime[0]);
            } else {
                mtCOVERAGE_TEST_MARKER();
            }

            ulTaskSwitchedInTime[0] = ulTotalRunTime[0];
        }
#endif /* configGENERATE_RUN_TIME_STATS */

        /* Check for stack overflow, if configured. */
        taskCHECK_FOR_STACK_OVERFLOW();

/* Before the currently running task is switched out, save its errno. */
#if (configUSE_POSIX_ERRNO == 1)
        {
            pxCurrentTCB->iTaskErrno = FreeRTOS_errno;
        }
#endif

        /* Select a new task to run using either the generic C or port
         * optimised asm code. */
        /* MISRA Ref 11.5.3 [Void pointer assignment] */
        /* More details at: https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/MISRA.md#rule-115 */
        /* coverity[misra_c_2012_rule_11_5_violation] */
        taskSELECT_HIGHEST_PRIORITY_TASK();
#ifdef SANITY_PROBE
        /* Sanity probe — must NOT prove. Checks that the hypothesis
         * set at the point where EDFProperty must hold is not
         * vacuous. Enabled only by sanity-check.sh. */
        //@ assert \false;
#endif
        traceTASK_SWITCHED_IN();

        /* Macro to inject port specific behaviour immediately after
         * switching tasks, such as setting an end of stack watchpoint
         * or reconfiguring the MPU. */
        portTASK_SWITCH_HOOK(pxCurrentTCB);

/* After the new task is switched in, update the global errno. */
#if (configUSE_POSIX_ERRNO == 1)
        {
            FreeRTOS_errno = pxCurrentTCB->iTaskErrno;
        }
#endif

#if (configUSE_C_RUNTIME_TLS_SUPPORT == 1)
        {
            /* Switch C-Runtime's TLS Block to point to the TLS
             * Block specific to this task. */
            configSET_TLS_BLOCK(pxCurrentTCB->xTLSBlock);
        }
#endif
    }

    traceRETURN_vTaskSwitchContext();
}
#else /* if ( configNUMBER_OF_CORES == 1 ) */
/* SMP branch (tasks.c:4803) */
#error "vTaskSwitchContext SMP branch is not part of this verification target"
#endif /* if ( configNUMBER_OF_CORES == 1 ) */
