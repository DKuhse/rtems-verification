/*
 * Shared FreeRTOS reference-overlay stubs for the scheduler/port glue
 * the standalone extractions all depend on.  Provides:
 *
 *   - The MSP430 port quartet (push-sr / disable-interrupts /
 *     save-context / restore-context) as `assigns \nothing` stubs,
 *     plus the portDISABLE_INTERRUPTS / portSAVE_CONTEXT /
 *     portRESTORE_CONTEXT redirect macros.
 *   - The vTaskSwitchContext callee contract used by yield wrappers.
 *   - vPortYield's source-shaped body and unified contract.
 *
 * Prerequisites in the includer (before #include):
 *   - freertos_volatile_instrumentation.h (TCB layout + globals).
 *   - scheduler_model.h (EDFProperty, ReadyList, SchedulerListContext,
 *     SchedulerSuspendedListContext, ...).
 *
 * Unified vPortYield contract: the function-level requires/ensures uses
 * the 3-arg SchedulerListContext (which all callers can satisfy).  A
 * gated ensures additionally publishes preservation of the 4-arg
 * SchedulerSuspendedListContext form so callers that operate with a
 * suspended-list invariant (suspend.c, resume.c) don't lose it across
 * the yield.  The body assigns nothing list-related, so WP discharges
 * both forms from the same assigns clause.
 */

#ifndef VERIFICATION_FREERTOS_MODEL_SCHEDULER_STUBS_H
#define VERIFICATION_FREERTOS_MODEL_SCHEDULER_STUBS_H

#ifndef __FRAMAC__
#  error "freertos_scheduler_stubs.h is a Frama-C verification-only header."
#endif

/* --- vTaskSwitchContext: shared callee + definition-site contract -------- */

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
    assigns xYieldPendings[0],
            xYieldPendings0_ghost;
    ensures xYieldPendings0_ghost == pdTRUE;
    ensures pxCurrentTCB_ghost == \old(pxCurrentTCB_ghost);

  behavior running:
    assumes uxSchedulerSuspended_ghost == (UBaseType_t)0U;
    assigns pxCurrentTCB,
            pxCurrentTCB_ghost,
            xYieldPendings[0],
            xYieldPendings0_ghost;
    ensures xYieldPendings0_ghost == pdFALSE;
    ensures pxCurrentTCB_ghost == (TCB_t *)Head(&xReadyTasksList)->pvOwner;
    ensures EDFProperty(&xReadyTasksList, pxCurrentTCB_ghost);

  complete behaviors suspended, running;
  disjoint behaviors suspended, running;
*/
void vTaskSwitchContext(void);

/* --- MSP430 port quartet (assembly stand-ins) ---------------------------- */

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

/* --- vPortYield (unified) ------------------------------------------------ */

/*@
  requires uxSchedulerSuspended_ghost == (UBaseType_t)0U;
  requires xReadyTasksList.uxNumberOfItems > (UBaseType_t)0U;
  requires ReadyList(&xReadyTasksList);
  requires SchedulerListContext(&xReadyTasksList,
                                pxDelayedTaskList_ghost,
                                pxOverflowDelayedTaskList_ghost);

  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;

  assigns pxCurrentTCB,
          pxCurrentTCB_ghost,
          xYieldPendings[0],
          xYieldPendings0_ghost;

  ensures EDFProperty(&xReadyTasksList, pxCurrentTCB_ghost);
  ensures SchedulerListContext(&xReadyTasksList,
                               pxDelayedTaskList_ghost,
                               pxOverflowDelayedTaskList_ghost);
  ensures SchedulerSuspendedListContext{Pre}(&xReadyTasksList,
                                             pxDelayedTaskList_ghost,
                                             pxOverflowDelayedTaskList_ghost,
                                             &xSuspendedTaskList) ==>
          SchedulerSuspendedListContext(&xReadyTasksList,
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

#endif /* VERIFICATION_FREERTOS_MODEL_SCHEDULER_STUBS_H */
