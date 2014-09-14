/* db_statement.c - statement parser
 *
 * Copyright (c) 2014 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdlib.h>
#include <string.h>
#include "db_compiler.h"

/* statement handler prototypes */
static int ParseDef(ParseContext *c);
static void ParseFunctionDef(ParseContext *c, char *name);
static void FinishFunctionDef(ParseContext *c);
static void ParseVar(ParseContext *c);
static int ParseVariableDecl(ParseContext *c, char *name, VMVALUE *pSize);
static VMVALUE ParseScalarInitializer(ParseContext *c);
static void ParseArrayInitializers(ParseContext *c, VMVALUE *pSize);
static void ClearArrayInitializers(ParseContext *c, VMVALUE size);
static void ParseConstantDef(ParseContext *c, char *name);
static void ParseIf(ParseContext *c);
static void CheckForElse(ParseContext *c);
static void FinishElse(ParseContext *c);
static void ParseWhile(ParseContext *c);
static void FinishWhile(ParseContext *c);
static void ParseDo(ParseContext *c);
static void FinishDoWhile(ParseContext *c);
static void ParseFor(ParseContext *c);
static void FinishFor(ParseContext *c);
static void ParseBreakOrContinue(ParseContext *c, int isBreak);
static void ParseGoto(ParseContext *c);
static void ParseReturn(ParseContext *c);
static void ParsePrint(ParseContext *c);
#ifdef USE_ASM
static void ParseAsm(ParseContext *c);
#endif

/* prototypes */
static int ParseStatement1(ParseContext *c, int tkn);
static void CallHandler(ParseContext *c, int trap, ParseTreeNode *expr);
static void DefineLabel(ParseContext *c, char *name, int offset);
static int ReferenceLabel(ParseContext *c, char *name, int offset);
static void PushBlock(ParseContext *c, BlockType type);
static void PopBlock(ParseContext *c);

/* ParseStatement - parse a statement */
void ParseStatement(ParseContext *c, int tkn)
{
    if (ParseStatement1(c, tkn)) {
        switch (CurrentBlockType(c)) {
        case BLOCK_IF:
            CheckForElse(c);
            break;
        case BLOCK_ELSE:
            FinishElse(c);
            break;
        case BLOCK_FOR:
            FinishFor(c);
            break;
        case BLOCK_WHILE:
            FinishWhile(c);
            break;
        case BLOCK_DO:
            FinishDoWhile(c);
            break;
        case BLOCK_DEF:
        case BLOCK_BLOCK:
        case BLOCK_NONE:
            break;
        }
    }
}

/* ParseStatement1 - parse a statement or fragment */
static int ParseStatement1(ParseContext *c, int tkn)
{
    int complete = VMTRUE;
    
    /* dispatch on the statement keyword */
    switch (tkn) {
    case T_DEF:
        complete = ParseDef(c);
        break;
    case T_VAR:
        ParseVar(c);
        break;
    case T_IF:
        ParseIf(c);
        complete = VMFALSE;
        break;
    case T_ELSE:
        ParseError(c, "'else' without a matching 'if'");
    case T_WHILE:
        ParseWhile(c);
        complete = VMFALSE;
        break;
    case T_DO:
        ParseDo(c);
        complete = VMFALSE;
        break;
    case T_FOR:
        ParseFor(c);
        complete = VMFALSE;
        break;
    case T_BREAK:
        ParseBreakOrContinue(c, VMTRUE);
        break;
    case T_CONTINUE:
        ParseBreakOrContinue(c, VMFALSE);
        break;
    case T_GOTO:
        ParseGoto(c);
        break;
    case T_RETURN:
        ParseReturn(c);
        break;
    case '{':
        PushBlock(c, BLOCK_BLOCK);
        complete = VMFALSE;
        break;
    case '}':
        if (CurrentBlockType(c) == BLOCK_DEF)
            FinishFunctionDef(c);
        else
            PopBlock(c);
        break;
    case ';':
        break;
    case T_PRINT:
        ParsePrint(c);
        break;
#ifdef USE_ASM
    case T_ASM:
        ParseAsm(c);
        break;
#endif
    case T_IDENTIFIER:
        if (SkipSpaces(c) == ':') {
            DefineLabel(c, c->token, codeaddr(c));
            break;
        }
        UngetC(c);
        /* fall through */
    default:
        SaveToken(c, tkn);
        ParseRValue(c);
        putcbyte(c, OP_DROP);
        FRequire(c, ';');
        break;
    }
    
    return complete;
}

