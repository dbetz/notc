/* db_scan.c - token scanner
 *
 * Copyright (c) 2014 by David Michael Betz.  All rights reserved.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>
#include <ctype.h>
#include "db_compiler.h"

/* keyword table */
static struct {
    char *keyword;
    int token;
} ktab[] = {

/* these must be in the same order as the int enum */
{   "def",      T_DEF       },
{   "var",      T_VAR       },
{   "if",       T_IF        },
{   "else",     T_ELSE      },
{   "for",      T_FOR       },
{   "do",       T_DO        },
{   "while",    T_WHILE     },
{   "goto",     T_GOTO      },
{   "return",   T_RETURN    },
{   "print",    T_PRINT     },
#ifdef USE_ASM
{   "asm",      T_ASM       },
#endif
{   NULL,       0           }
};

/* local function prototypes */
static int NextToken(ParseContext *c);
static int IdentifierToken(ParseContext *c, int ch);
static int IdentifierCharP(int ch);
static int NumberToken(ParseContext *c, int ch);
static int HexNumberToken(ParseContext *c);
static int BinaryNumberToken(ParseContext *c);
static int StringToken(ParseContext *c);
static int CharToken(ParseContext *c);
static int LiteralChar(ParseContext *c);
static int SkipComment(ParseContext *c);
static int XGetC(ParseContext *c);

/* FRequire - fetch a token and check it */
void FRequire(ParseContext *c, int requiredToken)
{
    Require(c, GetToken(c), requiredToken);
}

/* Require - check for a required token */
void Require(ParseContext *c, int token, int requiredToken)
{
    char tknbuf[MAXTOKEN];
    if (token != requiredToken) {
        strcpy(tknbuf, TokenName(requiredToken));
        ParseError(c, "Expecting '%s', found '%s'", tknbuf, TokenName(token));
    }
}

/* GetToken - get the next token */
int GetToken(ParseContext *c)
{
    int tkn;

    /* check for a saved token */
    if ((tkn = c->savedToken) != T_NONE)
        c->savedToken = T_NONE;

    /* otherwise, get the next token */
    else
        tkn = NextToken(c);

    /* return the token */
    return tkn;
}

/* SaveToken - save a token */
void SaveToken(ParseContext *c, int token)
{
    c->savedToken = token;
}

/* TokenName - get the name of a token */
char *TokenName(int token)
{
    static char nameBuf[4];
    char *name;

    switch (token) {
    case T_NONE:
        name = "<NONE>";
        break;
    case T_DEF:
    case T_VAR:
    case T_IF:
    case T_ELSE:
    case T_FOR:
    case T_DO:
    case T_WHILE:
    case T_GOTO:
    case T_RETURN:
    case T_PRINT:
#ifdef USE_ASM
    case T_ASM:
#endif
        name = ktab[token - T_DEF].keyword;
        break;
#ifdef USE_ASM
    case T_END_ASM:
        name = "END ASM";
        break;
#endif
    case T_LE:
        name = "<=";
        break;
    case T_EQ:
        name = "==";
        break;
    case T_NE:
        name = "!=";
        break;
    case T_GE:
        name = ">=";
        break;
    case T_SHL:
        name = "<<";
        break;
    case T_SHR:
        name = ">>";
        break;
    case T_AND:
        name = "&&";
        break;
    case T_OR:
        name = "||";
        break;
    case T_IDENTIFIER:
        name = "<IDENTIFIER>";
        break;
    case T_NUMBER:
        name = "<NUMBER>";
        break;
    case T_STRING:
        name = "<STRING>";
        break;
    case T_EOL:
        name = "<EOL>";
        break;
    case T_EOF:
        name = "<EOF>";
        break;
    default:
        nameBuf[0] = '\'';
        nameBuf[1] = token;
        nameBuf[2] = '\'';
        nameBuf[3] = '\0';
        name = nameBuf;
        break;
    }

    /* return the token name */
    return name;
}

