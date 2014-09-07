/* db_system.h - global system context
 *
 * Copyright (c) 2014 by David Michael Betz.  All rights reserved.
 *
 */

#ifndef __DB_SYSTEM_H__
#define __DB_SYSTEM_H__

#include <stdarg.h>
#include <setjmp.h>
#include "db_types.h"

/* program limits */
#define MAXLINE         128

/* line input handler */
typedef int GetLineHandler(void *cookie, char *buf, int len, VMVALUE *pLineNumber);

/* system context */
typedef struct {
    jmp_buf errorTarget;        /* error target */
    GetLineHandler *getLine;    /* function to get a line of input */
    void *getLineCookie;        /* cookie for the getLine function */
    int lineNumber;             /* current line number */
    uint8_t *freeSpace;         /* base of free space */
    uint8_t *freeMark;          /* top of permanently allocated storage */
    uint8_t *freeNext;          /* next free space available */
    uint8_t *freeTop;           /* top of free space */
    char lineBuf[MAXLINE];      /* current input line */
    char *linePtr;              /* pointer to the current character */
} System;

System *InitSystem(uint8_t *freeSpace, size_t freeSize);
uint8_t *AllocateFreeSpace(System *sys, size_t size);
int GetLine(System *sys);
void Abort(System *sys, const char *fmt, ...);

/* directory entry structure (platform specific) */
typedef struct VMDIRENT VMDIRENT;

/* open directory structure (platform specific) */
typedef struct VMDIR VMDIR;

void VM_sysinit(int argc, char *argv[]);
int VM_getchar(void);
char *VM_getline(char *buf, int size);
void VM_printf(const char *fmt, ...);
void VM_vprintf(const char *fmt, va_list ap);
void VM_putchar(int ch);
void VM_flush(void);
int VM_opendir(const char *path, VMDIR *dir);
int VM_readdir(VMDIR *dir, VMDIRENT *entry);
void VM_closedir(VMDIR *dir);

#endif