/* ParseDef - parse the 'DEF' statement */
static int ParseDef(ParseContext *c)
{
    int complete = VMTRUE;
    char name[MAXTOKEN];
    int tkn;

    /* get the name being defined */
    FRequire(c, T_IDENTIFIER);
    strcpy(name, c->token);

    /* check for a constant definition */
    if ((tkn = GetToken(c)) == '=')
        ParseConstantDef(c, name);

    /* otherwise, assume a function definition */
    else {
        Require(c, tkn, '(');
        ParseFunctionDef(c, name);
        complete = VMFALSE;
    }
    
    return complete;
}

/* ParseConstantDef - parse a 'DEF <name> =' statement */
static void ParseConstantDef(ParseContext *c, char *name)
{
    ParseTreeNode *expr;

    /* get the constant value */
    expr = ParseExpr(c);

    /* make sure it's a constant */
    if (!IsIntegerLit(expr))
        ParseError(c, "expecting a constant expression");

    /* add the symbol as a global */
    AddGlobal(c, name, SC_CONSTANT, expr->u.integerLit.value);

    FRequire(c, ';');
}

/* ParseFunctionDef - parse a 'DEF <name> () {}' statement */
static void ParseFunctionDef(ParseContext *c, char *name)
{
    int tkn;
    
    /* enter a function definition block */
    PushBlock(c, BLOCK_DEF);

    /* enter the function name in the global symbol table */
    c->codeSymbol = AddGlobal(c, name, SC_VARIABLE, 0);

    /* start the code under construction */
    StartCode(c, CODE_TYPE_FUNCTION);

    /* get the argument list */
    if ((tkn = GetToken(c)) != ')') {
        int offset = 0;
        SaveToken(c, tkn);
        do {
            FRequire(c, T_IDENTIFIER);
            AddArgument(c, c->token, SC_VARIABLE, offset);
            ++offset;
        } while ((tkn = GetToken(c)) == ',');
    }
    Require(c, tkn, ')');
    FRequire(c, '{');
}

/* FinishFunctionDef - finish a 'def <name> () {}' statement */
static void FinishFunctionDef(ParseContext *c)
{
    if (c->codeType != CODE_TYPE_FUNCTION)
        ParseError(c, "not in a function definition");
    c->codeSymbol->value = StoreCode(c);
    c->codeSymbol = NULL;
    PopBlock(c);
}

/* ParseVar - parse the 'var' statement */
static void ParseVar(ParseContext *c)
{
    char name[MAXTOKEN];
    VMVALUE value, size = 0;
    int isArray;
    int tkn;

    /* parse variable declarations */
    do {

        /* get variable name */
        isArray = ParseVariableDecl(c, name, &size);

        /* add to the global symbol table if outside a function definition */
        if (c->codeType == CODE_TYPE_MAIN) {
            VMVALUE *data, *dataPtr;

            data = dataPtr = (VMVALUE *)c->image->codeBuf;
            
            /* check for initializers */
            if ((tkn = GetToken(c)) == '=') {
                if (isArray)
                    ParseArrayInitializers(c, &size);
                else {
                    dataPtr = (VMVALUE *)((long)dataPtr & ~3);
                    if (dataPtr >= (VMVALUE *)c->image->heapFree)
                        ParseError(c, "insufficient image space");
                    *dataPtr = ParseScalarInitializer(c);
                }
            }

            /* no initializers */
            else {
                ClearArrayInitializers(c, isArray ? size : 1);
                SaveToken(c, tkn);
            }

            /* allocate space for the data */
            value = (VMVALUE)data;
            c->image->codeBuf = c->image->codeFree = (uint8_t *)(data + size);
            
            /* add the symbol to the global symbol table */
            AddGlobal(c, name, SC_VARIABLE, value);
        }

        /* otherwise, add to the local symbol table */
        else {
            if (isArray)
                ParseError(c, "local arrays are not supported");
            AddLocal(c, name, SC_VARIABLE, c->localOffset);
            ++c->localOffset;
        }

    } while ((tkn = GetToken(c)) == ',');

    Require(c, tkn, ';');
}