/* NextToken - read the next token */
static int NextToken(ParseContext *c)
{
    int ch, tkn;
    
    /* skip leading blanks */
    ch = SkipSpaces(c);

    /* remember the start of the current token */
    c->tokenOffset = (int)(c->sys->linePtr - c->sys->lineBuf);

    /* check the next character */
    switch (ch) {
    case EOF:
        tkn = T_EOL;
        break;
    case '"':
        tkn = StringToken(c);
        break;
    case '\'':
        tkn = CharToken(c);
        break;
    case '<':
        if ((ch = GetChar(c)) == '=')
            tkn = T_LE;
        else if (ch == '<')
            tkn = T_SHL;
        else {
            UngetC(c);
            tkn = '<';
        }
        break;
    case '=':
        if ((ch = GetChar(c)) == '=')
            tkn = T_EQ;
        else {
            UngetC(c);
            tkn = '=';
        }
        break;
    case '!':
        if ((ch = GetChar(c)) == '=')
            tkn = T_NE;
        else {
            UngetC(c);
            tkn = '!';
        }
        break;
    case '>':
        if ((ch = GetChar(c)) == '=')
            tkn = T_GE;
        else if (ch == '>')
            tkn = T_SHR;
        else {
            UngetC(c);
            tkn = '>';
        }
        break;
    case '&':
        if ((ch = GetChar(c)) == '&')
            tkn = T_AND;
        else {
            UngetC(c);
            tkn = '&';
        }
        break;
    case '|':
        if ((ch = GetChar(c)) == '|')
            tkn = T_AND;
        else {
            UngetC(c);
            tkn = '|';
        }
        break;
    case '0':
        switch (GetChar(c)) {
        case 'x':
        case 'X':
            tkn = HexNumberToken(c);
            break;
        case 'b':
        case 'B':
            tkn = BinaryNumberToken(c);
            break;
        default:
            UngetC(c);
            tkn = NumberToken(c, '0');
            break;
        }
        break;
    default:
        if (isdigit(ch))
            tkn = NumberToken(c, ch);
        else if (IdentifierCharP(ch))
            tkn = IdentifierToken(c,ch);
        else
            tkn = ch;
    }

    /* return the token */
    return tkn;
}

/* IdentifierToken - get an identifier */
static int IdentifierToken(ParseContext *c, int ch)
{
    int len, i;
    char *p;

    /* get the identifier */
    p = c->token; *p++ = ch; len = 1;
    while ((ch = GetChar(c)) != EOF && IdentifierCharP(ch)) {
        if (++len > MAXTOKEN)
            ParseError(c, "Identifier too long");
        *p++ = ch;
    }
    UngetC(c);
    *p = '\0';

    /* check to see if it is a keyword */
    for (i = 0; ktab[i].keyword != NULL; ++i)
        if (strcmp(ktab[i].keyword, c->token) == 0)
            return ktab[i].token;

    /* otherwise, it is an identifier */
    return T_IDENTIFIER;
}

/* IdentifierCharP - is this an identifier character? */
static int IdentifierCharP(int ch)
{
    return isupper(ch)
        || islower(ch)
        || isdigit(ch)
        || ch == '_';
}

/* NumberToken - get a number */
static int NumberToken(ParseContext *c, int ch)
{
    char *p = c->token;

    /* get the number */
    *p++ = ch;
    while ((ch = GetChar(c)) != EOF) {
        if (isdigit(ch))
            *p++ = ch;
        else if (ch != '_')
            break;
    }
    UngetC(c);
    *p = '\0';
    
    /* convert the string to an integer */
    c->value = (VMVALUE)atol(c->token);
    
    /* return the token */
    return T_NUMBER;
}

/* HexNumberToken - get a hexadecimal number */
static int HexNumberToken(ParseContext *c)
{
    char *p = c->token;
    int ch;

    /* get the number */
    while ((ch = GetChar(c)) != EOF) {
        if (isxdigit(ch))
            *p++ = ch;
        else if (ch != '_')
            break;
    }
    UngetC(c);
    *p = '\0';
    
    /* convert the string to an integer */
    c->value = (VMVALUE)strtoul(c->token, NULL, 16);
    
    /* return the token */
    return T_NUMBER;
}

