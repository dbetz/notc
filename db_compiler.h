/* db_compiler.h - definitions for a simple basic compiler
 *
 * Copyright (c) 2014 by David Michael Betz.  All rights reserved.
 *
 */

#ifndef __DB_COMPILER_H__
#define __DB_COMPILER_H__

#include <stdio.h>
#include <setjmp.h>
#include "db_types.h"
#include "db_image.h"
#include "db_symbols.h"
#include "db_system.h"

/* program limits */
#define MAXTOKEN        32

/* forward type declarations */
typedef struct ParseTreeNode ParseTreeNode;
typedef struct ExprListEntry ExprListEntry;

/* lexical tokens */
enum {
    T_NONE,
    T_DEF = 0x100,  /* keywords start here */
    T_VAR,
    T_IF,
    T_ELSE,
    T_FOR,
    T_DO,
    T_WHILE,
    T_GOTO,
    T_RETURN,
    T_PRINT,
#ifdef USE_ASM
    T_ASM,
#endif
#ifdef USE_ASM
    T_END_ASM,
#endif
    T_LE,       /* non-keyword tokens */
    T_EQ,
    T_NE,
    T_GE,
    T_SHL,
    T_SHR,
    T_AND,
    T_OR,
    T_IDENTIFIER,
    T_NUMBER,
    T_STRING,
    T_EOL,
    T_EOF
};

/* block type */
typedef enum {
    BLOCK_NONE,
    BLOCK_DEF,
    BLOCK_IF,
    BLOCK_ELSE,
    BLOCK_FOR,
    BLOCK_WHILE,
    BLOCK_DO,
    BLOCK_BLOCK
} BlockType;

/* block structure */
typedef struct Block Block;
struct Block {
    BlockType type;
    union {
        struct {
            int nxt;
            int end;
        } IfBlock;
        struct {
            int end;
        } ElseBlock;
        struct {
            int nxt;
            int end;
        } ForBlock;
        struct {
            int nxt;
            int end;
        } DoBlock;
    } u;
};

/* label structure */
typedef struct Label Label;
struct Label {
    Label *next;
    int placed;
    int fixups;
    int offset;
    char name[1];
};

/* code types */
typedef enum {
    CODE_TYPE_MAIN,
    CODE_TYPE_FUNCTION
} CodeType;

/* parse context */
typedef struct {
    System *sys;                    /* system context */
    ImageHdr *image;                /* header of image being constructed */
    uint8_t *heapBase;              /* code staging buffer (start of heap) */
    uint8_t *heapFree;              /* next free heap location */
    uint8_t *heapTop;               /* top of heap */
    int lineNumber;                 /* scan - current line number */
    int savedToken;                 /* scan - lookahead token */
    int tokenOffset;                /* scan - offset to the start of the current token */
    char token[MAXTOKEN];           /* scan - current token string */
    VMVALUE value;                  /* scan - current token integer value */
    int inComment;                  /* scan - inside of a slash/star comment */
    Label *labels;                  /* parse - local labels */
    CodeType codeType;              /* parse - type of code under construction */
    Symbol *codeSymbol;             /* parse - symbol table entry of code under construction */
    SymbolTable arguments;          /* parse - arguments of current function definition */
    SymbolTable locals;             /* parse - local variables of current function definition */
    int localOffset;                /* parse - offset to next available local variable */
    Block blockBuf[10];             /* parse - stack of nested blocks */
    Block *bptr;                    /* parse - current block */
    Block *btop;                    /* parse - top of block stack */
} ParseContext;

/* partial value function codes */
typedef enum {
    PV_ADDR,
    PV_LOAD,
    PV_STORE
} PValOp;

/* partial value structure */
typedef struct PVAL PVAL;
struct PVAL {
    void (*fcn)(ParseContext *c, PValOp op, PVAL *pv);
    union {
        Symbol *sym;
        String *str;
        VMVALUE val;
    } u;
};

/* parse tree node types */
enum {
    NodeTypeSymbolRef,
    NodeTypeStringLit,
    NodeTypeIntegerLit,
    NodeTypeFunctionLit,
    NodeTypeUnaryOp,
    NodeTypeBinaryOp,
    NodeTypeArrayRef,
    NodeTypeFunctionCall,
    NodeTypeDisjunction,
    NodeTypeConjunction
};

