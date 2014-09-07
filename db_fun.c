/* db_fun.c - built-in functions
 *
 * Copyright (c) 2014 by David Michael Betz.  All rights reserved.
 *
 */

#include "db_compiler.h"

#ifdef PROPELLER_GCC

static uint8_t bi_waitcnt[] = {
    OP_FRAME, 2,
    OP_LREF, 0,
    OP_NATIVE, 0xf8,0x7c,0x0a,0x00, // waitcnt tos, #0
    OP_RETURN
};

static uint8_t bi_waitpeq[] = {
    OP_FRAME, 2,
    OP_LREF, 1,                     // get mask
    OP_NATIVE, 0xa0,0xbc,0x02,0x05, // mov t1, tos
    OP_LREF, 0,                     // get state
    OP_NATIVE, 0xf0,0xbc,0x0a,0x01, // waitpeq tos, t1 wr
    OP_RETURN
};

static uint8_t bi_waitpne[] = {
    OP_FRAME, 2,
    OP_LREF, 1,                     // get mask
    OP_NATIVE, 0xa0,0xbc,0x02,0x05, // mov t1, tos
    OP_LREF, 0,                     // get state
    OP_NATIVE, 0xf4,0xbc,0x0a,0x01, // waitpne tos, t1 wr
    OP_RETURN
};

#define COG_BASE    0x10000000

#endif

void EnterBuiltInSymbols(ParseContext *c)
{
#ifdef PROPELLER_GCC
    /* variables */
    AddGlobal(c, "clkfreq",  SC_HWVARIABLE, 0x00000000);
    AddGlobal(c, "par",      SC_HWVARIABLE, COG_BASE + 0x1f0 * 4);
    AddGlobal(c, "cnt",      SC_HWVARIABLE, COG_BASE + 0x1f1 * 4);
    AddGlobal(c, "ina",      SC_HWVARIABLE, COG_BASE + 0x1f2 * 4);
    AddGlobal(c, "inb",      SC_HWVARIABLE, COG_BASE + 0x1f3 * 4);
    AddGlobal(c, "outa",     SC_HWVARIABLE, COG_BASE + 0x1f4 * 4);
    AddGlobal(c, "outb",     SC_HWVARIABLE, COG_BASE + 0x1f5 * 4);
    AddGlobal(c, "dira",     SC_HWVARIABLE, COG_BASE + 0x1f6 * 4);
    AddGlobal(c, "dirb",     SC_HWVARIABLE, COG_BASE + 0x1f7 * 4);
    AddGlobal(c, "ctra",     SC_HWVARIABLE, COG_BASE + 0x1f8 * 4);
    AddGlobal(c, "ctrb",     SC_HWVARIABLE, COG_BASE + 0x1f9 * 4);
    AddGlobal(c, "frqa",     SC_HWVARIABLE, COG_BASE + 0x1fa * 4);
    AddGlobal(c, "frqb",     SC_HWVARIABLE, COG_BASE + 0x1fb * 4);
    AddGlobal(c, "phsa",     SC_HWVARIABLE, COG_BASE + 0x1fc * 4);
    AddGlobal(c, "phsb",     SC_HWVARIABLE, COG_BASE + 0x1fd * 4);
    AddGlobal(c, "vcfg",     SC_HWVARIABLE, COG_BASE + 0x1fe * 4);
    AddGlobal(c, "vscl",     SC_HWVARIABLE, COG_BASE + 0x1ff * 4);
    
    /* functions */
    AddGlobal(c, "waitcnt",  SC_VARIABLE,   (VMVALUE)bi_waitcnt);
    AddGlobal(c, "waitpeq",  SC_VARIABLE,   (VMVALUE)bi_waitpeq);
    AddGlobal(c, "waitpne",  SC_VARIABLE,   (VMVALUE)bi_waitpne);
#endif
}