/* BinaryNumberToken - get a binary number */
static int BinaryNumberToken(ParseContext *c)
{
    char *p = c->token;
    int ch;

    /* get the number */
    while ((ch = GetChar(c)) != EOF) {
        if (ch == '0' || ch == '1')
            *p++ = ch;
        else if (ch != '_')
            break;
    }
    UngetC(c);
    *p = '\0';
    
    /* convert the string to an integer */
    c->value = (VMVALUE)strtoul(c->token, NULL, 2);
    
    /* return the token */
    return T_NUMBER;
}

/* StringToken - get a string */
static int StringToken(ParseContext *c)
{
    int ch,len;
    char *p;

    /* collect the string */
    p = c->token; len = 0;
    while ((ch = XGetC(c)) != EOF && ch != '"') {
        if (++len > MAXTOKEN)
            ParseError(c, "String too long");
        *p++ = (ch == '\\' ? LiteralChar(c) : ch);
    }
    *p = '\0';

    /* check for premature end of file */
    if (ch != '"')
        ParseError(c, "unterminated string");

    /* return the token */
    return T_STRING;
}

/* CharToken - get a character constant */
static int CharToken(ParseContext *c)
{
    int ch = LiteralChar(c);
    if (XGetC(c) != '\'')
        ParseError(c,"Expecting a closing single quote");
    c->token[0] = ch;
    c->token[1] = '\0';
    c->value = ch;
    return T_NUMBER;
}

/* LiteralChar - get a character from a literal string */
static int LiteralChar(ParseContext *c)
{
    int ch;
    switch (ch = XGetC(c)) {
    case 'n': 
        ch = '\n';
        break;
    case 'r':
        ch = '\r';
        break;
    case 't':
        ch = '\t';
        break;
    case EOF:
        ch = '\\';
        break;
    }
    return ch;
}

/* SkipSpaces - skip leading spaces and the the next non-blank character */
int SkipSpaces(ParseContext *c)
{
    int ch;
    while ((ch = GetChar(c)) != EOF)
        if (!isspace(ch))
            break;
    return ch;
}

/* SkipComment - skip characters up to the end of a comment */
static int SkipComment(ParseContext *c)
{
    int lastch, ch;
    lastch = '\0';
    while ((ch = XGetC(c)) != EOF) {
        if (lastch == '*' && ch == '/')
            return VMTRUE;
        lastch = ch;
    }
    return VMFALSE;
}

/* GetChar - get the next character */
int GetChar(ParseContext *c)
{
    int ch;

    /* check for being in a comment */
    if (c->inComment) {
        if (!SkipComment(c))
            return EOF;
        c->inComment = VMFALSE;
    }

    /* loop until we find a non-comment character */
    for (;;) {
        
        /* get the next character */
        if ((ch = XGetC(c)) == EOF)
            return EOF;

        /* check for a comment */
        else if (ch == '/') {
            if ((ch = XGetC(c)) == '/') {
                while ((ch = XGetC(c)) != EOF)
                    ;
            }
            else if (ch == '*') {
                if (!SkipComment(c)) {
                    c->inComment = VMTRUE;
                    return EOF;
                }
            }
            else {
                UngetC(c);
                ch = '/';
                break;
            }
        }

        /* handle a normal character */
        else
            break;
    }

    /* return the character */
    return ch;
}

/* XGetC - get the next character without checking for comments */
static int XGetC(ParseContext *c)
{
    int ch;
    
    /* get the next character on the current line */
    while (!(ch = *c->sys->linePtr++)) {
        if (!GetLine(c->sys))
            return EOF;
    }
    
    /* return the character */
    return ch;
}

/* UngetC - unget the most recent character */
void UngetC(ParseContext *c)
{
    /* backup the input pointer */
    --c->sys->linePtr;
}

/* ParseError - report a parsing error */
void ParseError(ParseContext *c, char *fmt, ...)
{
    va_list ap;

    /* print the error message */
    va_start(ap, fmt);
    VM_printf("error: ");
    VM_vprintf(fmt, ap);
    VM_putchar('\n');
    va_end(ap);

    /* show the context */
    VM_printf("  line %d\n", c->lineNumber);
    VM_printf("    %s\n", c->sys->lineBuf);
    VM_printf("    %*s\n", c->tokenOffset, "^");

    /* exit until we fix the compiler so it can recover from parse errors */
    longjmp(c->sys->errorTarget, 1);
}
