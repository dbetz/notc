/* db_vmdebug.h - debug definitions
 *
 * Copyright (c) 2014 by David Michael Betz.  All rights reserved.
 *
 */

#ifndef __DB_VMDEBUG_H__
#define __DB_VMDEBUG_H__

#include "db_types.h"

/* instruction output formats */
#define FMT_NONE        0
#define FMT_BYTE        1
#define FMT_SBYTE       2
#define FMT_LONG        3
#define FMT_BR          4

typedef struct {
    int code;
    char *name;
    int fmt;
} OTDEF;

extern OTDEF OpcodeTable[];

void DecodeFunction(const uint8_t *code, int len);
int DecodeInstruction(const uint8_t *code, const uint8_t *lc);

#endif
