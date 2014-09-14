/* db_generate.c - code generation functions
 *
 * Copyright (c) 2014 by David Michael Betz.  All rights reserved.
 *
 */

#include "db_compiler.h"

/* local function prototypes */
static void code_expr(ParseContext *c, ParseTreeNode *expr, PVAL *pv);
static void code_shortcircuit(ParseContext *c, int op, ParseTreeNode *expr, PVAL *pv);
static void code_arrayref(ParseContext *c, ParseTreeNode *expr, PVAL *pv);
static void code_call(ParseContext *c, ParseTreeNode *expr, PVAL *pv);
VMWORD rd_cword(ParseContext *c, VMUVALUE off);
void wr_cword(ParseContext *c, VMUVALUE off, VMWORD v);
static VMVALUE rd_clong(ParseContext *c, VMUVALUE off);
static void wr_clong(ParseContext *c, VMUVALUE off, VMVALUE v);

/* code_lvalue - generate code for an l-value expression */
void code_lvalue(ParseContext *c, ParseTreeNode *expr, PVAL *pv)
{
    code_expr(c, expr, pv);
    chklvalue(c, pv);
}

/* code_rvalue - generate code for an r-value expression */
void code_rvalue(ParseContext *c, ParseTreeNode *expr)
{
    PVAL pv;
    code_expr(c, expr, &pv);
    rvalue(c, &pv);
}

/* code_expr - generate code for an expression parse tree */
static void code_expr(ParseContext *c, ParseTreeNode *expr, PVAL *pv)
{
    VMVALUE ival;
    PVAL pv2;
    switch (expr->nodeType) {
    case NodeTypeGlobalSymbolRef:
        putcbyte(c, OP_LIT);
        if (expr->u.symbolRef.symbol->storageClass == SC_HWVARIABLE)
            putclong(c, expr->u.symbolRef.symbol->value);
        else
            putclong(c, (VMVALUE)expr->u.symbolRef.symbol);
        *pv = VT_LVALUE;
        break;
    case NodeTypeLocalSymbolRef:
        putcbyte(c, OP_LADDR);
        putcbyte(c, expr->u.symbolRef.offset);
        *pv = VT_LVALUE;
        break;
    case NodeTypeStringLit:
        putcbyte(c, OP_LIT);
        putclong(c, (VMVALUE)&expr->u.stringLit.string->data);
        *pv = VT_RVALUE;
        break;
    case NodeTypeIntegerLit:
        ival = expr->u.integerLit.value;
        if (ival >= -128 && ival <= 127) {
            putcbyte(c, OP_SLIT);
            putcbyte(c, ival);
        }
        else {
            putcbyte(c, OP_LIT);
            putclong(c, ival);
        }
        *pv = VT_RVALUE;
        break;
    case NodeTypeFunctionLit:
        putcbyte(c, OP_LIT);
        putclong(c, expr->u.functionLit.offset);
        *pv = VT_RVALUE;
        break;
    case NodeTypePreincrementOp:
        code_lvalue(c, expr->u.incrementOp.expr, &pv2);
        putcbyte(c, OP_DUP);
        putcbyte(c, OP_LOAD);
        putcbyte(c, OP_SLIT);
        putcbyte(c, expr->u.incrementOp.increment);
        putcbyte(c, OP_ADD);
        putcbyte(c, OP_STORE);
        *pv = VT_RVALUE;
        break;
    case NodeTypePostincrementOp:
        code_lvalue(c, expr->u.incrementOp.expr, &pv2);
        putcbyte(c, OP_DUP);
        putcbyte(c, OP_LOAD);
        putcbyte(c, OP_TUCK);
        putcbyte(c, OP_SLIT);
        putcbyte(c, expr->u.incrementOp.increment);
        putcbyte(c, OP_ADD);
        putcbyte(c, OP_STORE);
        putcbyte(c, OP_DROP);
        *pv = VT_RVALUE;
        break;
    case NodeTypeUnaryOp:
        code_rvalue(c, expr->u.unaryOp.expr);
        putcbyte(c, expr->u.unaryOp.op);
        *pv = VT_RVALUE;
        break;
    case NodeTypeBinaryOp:
        code_rvalue(c, expr->u.binaryOp.left);
        code_rvalue(c, expr->u.binaryOp.right);
        putcbyte(c, expr->u.binaryOp.op);
        *pv = VT_RVALUE;
        break;
    case NodeTypeAssignmentOp:
        if (expr->u.binaryOp.op == OP_EQ) {
            code_lvalue(c, expr->u.binaryOp.left, &pv2);
            code_rvalue(c, expr->u.binaryOp.right);
            putcbyte(c, OP_STORE);
        }
        else {
            code_lvalue(c, expr->u.binaryOp.left, &pv2);
            putcbyte(c, OP_DUP);
            putcbyte(c, OP_LOAD);
            code_rvalue(c, expr->u.binaryOp.right);
            putcbyte(c, expr->u.binaryOp.op);
            putcbyte(c, OP_STORE);
        }
        *pv = VT_RVALUE;
        break;
    case NodeTypeArrayRef:
        code_arrayref(c, expr, pv);
        break;
    case NodeTypeFunctionCall:
        code_call(c, expr, pv);
        break;
    case NodeTypeDisjunction:
        code_shortcircuit(c, OP_BRTSC, expr, pv);
        break;
    case NodeTypeConjunction:
        code_shortcircuit(c, OP_BRFSC, expr, pv);
        break;
    }
}

