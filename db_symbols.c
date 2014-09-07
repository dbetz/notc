/* db_symbols.c - symbol table routines
 *
 * Copyright (c) 2014 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "db_compiler.h"
#include "db_symbols.h"

/* local function prototypes */
static Symbol *AddLocalSymbol(ParseContext *c, SymbolTable *table, const char *name, StorageClass storageClass, int value);

/* InitSymbolTable - initialize a symbol table */
void InitSymbolTable(SymbolTable *table)
{
    table->head = NULL;
    table->pTail = &table->head;
    table->count = 0;
}

/* AddGlobal - add a global symbol to the symbol table */
Symbol *AddGlobal(ParseContext *c, const char *name, StorageClass storageClass, VMVALUE value)
{
    int size = sizeof(Symbol) + strlen(name);
    Symbol *sym;
    
    /* check to see if the symbol is already defined */
    for (sym = c->image->globals.head; sym != NULL; sym = sym->next)
        if (strcmp(name, sym->name) == 0)
            return sym;
    
    /* allocate the symbol structure */
    sym = (Symbol *)AllocateImageSpace(c->image, size);
    sym->storageClass = storageClass;
    strcpy(sym->name, name);
    sym->value = value;
    sym->next = NULL;

    /* add it to the symbol table */
    *c->image->globals.pTail = sym;
    c->image->globals.pTail = &sym->next;
    ++c->image->globals.count;
    
    /* return the symbol */
    return sym;
}

/* AddArgument - add an argument symbol to symbol table */
Symbol *AddArgument(ParseContext *c, const char *name, StorageClass storageClass, int value)
{
    return AddLocalSymbol(c, &c->arguments, name, storageClass, value);
}

/* AddLocal - add a local symbol to the symbol table */
Symbol *AddLocal(ParseContext *c, const char *name, StorageClass storageClass, int value)
{
    return AddLocalSymbol(c, &c->locals, name, storageClass, value);
}

/* AddLocalSymbol - add a symbol to a local symbol table */
static Symbol *AddLocalSymbol(ParseContext *c, SymbolTable *table, const char *name, StorageClass storageClass, int value)
{
    size_t size = sizeof(Symbol) + strlen(name);
    Symbol *sym;
    
    /* allocate the symbol structure */
    sym = (Symbol *)LocalAlloc(c, size);
    strcpy(sym->name, name);
    sym->storageClass = storageClass;
    sym->value = value;
    sym->next = NULL;

    /* add it to the symbol table */
    *table->pTail = sym;
    table->pTail = &sym->next;
    ++table->count;
    
    /* return the symbol */
    return sym;
}

/* FindSymbol - find a symbol in a symbol table */
Symbol *FindSymbol(SymbolTable *table, const char *name)
{
    Symbol *sym = table->head;
    while (sym) {
        if (strcmp(name, sym->name) == 0)
            return sym;
        sym = sym->next;
    }
    return NULL;
}

/* IsConstant - check to see if the value of a symbol is a constant */
int IsConstant(Symbol *symbol)
{
    return symbol->storageClass == SC_CONSTANT;
}

/* DumpSymbols - dump a symbol table */
void DumpSymbols(SymbolTable *table, char *tag)
{
    Symbol *sym;
    if ((sym = table->head) != NULL) {
        VM_printf("%s:\n", tag);
        for (; sym != NULL; sym = sym->next)
            VM_printf("  %s %08x: %08x\n", sym->name, (VMVALUE)sym, sym->value);
    }
}
