#define MPU_WRAPPERS_INCLUDED_FROM_API_FILE
#include "FreeRTOS.h"
#include "list.h"
#undef MPU_WRAPPERS_INCLUDED_FROM_API_FILE

struct tskTaskControlBlock {
    ListItem_t xStateListItem;
    ListItem_t xEventListItem;
    TickType_t xDeadline;
};
typedef struct tskTaskControlBlock TCB_t;

#define FREERTOS_USE_ABSTRACT_LIST_MODEL
#include "scheduler_model.h"

/* Frama-C's Volatile plugin instruments the source-shaped volatile globals
 * through the ghost mirrors below.
 */
#ifdef __FRAMAC__
    TCB_t * volatile pxCurrentTCB;
    List_t xReadyTasksList;
    volatile UBaseType_t uxSchedulerSuspended;
    volatile BaseType_t xYieldPendings[1];
#else
    TCB_t * volatile pxCurrentTCB;
    List_t xReadyTasksList;
    volatile UBaseType_t uxSchedulerSuspended;
    volatile BaseType_t xYieldPendings[1];
#endif

/*@ ghost TCB_t *pxCurrentTCB_ghost; */
/*@ ghost UBaseType_t uxSchedulerSuspended_ghost; */
/*@ ghost BaseType_t xYieldPendings0_ghost; */

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

#define taskSELECT_HIGHEST_PRIORITY_TASK() \
    pxCurrentTCB = listGET_OWNER_OF_HEAD_ENTRY( &xReadyTasksList )

/*@
  requires xReadyTasksList.uxNumberOfItems > 0;
  requires ReadyList( &xReadyTasksList );

  assigns pxCurrentTCB,
          pxCurrentTCB_ghost,
          xYieldPendings[ 0 ],
          xYieldPendings0_ghost;

  behavior suspended:
    assumes uxSchedulerSuspended_ghost != (UBaseType_t)0U;
    assigns xYieldPendings[0],
            xYieldPendings0_ghost;
    ensures xYieldPendings0_ghost == pdTRUE;
    ensures pxCurrentTCB_ghost == \old( pxCurrentTCB_ghost );

  behavior running:
    assumes uxSchedulerSuspended_ghost == (UBaseType_t)0U;
    assigns pxCurrentTCB,
            pxCurrentTCB_ghost,
            xYieldPendings[ 0 ],
            xYieldPendings0_ghost;
    ensures xYieldPendings0_ghost == pdFALSE;
    ensures pxCurrentTCB_ghost ==
              (TCB_t *) Head( &xReadyTasksList )->pvOwner;
    ensures EDFProperty( &xReadyTasksList, pxCurrentTCB_ghost );

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
        //@ assert sanity_task_switch_probe: \false;
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