/* ParseVariableDecl - parse a variable declaration */
static int ParseVariableDecl(ParseContext *c, char *name, VMVALUE *pSize)
{
    int isArray;
    int tkn;

    /* parse the variable name */
    FRequire(c, T_IDENTIFIER);
    strcpy(name, c->token);

    /* handle arrays */
    if ((tkn = GetToken(c)) == '[') {

        /* check for an array with unspecified size */
        if ((tkn = GetToken(c)) == ']')
            *pSize = 0;

        /* otherwise, parse the array size */
        else {
            ParseTreeNode *expr;

            /* put back the token */
            SaveToken(c, tkn);

            /* get the array size */
            expr = ParseExpr(c);

            /* make sure it's a constant */
            if (!IsIntegerLit(expr) || expr->u.integerLit.value <= 0)
                ParseError(c, "expecting a positive constant expression");
            *pSize = expr->u.integerLit.value;

            /* only one dimension allowed for now */
            FRequire(c, ']');
        }

        /* return an array and its size */
        isArray = VMTRUE;
        return VMTRUE;
    }

    /* not an array */
    else {
        SaveToken(c, tkn);
        isArray = VMFALSE;
        *pSize = 1;
    }

    /* return array indicator */
    return isArray;
}

/* ParseScalarInitializer - parse a scalar initializer */
static VMVALUE ParseScalarInitializer(ParseContext *c)
{
    ParseTreeNode *expr = ParseExpr(c);
    if (!IsIntegerLit(expr))
        ParseError(c, "expecting a constant expression");
    return expr->u.integerLit.value;
}

/* ParseArrayInitializers - parse array initializers */
static void ParseArrayInitializers(ParseContext *c, VMVALUE *pSize)
{
    VMVALUE *dataPtr = (VMVALUE *)c->image->codeBuf;
    VMVALUE *dataTop = (VMVALUE *)c->image->heapFree;
    int tkn, actualSize = 0;
    VMVALUE value;
    
    FRequire(c, '{');
    
    if ((tkn = GetToken(c)) != '}') {
        SaveToken(c, tkn);
        do {
            /* count another initializer */
            ++actualSize;
            
            /* check for too many initializers */
            if (*pSize > 0 && actualSize > *pSize)
                ParseError(c, "too many initializers");

            /* get a constant expression */
            value = ParseScalarInitializer(c);

            /* store the initial value */
            if (dataPtr >= dataTop)
                ParseError(c, "insufficient image space");
            *dataPtr++ = value;
            
        } while ((tkn = GetToken(c)) == ',');
    }
    
    if (*pSize == 0)
        *pSize = actualSize;
}

/* ClearArrayInitializers - clear the array initializers */
static void ClearArrayInitializers(ParseContext *c, VMVALUE size)
{
    VMVALUE *dataPtr = (VMVALUE *)c->image->codeBuf;
    VMVALUE *dataTop = (VMVALUE *)c->image->heapFree;
    if (dataPtr + size > dataTop)
        ParseError(c, "insufficient image space");
    memset(dataPtr, 0, size * sizeof(VMVALUE));
}

/* ParseIf - parse the 'if' statement */
static void ParseIf(ParseContext *c)
{
    FRequire(c, '(');
    ParseRValue(c);
    FRequire(c, ')');
    PushBlock(c, BLOCK_IF);
    putcbyte(c, OP_BRF);
    c->bptr->u.IfBlock.nxt = putcword(c, 0);
    c->bptr->u.IfBlock.end = 0;
}

