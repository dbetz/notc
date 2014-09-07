/* db_image.h - compiled image file definitions
 *
 * Copyright (c) 2014 by David Michael Betz.  All rights reserved.
 *
 */

#ifndef __DB_IMAGE_H__
#define __DB_IMAGE_H__

#include <stdarg.h>
#include "db_symbols.h"
#include "db_system.h"
#include "db_types.h"

/* forward type declarations */
typedef struct String String;

/* string structure */
struct String {
    String *next;
    char data[1];
};

/* image header */
typedef struct {
    SymbolTable globals;    /* global variables and constants */
    String *strings;        /* string constants */
    uint8_t *codeBuf;       /* code starts at beginning of heap */
    uint8_t *codeFree;      /* next available code location */
    uint8_t *heapFree;      /* next free heap location */
    uint8_t *heapTop;       /* top of heap */
    uint8_t data[1];        /* data space */
} ImageHdr;

/* opcodes */
#define OP_HALT         0x00    /* halt */
#define OP_BRT          0x01    /* branch on true */
#define OP_BRTSC        0x02    /* branch on true (for short circuit booleans) */
#define OP_BRF          0x03    /* branch on false */
#define OP_BRFSC        0x04    /* branch on false (for short circuit booleans) */
#define OP_BR           0x05    /* branch unconditionally */
#define OP_NOT          0x06    /* logical negate top of stack */
#define OP_NEG          0x07    /* negate */
#define OP_ADD          0x08    /* add two numeric expressions */
#define OP_SUB          0x09    /* subtract two numeric expressions */
#define OP_MUL          0x0a    /* multiply two numeric expressions */
#define OP_DIV          0x0b    /* divide two numeric expressions */
#define OP_REM          0x0c    /* remainder of two numeric expressions */
#define OP_BNOT         0x0d    /* bitwise not of two numeric expressions */
#define OP_BAND         0x0e    /* bitwise and of two numeric expressions */
#define OP_BOR          0x0f    /* bitwise or of two numeric expressions */
#define OP_BXOR         0x10    /* bitwise exclusive or */
#define OP_SHL          0x11    /* shift left */
#define OP_SHR          0x12    /* shift right */
#define OP_LT           0x13    /* less than */
#define OP_LE           0x14    /* less than or equal to */
#define OP_EQ           0x15    /* equal to */
#define OP_NE           0x16    /* not equal to */
#define OP_GE           0x17    /* greater than or equal to */
#define OP_GT           0x18    /* greater than */
#define OP_LIT          0x19    /* load a literal */
#define OP_SLIT         0x1a    /* load a short literal (-128 to 127) */
#define OP_LOAD         0x1b    /* load a long from memory */
#define OP_LOADB        0x1c    /* load a byte from memory */
#define OP_STORE        0x1d    /* store a long into memory */
#define OP_STOREB       0x1e    /* store a byte into memory */
#define OP_LREF         0x1f    /* load a local variable relative to the frame pointer */
#define OP_LSET         0x20    /* set a local variable relative to the frame pointer */
#define OP_INDEX        0x21    /* index into a vector of longs */
#define OP_CALL         0x22    /* call a function */
#define OP_FRAME        0x23    /* create a stack frame */
#define OP_RETURN       0x24    /* remove a stack frame and return from a function call */
#define OP_DROP         0x25    /* drop the top element of the stack */
#define OP_DUP          0x26    /* duplicate the top element of the stack */
#define OP_NATIVE       0x27    /* execute native code */
#define OP_TRAP         0x28    /* trap to handler */

/* VM trap codes */
enum {
    TRAP_GetChar      = 0,
    TRAP_PutChar      = 1,
    TRAP_PrintStr     = 2,
    TRAP_PrintInt     = 3,
    TRAP_PrintTab     = 4,
    TRAP_PrintNL      = 5,
    TRAP_PrintFlush   = 6,
};

/* prototypes */
ImageHdr *AllocateImage(System *sys, size_t imageBufferSize);
void InitImage(ImageHdr *image);
void *AllocateImageSpace(ImageHdr *image, size_t size);
VMVALUE StoreVector(ImageHdr *image, const VMVALUE *buf, size_t size);
VMVALUE StoreBVector(ImageHdr *image, const uint8_t *buf, size_t size);

#endif
