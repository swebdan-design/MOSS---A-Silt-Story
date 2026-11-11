/* VACANT - platform glue so generator and composer also build on a PC
 * for host tests (compile with -DHOST). */
#ifndef VACANT_PLATFORM_H
#define VACANT_PLATFORM_H
#include <stdint.h>

#ifdef HOST
#define BANKED
#define NONBANKED
#define CRITICAL
#define INTERRUPT
#else
#include <gb/gb.h>
#endif

#endif
