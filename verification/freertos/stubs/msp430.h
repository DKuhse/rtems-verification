/*
 * Stub for <msp430.h>.
 *
 * The real header (shipped with msp430-elf-gcc) maps hardware register
 * names to memory-mapped IO addresses. The EDF scheduler logic doesn't
 * touch those, but the FreeRTOS port headers (portmacro.h) reference a
 * handful of them in inline helpers that Frama-C parses transitively.
 *
 * We model the registers as plain volatile globals so they parse and
 * type-check, which is enough for verification of code paths that don't
 * actually depend on their values.
 */
#ifndef MSP430_STUB_H
#define MSP430_STUB_H

#include <stdint.h>

/* Real-time clock counter halves — read in portmacro.h:rtcGetCounter(). */
extern volatile uint16_t RTCTIM0;
extern volatile uint16_t RTCTIM1;

/* Power management module — referenced by portRESET_POR() macro. */
extern volatile uint16_t PMMCTL0;
#define PMMPW       ((uint16_t)0xA500)
#define PMMSWBOR    ((uint16_t)0x0004)

/* Tick interrupt vector — used as a #define address in port.c. */
#define TIMER0_A0_VECTOR 0xFFE0

#endif /* MSP430_STUB_H */
