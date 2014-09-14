/* db_vmint.c - bytecode interpreter for a simple virtual machine
 *
 * Copyright (c) 2014 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <propeller.h>
#include "../db_vm.h"
#include "../db_vmdebug.h"

/* VM commands */
enum {
    VM_Done           = 0,
    VM_Continue       = 1,
    VM_ReadLong       = 2,
    VM_WriteLong      = 3,
    VM_ReadByte       = 4
};

/* VM status codes */
enum {
    STS_Fail          = 0,
    STS_Halt          = 1,
    STS_Step          = 2,
    STS_Trap          = 3,
    STS_Success       = 4,
    STS_StackOver     = 5,
    STS_DivideZero    = 6,
    STS_IllegalOpcode = 7
};

/* VM mailbox structure (12 bytes) */
typedef struct {
    volatile uint32_t cmd;
    volatile uint32_t arg_sts;
    volatile uint32_t arg2_fcn;
} VM_Mailbox;

/* VM state structure (20 bytes) */
typedef struct {
    volatile VMVALUE *fp;
    volatile VMVALUE *sp;
    volatile VMVALUE tos;
    volatile uint8_t *pc;
    volatile int stepping;
} VM_State;

/* interpreter state structure */
typedef struct {
    VM_Mailbox mailbox;
    VM_State state;
    VMVALUE *stack;
    VMVALUE *stackTop;
    int cogn;
} Interpreter;

use_cog_driver(notc_vm);

static int StartInterpreter(Interpreter *i, size_t stackSize);
static void StopInterpreter(Interpreter *i);
static void ShowState(Interpreter *i);

/* Execute - execute the main code */
int Execute(System *sys, ImageHdr *image, VMVALUE main)
{
    size_t stackSize;
    Interpreter *i;
    int running;
    
    /* allocate the interpreter state */
    if (!(i = (Interpreter *)AllocateFreeSpace(sys, sizeof(Interpreter))))
        return VMFALSE;

    /* make sure there is space left for the stack */
    if ((stackSize = sys->freeTop - sys->freeNext) < MIN_STACK_SIZE)
        return VMFALSE;
        
    /* start the bytecode interpreter cog */
    if (!StartInterpreter(i, stackSize))
        return VMFALSE;

    i->state.pc = (uint8_t *)main;
    i->state.stepping = 0;
    i->mailbox.cmd = VM_Continue;
        
    running = VMTRUE;
    while (running) {
    
        while (i->mailbox.cmd != VM_Done)
            ;
        
        switch (i->mailbox.arg_sts) {
        case STS_Fail:
            VM_printf("Fail\n");
            running = VMFALSE;
            break;
        case STS_Halt:
            running = VMFALSE;
            break;
        case STS_Step:
            ShowState(i);
            i->mailbox.cmd = VM_Continue;
            break;
        case STS_Trap:
            switch (i->mailbox.arg2_fcn) {
            case TRAP_GetChar:
                *--i->state.sp = i->state.tos;
                i->state.tos = VM_getchar();
                i->mailbox.cmd = VM_Continue;
                break;
            case TRAP_PutChar:
                VM_putchar(i->state.tos);
                i->state.tos = *i->state.sp++;
                i->mailbox.cmd = VM_Continue;
                break;
            case TRAP_PrintStr:
                VM_printf("%s", (char *)i->state.tos);
                i->state.tos = *i->state.sp++;
                i->mailbox.cmd = VM_Continue;
                break;
            case TRAP_PrintInt:
                VM_printf("%d", i->state.tos);
                i->state.tos = *i->state.sp++;
                i->mailbox.cmd = VM_Continue;
                break;
            case TRAP_PrintTab:
                VM_putchar('\t');
                i->mailbox.cmd = VM_Continue;
                break;
            case TRAP_PrintNL:
                VM_putchar('\n');
                i->mailbox.cmd = VM_Continue;
                break;
            case TRAP_PrintFlush:
                fflush(stdout);
                i->mailbox.cmd = VM_Continue;
                break;
            default:
                VM_printf("Unknown trap\n");
                running = VMFALSE;
                break;
            }
            break;
        case STS_Success:
            VM_printf("Success\n");
            running = VMFALSE;
            break;
        case STS_StackOver:
            VM_printf("Stack overflow\n");
            running = VMFALSE;
            break;
        case STS_DivideZero:
            VM_printf("Divide by zero\n");
            running = VMFALSE;
            break;
        case STS_IllegalOpcode:
            VM_printf("Illegal opcode: pc %08x\n", (VMUVALUE)i->state.pc);
            running = VMFALSE;
            break;
        default:
            VM_printf("Unknown status\n");
            running = VMFALSE;
            break;
        }
    }
    
    StopInterpreter(i);
    
    return 0;
}

/* StartInterpreter - start the bytecode interpreter */
static int StartInterpreter(Interpreter *i, size_t stackSize)
{
    VMVALUE *stack, *stackTop;

    stack = (VMVALUE *)((uint8_t *)i + sizeof(Interpreter));
    stackTop = (VMVALUE *)((uint8_t *)stack + stackSize);

    i->stack = stack;
    i->stackTop = stackTop;
    
    i->mailbox.cmd = VM_Continue;
    
    if ((i->cogn = load_cog_driver(notc_vm, i)) < 0)
        return VMFALSE;
        
    while (i->mailbox.cmd != VM_Done)
        ;
        
    return VMTRUE;
}

/* StopInterpreter - stop the bytecode interpreter */
static void StopInterpreter(Interpreter *i)
{
    if (i->cogn >= 0) {
        cogstop(i->cogn);
        i->cogn = -1;
    }
}

/* ShowState - show the state of the interpreter */
static void ShowState(Interpreter *i)
{
    VMVALUE *p = (VMVALUE *)i->state.sp;
    if (p < i->stackTop) {
        VM_printf(" %d", i->state.tos);
        for (; p < i->stackTop - 1; ++p) {
            if (p == i->state.fp)
                VM_printf(" <fp>");
            VM_printf(" %d", *p);
        }
        VM_printf("\n");
    }
    DecodeInstruction((uint8_t *)i->state.pc, (uint8_t *)i->state.pc);
}
