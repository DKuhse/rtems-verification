// Instrumentation for volatile variables


#ifndef VERIFICATION_FREERTOS_MODEL_VOLATILE_INSTRUMENTATION_H
#define VERIFICATION_FREERTOS_MODEL_VOLATILE_INSTRUMENTATION_H

#ifndef __FRAMAC__
#  error "freertos_volatile_instrumentation.h is a Frama-C verification-only header."
#endif

/* --- Minimal verification-only TCB layout -------------------------------- */

struct tskTaskControlBlock {
    ListItem_t xStateListItem;
    ListItem_t xEventListItem;
    TickType_t xDeadline;
};
typedef struct tskTaskControlBlock TCB_t;
typedef TCB_t * TaskHandle_t;

/* --- Non-volatile list storage ------------------------------------------- */

List_t xReadyTasksList;
List_t xSuspendedTaskList;
List_t xPendingReadyList;
List_t xDelayedTaskList1;
List_t xDelayedTaskList2;

/* --- Volatile mirror storage --------------------------------------------- */

TCB_t * volatile     pxCurrentTCB;
volatile UBaseType_t uxSchedulerSuspended;
volatile UBaseType_t uxCurrentNumberOfTasks;
volatile TickType_t  xTickCount;
volatile TickType_t  xPendedTicks;
volatile TickType_t  xNextTaskUnblockTime;
volatile BaseType_t  xSchedulerRunning;
volatile BaseType_t  xNumOfOverflows;
volatile BaseType_t  xYieldPendings[1];
List_t * volatile    pxDelayedTaskList;
List_t * volatile    pxOverflowDelayedTaskList;

/* --- Ghost mirrors ------------------------------------------------------- */

/*@ ghost TCB_t       *pxCurrentTCB_ghost; */
/*@ ghost UBaseType_t  uxSchedulerSuspended_ghost; */
/*@ ghost UBaseType_t  uxCurrentNumberOfTasks_ghost; */
/*@ ghost TickType_t   xTickCount_ghost; */
/*@ ghost TickType_t   xPendedTicks_ghost; */
/*@ ghost TickType_t   xNextTaskUnblockTime_ghost; */
/*@ ghost BaseType_t   xSchedulerRunning_ghost; */
/*@ ghost BaseType_t   xNumOfOverflows_ghost; */
/*@ ghost BaseType_t   xYieldPendings0_ghost; */
/*@ ghost List_t      *pxDelayedTaskList_ghost; */
/*@ ghost List_t      *pxOverflowDelayedTaskList_ghost; */

/* --- Read/write stubs + volatile directives ------------------------------ */

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
  assigns \result \from xTickCount_ghost;
  ensures \result == xTickCount_ghost;
*/
TickType_t xTickCount_read(volatile TickType_t *tick);

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns *tick, xTickCount_ghost, \result \from value;
  ensures \result == value;
  ensures xTickCount_ghost == value;
*/
TickType_t xTickCount_write(volatile TickType_t *tick,
                            TickType_t value);

/*@ volatile xTickCount
      reads xTickCount_read
      writes xTickCount_write;
*/

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns \result \from xPendedTicks_ghost;
  ensures \result == xPendedTicks_ghost;
*/
TickType_t xPendedTicks_read(volatile TickType_t *pendedTicks);

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns *pendedTicks, xPendedTicks_ghost, \result \from value;
  ensures \result == value;
  ensures xPendedTicks_ghost == value;
*/
TickType_t xPendedTicks_write(volatile TickType_t *pendedTicks,
                              TickType_t value);

/*@ volatile xPendedTicks
      reads xPendedTicks_read
      writes xPendedTicks_write;
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
  assigns \result \from xNumOfOverflows_ghost;
  ensures \result == xNumOfOverflows_ghost;
*/
BaseType_t xNumOfOverflows_read(volatile BaseType_t *numOverflows);

/*@
  terminates \true;
  allocates \nothing;
  frees \nothing;
  exits \false;
  assigns *numOverflows, xNumOfOverflows_ghost, \result \from value;
  ensures \result == value;
  ensures xNumOfOverflows_ghost == value;
*/
BaseType_t xNumOfOverflows_write(volatile BaseType_t *numOverflows,
                                 BaseType_t value);

/*@ volatile xNumOfOverflows
      reads xNumOfOverflows_read
      writes xNumOfOverflows_write;
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

#endif /* VERIFICATION_FREERTOS_MODEL_VOLATILE_INSTRUMENTATION_H */