/* CheckForElse - check for an 'else' clause */
void CheckForElse(ParseContext *c)
{
    int tkn;
    if ((tkn = GetToken(c)) == T_ELSE) {
        int end;
        putcbyte(c, OP_BR);
        end = putcword(c, c->bptr->u.IfBlock.end);
        fixupbranch(c, c->bptr->u.IfBlock.nxt, codeaddr(c));
        c->bptr->type = BLOCK_ELSE;
        c->bptr->u.ElseBlock.end = end;
    }
    else {
        SaveToken(c, tkn);
        fixupbranch(c, c->bptr->u.IfBlock.nxt, codeaddr(c));
        fixupbranch(c, c->bptr->u.IfBlock.end, codeaddr(c));
        PopBlock(c);
    }
}

/* FinishElse - finish an 'else' clause */
void FinishElse(ParseContext *c)
{
    fixupbranch(c, c->bptr->u.ElseBlock.end, codeaddr(c));
    PopBlock(c);
}

/* ParseWhile - parse the 'while' statement */
static void ParseWhile(ParseContext *c)
{
    PushBlock(c, BLOCK_WHILE);
    c->bptr->u.LoopBlock.cont = c->bptr->u.LoopBlock.nxt = codeaddr(c);
    c->bptr->u.LoopBlock.contDefined = VMTRUE;
    FRequire(c, '(');
    ParseRValue(c);
    FRequire(c, ')');
    putcbyte(c, OP_BRF);
    c->bptr->u.LoopBlock.end = putcword(c, 0);
}

/* FinishWhile - finish a 'while' statement */
void FinishWhile(ParseContext *c)
{
    int inst = putcbyte(c, OP_BR);
    putcword(c, c->bptr->u.LoopBlock.nxt - inst - 1 - sizeof(VMWORD));
    fixupbranch(c, c->bptr->u.LoopBlock.end, codeaddr(c));
    PopBlock(c);
}

/* ParseDo - parse the 'do/while' statement */
static void ParseDo(ParseContext *c)
{
    PushBlock(c, BLOCK_DO);
    c->bptr->u.LoopBlock.cont = 0;
    c->bptr->u.LoopBlock.contDefined = VMFALSE;
    c->bptr->u.LoopBlock.nxt = codeaddr(c);
    c->bptr->u.LoopBlock.end = 0;
}

/* FinishDoWhile - finish a 'do/while' statement */
void FinishDoWhile(ParseContext *c)
{
    int inst;
    fixupbranch(c, c->bptr->u.LoopBlock.cont, codeaddr(c));
    FRequire(c, T_WHILE);
    FRequire(c, '(');
    ParseRValue(c);
    FRequire(c, ')');
    inst = putcbyte(c, OP_BRT);
    putcword(c, c->bptr->u.LoopBlock.nxt - inst - 1 - sizeof(VMWORD));
    fixupbranch(c, c->bptr->u.LoopBlock.end, codeaddr(c));
    PopBlock(c);
    FRequire(c, ';');
}

/* ParseFor - parse the 'for' statement */
static void ParseFor(ParseContext *c)
{
    int tkn, nxt, body, inst, test;

    PushBlock(c, BLOCK_FOR);

    /* compile the initialization expression */
    FRequire(c, '(');
    if ((tkn = GetToken(c)) != ';') {
        SaveToken(c, tkn);
        ParseRValue(c);
        FRequire(c, ';');
        putcbyte(c, OP_DROP);
    }

    /* compile the test expression */
    nxt = codeaddr(c);
    if ((tkn = GetToken(c)) == ';')
        test = VMFALSE;
    else {
        SaveToken(c, tkn);
        ParseRValue(c);
        FRequire(c, ';');
        test = VMTRUE;
    }

    /* branch to the loop body if the expression is true */
    putcbyte(c, test ? OP_BRT : OP_BR);
    body = putcword(c, 0);

    /* branch to the end if the expression is false */
    if (test) {
        putcbyte(c, OP_BR);
        c->bptr->u.LoopBlock.end = putcword(c, 0);
    }

    /* compile the update expression */
    c->bptr->u.LoopBlock.cont = c->bptr->u.LoopBlock.nxt = codeaddr(c);
    c->bptr->u.LoopBlock.contDefined = VMTRUE;
    if ((tkn = GetToken(c)) != ')') {
        SaveToken(c, tkn);
        ParseRValue(c);
        putcbyte(c, OP_DROP);
        FRequire(c, ')');
    }

    /* branch back to the test code */
    inst = putcbyte(c, OP_BR);
    putcword(c, nxt - inst - 1 - sizeof(VMWORD));

    /* compile the loop body */
    fixupbranch(c, body, codeaddr(c));
}

