#ifndef DHT22_SIM_PREFIX_H
#define DHT22_SIM_PREFIX_H

/* Force-included (gcc -include) into the src/app/hal/dht22.c translation unit
 * only.  It pulls in the stub peripheral header first -- so the stub's
 * "extern DWT_Type *DWT;" declaration is parsed once, before anything is
 * macro-replaced -- and then redirects the two fixed-address peripheral objects
 * the driver touches onto the simulator:
 *
 *   DWT       -> a macro that advances the virtual clock, then yields the
 *                simulator's register block.  This is what makes the driver's
 *                busy-wait timeouts and pulse-width measurements terminate and
 *                read back a real elapsed time on a host.
 *   CoreDebug -> the simulator's register block, so DHT22_Init() does not write
 *                to 0xE000EDF0 and crash.
 */
#include "dht22_sim.h"
#include "stm32f1xx_hal.h"

#undef DWT
#define DWT (dht22_sim_tick(), dht22_sim_dwt())

#undef CoreDebug
#define CoreDebug (&dht22_sim_coredebug)

#endif /* DHT22_SIM_PREFIX_H */
