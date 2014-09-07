/* ebasic.c - a simple basic interpreter
 *
 * Copyright (c) 2014 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdio.h>
#include "db_compiler.h"
#include "db_image.h"
#include "db_vm.h"

static uint8_t space[HEAPSIZE];

static int TermGetLine(void *cookie, char *buf, int len, VMVALUE *pLineNumber);

int main(int argc, char *argv[])
{
    VMVALUE lineNumber = 0;
    ImageHdr *image;
    VMVALUE code;
    System *sys;

    VM_printf("notc 0.001\n");

    VM_sysinit(argc, argv);

    sys = InitSystem(space, sizeof(space));
    sys->getLine = TermGetLine;
    sys->getLineCookie = &lineNumber;

    if (!(image = AllocateImage(sys, IMAGESIZE)))
        return 1;
        
    sys->freeMark = sys->freeNext;

    for (;;) {
        if ((code = Compile(sys, image)) != 0) {
            sys->freeNext = sys->freeMark;
            Execute(sys, image, code);
        }
    }

    return 0;
}

static int TermGetLine(void *cookie, char *buf, int len, VMVALUE *pLineNumber)
{
    VMVALUE *pLine = (VMVALUE *)cookie;
    *pLineNumber = ++(*pLine);
    return VM_getline(buf, len) != NULL;
}