/* FinishFor - finish a 'for' statement */
void FinishFor(ParseContext *c)
{
    int inst;
    inst = putcbyte(c, OP_BR);
    putcword(c, c->bptr->u.LoopBlock.nxt - inst - 1 - sizeof(VMWORD));
    fixupbranch(c, c->bptr->u.LoopBlock.end, codeaddr(c));
    PopBlock(c);
}

/* ParseBreakOrContinue - parse a 'break' or 'continue' statement */
static void ParseBreakOrContinue(ParseContext *c, int isBreak)
{
    Block *block = c->bptr;
    int inst;
    for (block = c->bptr; block >= c->blockBuf; --block) {
        switch (block->type) {
        case BLOCK_FOR:
        case BLOCK_WHILE:
        case BLOCK_DO:
            inst = putcbyte(c, OP_BR);
            if (isBreak)
                block->u.LoopBlock.end = putcword(c, block->u.LoopBlock.end);
            else {
                if (block->u.LoopBlock.contDefined)
                    putcword(c, block->u.LoopBlock.cont - inst - 1 - sizeof(VMWORD));
                else
                    block->u.LoopBlock.cont = putcword(c, block->u.LoopBlock.cont);
            }
            FRequire(c, ';');
            return;
        default:
            break;
        }
    }
    ParseError(c, "'continue' not allowed outside of a loop");
}

/* ParseGoto - parse the 'GOTO' statement */
static void ParseGoto(ParseContext *c)
{
    FRequire(c, T_IDENTIFIER);
    putcbyte(c, OP_BR);
    putcword(c, ReferenceLabel(c, c->token, codeaddr(c)));
    FRequire(c, ';');
}

/* ParseReturn - parse the 'RETURN' statement */
static void ParseReturn(ParseContext *c)
{
    int tkn;
    if ((tkn = GetToken(c)) == ';') {
        putcbyte(c, OP_SLIT);
        putcbyte(c, 0);
    }
    else {
        SaveToken(c, tkn);
        ParseRValue(c);
        FRequire(c, ';');
    }
    putcbyte(c, OP_RETURN);
}

/* ParsePrint - handle the 'PRINT' statement */
static void ParsePrint(ParseContext *c)
{
    int needNewline = VMTRUE;
    ParseTreeNode *expr;
    int tkn;

    while ((tkn = GetToken(c)) != ';') {
        switch (tkn) {
        case ',':
            needNewline = VMFALSE;
            CallHandler(c, TRAP_PrintTab, NULL);
            break;
        case '$':
            needNewline = VMFALSE;
            break;
        default:
            needNewline = VMTRUE;
            SaveToken(c, tkn);
            expr = ParseExpr(c);
            switch (expr->nodeType) {
            case NodeTypeStringLit:
                CallHandler(c, TRAP_PrintStr, expr);
                break;
            default:
                CallHandler(c, TRAP_PrintInt, expr);
                break;
            }
            break;
        }
    }

    if (needNewline)
        CallHandler(c, TRAP_PrintNL, NULL);
    else
        CallHandler(c, TRAP_PrintFlush, NULL);
}

#ifdef USE_ASM

#include "db_vmdebug.h"

static void Assemble(ParseContext *c, char *name);
static VMVALUE ParseIntegerConstant(ParseContext *c);

/* ParseAsm - parse the 'ASM ... END ASM' statement */
static void ParseAsm(ParseContext *c)
{
    int tkn;
    
    /* check for the end of the 'ASM' statement */
    FRequire(c, ';');
    
    /* parse each assembly instruction */
    for (;;) {
    
        /* get the next line */
        if (!GetLine(c->sys))
            ParseError(c, "unexpected end of file in ASM statement");
        
        /* check for the end of the assembly instructions */
        if ((tkn = GetToken(c)) == T_END_ASM)
            break;
            
        /* check for an empty statement */
        else if (tkn == ';')
            continue;
            
        /* check for an opcode name */
        else if (tkn != T_IDENTIFIER)
            ParseError(c, "expected an assembly instruction opcode");
            
        /* assemble a single instruction */
        Assemble(c, c->token);
    }
    
    /* check for the end of the 'END ASM' statement */
    FRequire(c, ';');
}