/* code_shortcircuit - generate code for a conjunction or disjunction of boolean expressions */
static void code_shortcircuit(ParseContext *c, int op, ParseTreeNode *expr, PVAL *pv)
{
    ExprListEntry *entry = expr->u.exprList.exprs;
    int end = 0;

    code_rvalue(c, entry->expr);
    entry = entry->next;

    do {
        putcbyte(c, op);
        end = putcword(c, end);
        code_rvalue(c, entry->expr);
    } while ((entry = entry->next) != NULL);

    fixupbranch(c, end, codeaddr(c));

    *pv = VT_RVALUE;
}

/* code_arrayref - code an array reference */
static void code_arrayref(ParseContext *c, ParseTreeNode *expr, PVAL *pv)
{
    /* code the array reference */
    code_rvalue(c, expr->u.arrayRef.array);

    /* code the index */
    code_rvalue(c, expr->u.arrayRef.index);

    /* code the index operation */
    putcbyte(c, OP_INDEX);

    *pv = VT_LVALUE;
}

/* code_call - code a function call */
static void code_call(ParseContext *c, ParseTreeNode *expr, PVAL *pv)
{
    ExprListEntry *arg;

    /* code each argument expression */
    for (arg = expr->u.functionCall.args; arg != NULL; arg = arg->next)
        code_rvalue(c, arg->expr);

    /* call the function */
    code_rvalue(c, expr->u.functionCall.fcn);
    putcbyte(c, OP_CALL);
    putcbyte(c, expr->u.functionCall.argc);

    /* we've got an rvalue now */
    *pv = VT_RVALUE;
}

/* rvalue - get the rvalue of a partial expression */
void rvalue(ParseContext *c, PVAL *pv)
{
    if (*pv == VT_LVALUE) {
        putcbyte(c, OP_LOAD);
        *pv = VT_RVALUE;
    }
}

/* chklvalue - make sure we've got an lvalue */
void chklvalue(ParseContext *c, PVAL *pv)
{
    if (*pv == VT_RVALUE)
        ParseError(c,"expecting an lvalue");
}

/* codeaddr - get the current code address (actually, offset) */
int codeaddr(ParseContext *c)
{
    return (int)(c->image->codeFree - c->image->codeBuf);
}

/* putcbyte - put a code byte into the code buffer */
int putcbyte(ParseContext *c, int b)
{
    int addr = codeaddr(c);
    if (c->image->codeFree >= c->image->heapFree)
        Abort(c->sys, "insufficient memory");
    *c->image->codeFree++ = b;
    return addr;
}

/* putcword - put a code word into the code buffer */
int putcword(ParseContext *c, VMWORD v)
{
    int addr = codeaddr(c);
    if (c->image->codeFree + sizeof(VMWORD) > c->image->heapFree)
        Abort(c->sys, "insufficient memory");
    wr_cword(c, c->image->codeFree - c->image->codeBuf, v);
    c->image->codeFree += sizeof(VMWORD);
    return addr;
}

/* rd_cword - get a code word from the code buffer */
VMWORD rd_cword(ParseContext *c, VMUVALUE off)
{
    int cnt = sizeof(VMWORD);
    VMWORD v = 0;
    while (--cnt >= 0)
        v = (v << 8) | c->image->codeBuf[off++];
    return v;
}

/* wr_cword - put a code word into the code buffer */
void wr_cword(ParseContext *c, VMUVALUE off, VMWORD v)
{
    uint8_t *p = &c->image->codeBuf[off] + sizeof(VMWORD);
    int cnt = sizeof(VMWORD);
    while (--cnt >= 0) {
        *--p = v;
        v >>= 8;
    }
}

/* fixupbranch - fixup a branch reference chain */
void fixupbranch(ParseContext *c, VMUVALUE chn, VMUVALUE val)
{
    while (chn != 0) {
        int nxt = rd_cword(c, chn);
        VMWORD off = val - (chn + sizeof(VMWORD)); /* this assumes all 1+sizeof(VMWORD) byte branch instructions */
        wr_cword(c, chn, off);
        chn = nxt;
    }
}

/* putclong - put a code word into the code buffer */
int putclong(ParseContext *c, VMVALUE v)
{
    int addr = codeaddr(c);
    if (c->image->codeFree + sizeof(VMVALUE) > c->image->heapFree)
        Abort(c->sys, "insufficient memory");
    wr_clong(c, c->image->codeFree - c->image->codeBuf, v);
    c->image->codeFree += sizeof(VMVALUE);
    return addr;
}

/* rd_clong - get a code word from the code buffer */
VMVALUE rd_clong(ParseContext *c, VMUVALUE off)
{
    int cnt = sizeof(VMVALUE);
    VMVALUE v = 0;
    while (--cnt >= 0)
        v = (v << 8) | c->image->codeBuf[off++];
    return v;
}

/* wr_clong - put a code word into the code buffer */
void wr_clong(ParseContext *c, VMUVALUE off, VMVALUE v)
{
    uint8_t *p = &c->image->codeBuf[off] + sizeof(VMVALUE);
    int cnt = sizeof(VMVALUE);
    while (--cnt >= 0) {
        *--p = v;
        v >>= 8;
    }
}

/* fixup - fixup a reference chain */
void fixup(ParseContext *c, VMUVALUE chn, VMUVALUE val)
{
    while (chn != 0) {
        int nxt = rd_clong(c, chn);
        wr_clong(c, chn, val);
        chn = nxt;
    }
}
