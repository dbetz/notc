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

/* interpreter state structure */
typedef struct {
    int cogn;
} Interpreter;

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

/* VM mailbox structure */
typedef struct {
    volatile uint32_t cmd;
    volatile uint32_t arg_sts;
    volatile uint32_t arg2_fcn;
} VM_Mailbox;

/* VM state structure */
typedef struct {
    volatile VMVALUE *fp;
    volatile VMVALUE *sp;
    volatile VMVALUE tos;
    volatile uint8_t *pc;
    volatile int stepping;
} VM_State;

/* VM initialization structure */
typedef struct {
    VM_Mailbox *mailbox;
    VM_State *state;
    VMVALUE *stack;
    VMVALUE *stackTop;
} VM_Init;

static VM_Mailbox mailbox;
static VM_State state;
static int cog;

use_cog_driver(notc_vm);

static int StartInterpreter(Interpreter *i, size_t stackSize);
static void StopInterpreter(Interpreter *i);

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

    state.pc = (uint8_t *)main;
    state.stepping = 1;
    mailbox.cmd = VM_Continue;
        
    running = VMTRUE;
    while (running) {
    
        while (mailbox.cmd != VM_Done)
            ;
        
        switch (mailbox.arg_sts) {
        case STS_Fail:
            VM_printf("Fail\n");
            running = VMFALSE;
            break;
        case STS_Halt:
            running = VMFALSE;
            break;
        case STS_Step:
            VM_printf("Step: pc %08x, sp %08x, fp %08x, tos %08x\n", (VMUVALUE)state.pc, (VMUVALUE)state.sp, (VMUVALUE)state.fp, state.tos);
            mailbox.cmd = VM_Continue;
            break;
        case STS_Trap:
            switch (mailbox.arg2_fcn) {
            case TRAP_GetChar:
                *--state.sp = state.tos;
                state.tos = getchar();
                mailbox.cmd = VM_Continue;
                break;
            case TRAP_PutChar:
                putchar(state.tos);
                state.tos = *state.sp++;
                mailbox.cmd = VM_Continue;
                break;
            case TRAP_PrintStr:
                VM_printf("%s", (char *)state.tos);
                state.tos = *state.sp++;
                mailbox.cmd = VM_Continue;
                break;
            case TRAP_PrintInt:
                VM_printf("%d", state.tos);
                state.tos = *state.sp++;
                mailbox.cmd = VM_Continue;
                break;
            case TRAP_PrintTab:
                putchar('\t');
                mailbox.cmd = VM_Continue;
                break;
            case TRAP_PrintNL:
                putchar('\n');
                mailbox.cmd = VM_Continue;
                break;
            case TRAP_PrintFlush:
                fflush(stdout);
                mailbox.cmd = VM_Continue;
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
            VM_printf("Illegal opcode: pc %08x\n", (VMUVALUE)state.pc);
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
    VM_Init init;

    stack = (VMVALUE *)((uint8_t *)i + sizeof(Interpreter));
    stackTop = (VMVALUE *)((uint8_t *)stack + stackSize);

    init.mailbox = &mailbox;
    init.state = &state;
    init.stack = stack;
    init.stackTop = stackTop;
    
    mailbox.cmd = VM_Continue;
    
    if ((cog = load_cog_driver(notc_vm, &init)) < 0)
        return VMFALSE;
        
    while (mailbox.cmd != VM_Done)
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