/* parse tree node structure */
struct ParseTreeNode {
    int nodeType;
    union {
        struct {
            Symbol *symbol;
            void (*fcn)(ParseContext *c, PValOp op, PVAL *pv);
            int offset;
        } symbolRef;
        struct {
            String *string;
        } stringLit;
        struct {
            VMVALUE value;
        } integerLit;
        struct {
            int offset;
        } functionLit;
        struct {
            int op;
            ParseTreeNode *expr;
        } unaryOp;
        struct {
            int op;
            ParseTreeNode *left;
            ParseTreeNode *right;
        } binaryOp;
        struct {
            ParseTreeNode *array;
            ParseTreeNode *index;
        } arrayRef;
        struct {
            ParseTreeNode *fcn;
            ExprListEntry *args;
            int argc;
        } functionCall;
        struct {
            ExprListEntry *exprs;
        } exprList;
    } u;
};

/* expression list entry structure */
struct ExprListEntry {
    ParseTreeNode *expr;
    ExprListEntry *next;
};

/* db_compiler.c */
VMVALUE Compile(System *sys, ImageHdr *image);
void EnterBuiltInSymbols(ParseContext *c);
void InitCodeBuffer(ParseContext *c);
void StartCode(ParseContext *c, CodeType type);
VMVALUE StoreCode(ParseContext *c);
void AddIntrinsic(ParseContext *c, char *name, int index);
String *AddString(ParseContext *c, char *value);
VMVALUE AddStringRef(String *str, int offset);
void *LocalAlloc(ParseContext *c, size_t size);
void Fatal(ParseContext *c, char *fmt, ...);

/* db_statement.c */
void ParseStatement(ParseContext *c, int tkn);
BlockType CurrentBlockType(ParseContext *c);
void CheckLabels(ParseContext *c);

/* db_expr.c */
void ParseRValue(ParseContext *c);
ParseTreeNode *ParseExpr(ParseContext *c);
ParseTreeNode *ParsePrimary(ParseContext *c);
ParseTreeNode *GetSymbolRef(ParseContext *c, char *name);
int IsIntegerLit(ParseTreeNode *node);

/* db_scan.c */
void FRequire(ParseContext *c, int requiredToken);
void Require(ParseContext *c, int token, int requiredToken);
int GetToken(ParseContext *c);
void SaveToken(ParseContext *c, int token);
char *TokenName(int token);
int SkipSpaces(ParseContext *c);
int GetChar(ParseContext *c);
void UngetC(ParseContext *c);
void ParseError(ParseContext *c, char *fmt, ...);

/* db_symbols.c */
void InitSymbolTable(SymbolTable *table);
Symbol *AddGlobal(ParseContext *c, const char *name, StorageClass storageClass, VMVALUE value);
Symbol *AddArgument(ParseContext *c, const char *name, StorageClass storageClass, int value);
Symbol *AddLocal(ParseContext *c, const char *name, StorageClass storageClass, int value);
Symbol *FindSymbol(SymbolTable *table, const char *name);
int IsConstant(Symbol *symbol);
void DumpSymbols(SymbolTable *table, char *tag);

/* db_generate.c */
void code_lvalue(ParseContext *c, ParseTreeNode *expr, PVAL *pv);
void code_rvalue(ParseContext *c, ParseTreeNode *expr);
void rvalue(ParseContext *c, PVAL *pv);
void chklvalue(ParseContext *c, PVAL *pv);
void code_global(ParseContext *c, PValOp fcn, PVAL *pv);
void code_local(ParseContext *c, PValOp fcn, PVAL *pv);
int codeaddr(ParseContext *c);
int putcbyte(ParseContext *c, int v);
int putcword(ParseContext *c, VMWORD v);
int putclong(ParseContext *c, VMVALUE v);
void fixup(ParseContext *c, VMUVALUE chn, VMUVALUE val);
void fixupbranch(ParseContext *c, VMUVALUE chn, VMUVALUE val);

#endif

