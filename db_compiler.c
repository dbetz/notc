/* db_compiler.c - a simple basic compiler
 *
 * Copyright (c) 2014 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <string.h>
#include <ctype.h>
#include "db_compiler.h"
#include "db_vmdebug.h"
#include "db_vm.h"

#define DEBUG

/* Compile - compile a program */
VMVALUE Compile(System *sys, ImageHdr *image)
{
    VMVALUE mainCode;
    ParseContext *c;
    int tkn;

    /* allocate and initialize the parse context */
    if (!(c = (ParseContext *)AllocateFreeSpace(sys, sizeof(ParseContext))))
        return 0;
    memset(c, 0, sizeof(ParseContext));
    c->sys = sys;
    c->image = image;
    
    /* setup an error target */
    if (setjmp(c->sys->errorTarget) != 0)
        return 0;

    /* use the rest of the free space for the compiler heap */
    c->heapBase = c->heapFree = sys->freeNext;
    c->heapTop = sys->freeTop;

    /* initialize block nesting table */
    c->btop = (Block *)((char *)c->blockBuf + sizeof(c->blockBuf));
    c->bptr = c->blockBuf - 1;

    /* initialize the label table */
    c->labels = NULL;

    /* start in the main code */
    c->codeType = CODE_TYPE_MAIN;

    /* enter built-in symbols */
    EnterBuiltInSymbols(c);

    /* initialize scanner */
    c->inComment = VMFALSE;
    
    /* parse a statement */
    do {
        if ((tkn = GetToken(c)) == T_EOF)
            break;
        ParseStatement(c, tkn);
    } while (c->bptr >= c->blockBuf);

    /* end the main code with a halt */
    putcbyte(c, OP_HALT);
    
    /* write the main code */
    StartCode(c, CODE_TYPE_MAIN);
    mainCode = StoreCode(c);

#ifdef DEBUG
    {
        int size = image->heapTop - image->data;
        size -= image->heapFree - image->codeFree;
        DumpSymbols(&c->image->globals, "symbols");
        VM_printf("size: %d\n", size);
    }
#endif

    /* return the main function */
    return mainCode;
}

/* StartCode - start a function or method under construction */
void StartCode(ParseContext *c, CodeType type)
{
    ImageHdr *image = c->image;
    
    /* all methods must precede the main code */
    if (type != CODE_TYPE_MAIN && image->codeFree > image->codeBuf)
        ParseError(c, "subroutines and functions must precede the main code");

    /* don't allow nested functions or subroutines (for now anyway) */
    if (type != CODE_TYPE_MAIN && c->codeType != CODE_TYPE_MAIN)
        ParseError(c, "nested subroutines and functions are not supported");

    /* initialize the code object under construction */
    InitSymbolTable(&c->arguments);
    InitSymbolTable(&c->locals);
    c->localOffset = 0;
    c->codeType = type;
    
    /* write the code prolog */
    if (type != CODE_TYPE_MAIN) {
        putcbyte(c, OP_FRAME);
        putcbyte(c, 0);
    }
}

/* StoreCode - store the function or method under construction */
VMVALUE StoreCode(ParseContext *c)
{
    ImageHdr *image = c->image;
    VMVALUE code;
    size_t size;

    /* check for unterminated blocks */
    switch (CurrentBlockType(c)) {
    case BLOCK_DEF:
        //ParseError(c, "expecting '}'");
        break;
    case BLOCK_IF:
        ParseError(c, "expecting statement after 'if'");
    case BLOCK_ELSE:
        ParseError(c, "expecting statement after 'else'");
    case BLOCK_FOR:
        ParseError(c, "expecting statement after 'for'");
    case BLOCK_WHILE:
        ParseError(c, "expecting statement after 'while'");
    case BLOCK_DO:
        ParseError(c, "expecting statement after 'do'");
    case BLOCK_BLOCK:
        ParseError(c, "expecting '}'");
    case BLOCK_NONE:
        break;
    }

    /* fixup the RESERVE instruction at the start of the code */
    if (c->codeType != CODE_TYPE_MAIN) {
        image->codeBuf[1] = 2 + c->localOffset;
        putcbyte(c, OP_RETURN);
    }

    /* make sure all referenced labels were defined */
    CheckLabels(c);
    
    /* get the address of the compiled code */
    code = (VMVALUE)image->codeBuf;
    size = image->codeFree - image->codeBuf;
    image->codeBuf += (size + ALIGN_MASK) & ~ALIGN_MASK;
    image->codeFree = image->codeBuf;

#ifdef DEBUG
{
    VM_printf("%s:\n", c->codeSymbol ? c->codeSymbol->name : "<main>");
    DecodeFunction((uint8_t *)code, size);
    DumpSymbols(&c->arguments, "arguments");
    DumpSymbols(&c->locals, "locals");
    VM_printf("\n");
}
#endif

    /* empty the local heap */
    c->heapFree = c->heapBase;
    InitSymbolTable(&c->arguments);
    InitSymbolTable(&c->locals);
    c->labels = NULL;

    /* reset to compile the next code */
    c->codeType = CODE_TYPE_MAIN;
    
    /* return the code vector */
    return code;
}

/* AddString - add a string to the string table */
String *AddString(ParseContext *c, char *value)
{
    String *str;
    int size;
    
    /* check to see if the string is already in the table */
    for (str = c->image->strings; str != NULL; str = str->next)
        if (strcmp(value, str->data) == 0)
            return str;

    /* allocate the string structure */
    size = sizeof(String) + strlen(value);
    str = (String *)AllocateImageSpace(c->image, size);
    strcpy((char *)str->data, value);
    str->next = c->image->strings;
    c->image->strings = str;

    /* return the string table entry */
    return str;
}

/* LocalAlloc - allocate memory from the local heap */
void *LocalAlloc(ParseContext *c, size_t size)
{
    void *addr = c->heapFree;
    size = (size + ALIGN_MASK) & ~ALIGN_MASK;
    if (c->heapFree + size > c->heapTop)
        Abort(c->sys, "insufficient memory");
    c->heapFree += size;
    return addr;
}
