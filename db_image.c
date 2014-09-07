/* db_image.c - compiled image functions
 *
 * Copyright (c) 2014 by David Michael Betz.  All rights reserved.
 *
 */

#include "db_system.h"
#include "db_image.h"

/* AllocateImage - allocate a program image */
ImageHdr *AllocateImage(System *sys, size_t size)
{
    ImageHdr *image;
    if (!(image = (ImageHdr *)AllocateFreeSpace(sys, size)))
        return NULL;
    image->heapTop = (uint8_t *)image + size;
    InitImage(image);
    return image;
}
        
/* InitImage - initialize an image */
void InitImage(ImageHdr *image)
{
    InitSymbolTable(&image->globals);
    image->codeBuf = image->codeFree = image->data;
    image->heapFree = image->heapTop;
    image->strings = NULL;
}

/* AllocateImageSpace - allocate image space */
void *AllocateImageSpace(ImageHdr *image, size_t size)
{
    size = (size + ALIGN_MASK) & ~ALIGN_MASK;
    if (image->heapFree - size < image->codeFree)
        return NULL;
    image->heapFree -= size;
    return image->heapFree;
}

/* StoreVector - store a vector */
VMVALUE StoreVector(ImageHdr *image, const VMVALUE *buf, size_t size)
{
    return StoreBVector(image, (uint8_t *)buf, size * sizeof(VMVALUE));
}

/* StoreBVector - store a byte vector */
VMVALUE StoreBVector(ImageHdr *image, const uint8_t *buf, size_t size)
{
    void *addr;
    if (!(addr = AllocateImageSpace(image, size)))
        return 0;
    memcpy(addr, buf, size);
    return (VMVALUE)addr;
}
