#ifndef FREERTOS_TRACING_H
#define FREERTOS_TRACING_H

typedef enum {
    eSwitchedIn = 0,
    eSwitchedOut,
    eMovedTaskToReadyState,
    ePostMovedTaskToReadyState,
    eTaskIncrementTick,
    eTaskIncrementTickEnd,
} eTracingEventType;

typedef struct xTRACING_EVENT {
    eTracingEventType eEventID;
    uint32_t ulCycleCount;
    TickType_t xTickCount;
    void* pvTCB;
} TracingEvent_t;

extern TracingEvent_t* xTracingEvents;
extern size_t xTracingEventIndex;
/*
#define traceMOVED_TASK_TO_READY_STATE(pxTCB)                                                      \
    TracingEvent_t ev_traceMOVED_TASK_TO_READY_STATE = {.eEventID = eMovedTaskToReadyState,        \
                                                        .ulCycleCount = esp_cpu_get_cycle_count(), \
                                                        .xTickCount = xTickCount,                  \
                                                        .pvTCB = pxTCB};                           \
    xTracingEvents[xTracingEventIndex] = ev_traceMOVED_TASK_TO_READY_STATE;                        \
    xTracingEventIndex++

#define tracePOST_MOVED_TASK_TO_READY_STATE(pxTCB)                                                      \
    TracingEvent_t ev_tracePOST_MOVED_TASK_TO_READY_STATE = {.eEventID = ePostMovedTaskToReadyState,    \
                                                             .ulCycleCount = esp_cpu_get_cycle_count(), \
                                                             .xTickCount = xTickCount,                  \
                                                             .pvTCB = pxTCB};                           \
    xTracingEvents[xTracingEventIndex] = ev_tracePOST_MOVED_TASK_TO_READY_STATE;                        \
    xTracingEventIndex++
*/
#define traceTASK_SWITCHED_IN()                                                            \
    TracingEvent_t ev_traceTASK_SWITCHED_IN = {.eEventID = eSwitchedIn,                    \
                                               .ulCycleCount = rtcGetCounter(),  \
                                               .xTickCount = xTickCount,                   \
                                               .pvTCB = pxCurrentTCB}; \
    xTracingEvents[xTracingEventIndex] = ev_traceTASK_SWITCHED_IN;                         \
    xTracingEventIndex++

#define traceTASK_SWITCHED_OUT()                                                            \
    TracingEvent_t ev_traceTASK_SWITCHED_OUT = {.eEventID = eSwitchedOut,                   \
                                                .ulCycleCount = rtcGetCounter(),  \
                                                .xTickCount = xTickCount,                   \
                                                .pvTCB = pxCurrentTCB}; \
    xTracingEvents[xTracingEventIndex] = ev_traceTASK_SWITCHED_OUT;                         \
    xTracingEventIndex++
/*
#define traceTASK_INCREMENT_TICK(xTickCount)                                                  \
    TracingEvent_t ev_traceTASK_INCREMENT_TICK = {.eEventID = eTaskIncrementTick,             \
                                                  .ulCycleCount = xTaskGetTickCountFromISR(),  \
                                                  .xTickCount = xTickCount,                   \
                                                  .pvTCB = pxCurrentTCBs[portGET_CORE_ID()]}; \
    xTracingEvents[xTracingEventIndex] = ev_traceTASK_INCREMENT_TICK;                         \
    xTracingEventIndex++

#define traceTASK_INCREMENT_TICK_END(xTickCount)                                                  \
    TracingEvent_t ev_traceTASK_INCREMENT_TICK_END = {.eEventID = eTaskIncrementTickEnd,             \
                                                  .ulCycleCount = xTaskGetTickCountFromISR(),  \
                                                  .xTickCount = xTickCount,                   \
                                                  .pvTCB = pxCurrentTCBs[portGET_CORE_ID()]}; \
    xTracingEvents[xTracingEventIndex] = ev_traceTASK_INCREMENT_TICK_END;                         \
    xTracingEventIndex++
*/
#endif /* FREERTOS_TRACING_H */