/* Assemble - assemble a single line */
static void Assemble(ParseContext *c, char *name)
{
    OTDEF *def;
    
    /* lookup the opcode */
    for (def = OpcodeTable; def->name != NULL; ++def)
        if (strcmp(name, def->name) == 0) {
            putcbyte(c, def->code);
            switch (def->fmt) {
            case FMT_NONE:
                break;
            case FMT_BYTE:
            case FMT_SBYTE:
                putcbyte(c, ParseIntegerConstant(c));
                break;
            case FMT_LONG:
                putcword(c, ParseIntegerConstant(c));
                break;
            default:
                ParseError(c, "instruction not currently supported");
                break;
            }
            FRequire(c, ';');
            return;
        }
        
    ParseError(c, "undefined opcode");
}

/* ParseIntegerConstant - parse an integer constant expression */
static VMVALUE ParseIntegerConstant(ParseContext *c)
{
    ParseTreeNode *expr;
    expr = ParseExpr(c);
    if (!IsIntegerLit(expr))
        ParseError(c, "expecting an integer constant expression");
    return expr->u.integerLit.value;
}

#endif

/* CallHandler - compile a call to a runtime print function */
static void CallHandler(ParseContext *c, int trap, ParseTreeNode *expr)
{
    /* compile the argument */
    if (expr)
        code_rvalue(c, expr);
    
    /* compile the function symbol reference */
    putcbyte(c, OP_TRAP);
    putcbyte(c, trap);
}

/* DefineLabel - define a local label */
static void DefineLabel(ParseContext *c, char *name, int offset)
{
    Label *label;

    /* check to see if the label is already in the table */
    for (label = c->labels; label != NULL; label = label->next)
        if (strcmp(name, label->name) == 0) {
            if (!label->fixups)
                ParseError(c, "duplicate label: %s", name);
            else {
                fixupbranch(c, label->fixups, offset);
                label->offset = offset;
                label->fixups = 0;
            }
            return;
        }

    /* allocate the label structure */
    label = (Label *)LocalAlloc(c, sizeof(Label) + strlen(name));
    memset(label, 0, sizeof(Label));
    strcpy(label->name, name);
    label->offset = offset;
    label->next = c->labels;
    c->labels = label;
}

/* ReferenceLabel - add a reference to a local label */
static int ReferenceLabel(ParseContext *c, char *name, int offset)
{
    Label *label;

    /* check to see if the label is already in the table */
    for (label = c->labels; label != NULL; label = label->next)
        if (strcmp(name, label->name) == 0) {
            int link;
            if (!(link = label->fixups))
                return label->offset - offset - sizeof(VMWORD);
            else {
                label->fixups = offset;
                return link;
            }
        }

    /* allocate the label structure */
    label = (Label *)LocalAlloc(c, sizeof(Label) + strlen(name));
    memset(label, 0, sizeof(Label));
    strcpy(label->name, name);
    label->fixups = offset;
    label->next = c->labels;
    c->labels = label;

    /* return zero to terminate the fixup list */
    return 0;
}

/* CheckLabels - check for undefined labels */
void CheckLabels(ParseContext *c)
{
    Label *label, *next;
    for (label = c->labels; label != NULL; label = next) {
        next = label->next;
        if (label->fixups)
            Abort(c->sys, "undefined label: %s", label->name);
    }
    c->labels = NULL;
}

/* CurrentBlockType - make sure there is a block on the stack */
BlockType  CurrentBlockType(ParseContext *c)
{
    return c->bptr < c->blockBuf ? BLOCK_NONE : c->bptr->type;
}

/* PushBlock - push a block on the block stack */
static void PushBlock(ParseContext *c, BlockType type)
{
    if (++c->bptr >= c->btop)
        Abort(c->sys, "statements too deeply nested");
    c->bptr->type = type;
}

/* PopBlock - pop a block off the block stack */
static void PopBlock(ParseContext *c)
{
    --c->bptr;
}
