/* Copyright (c) 2007 by Ian Piumarta
 * Copyright (c) 2011 by Amos Wenger nddrylliog@gmail.com
 * Copyright (c) 2013 by perl11 org
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the 'Software'),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, provided that the above copyright notice(s) and this
 * permission notice appear in all copies of the Software.  Acknowledgement
 * of the use of this Software in supporting documentation would be
 * appreciated but is not required.
 * 
 * THE SOFTWARE IS PROVIDED 'AS IS'.  USE ENTIRELY AT YOUR OWN RISK.
 * 
 * Last edited: 2013-04-10 11:15:49 rurban
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "greg.h"

static int yyl(void)
{
  static int prev= 0;
  return ++prev;
}

static void charClassSet  (unsigned char bits[], int c)	{ bits[c >> 3] |=  (1 << (c & 7)); }
static void charClassClear(unsigned char bits[], int c)	{ bits[c >> 3] &= ~(1 << (c & 7)); }

typedef void (*setter)(unsigned char bits[], int c);

static int readChar(unsigned char **cp)
{
  unsigned char *cclass = *cp;
  int c= *cclass++, i = 0;
  if ('\\' == c && *cclass)
  {
    c= *cclass++;
    if (c >= '0' && c <= '9')
      {
        unsigned char oct= 0;
        for (i= 2; i >= 0; i--) {
          if (!(c >= '0' && c <= '9'))
            break;
          oct= (oct * 8) + (c - '0');
          c= *cclass++;
        }
        cclass--;
        c= oct;
        goto done;
      }

    switch (c)
      {
      case 'a':  c= '\a'; break;	/* bel */
      case 'b':  c= '\b'; break;	/* bs */
      case 'e':  c= '\e'; break;	/* esc */
      case 'f':  c= '\f'; break;	/* ff */
      case 'n':  c= '\n'; break;	/* nl */
      case 'r':  c= '\r'; break;	/* cr */
      case 't':  c= '\t'; break;	/* ht */
      case 'v':  c= '\v'; break;	/* vt */
      default:		break;
      }
  }

done:
  *cp = cclass;
  return c;
}

static char *yyqq(char* s) {
  char *d = s;
  char *dst;
  int sl = 0, dl = 0;
  while (*s++) {
    dl++; sl++;
    if (*s==7||*s==8||*s==9||*s==11||*s==12||*s==13||*s==27||*s==34||*s==92) { dl++; } // escape with '\'
    else if (*s==10) { dl += 3; }        // \n\^J
    else if (*s<32||*s>128) { dl += 4; } // octal \000
  }
  if (dl == sl) return d;
  s = d;
  dst = d = (char *)malloc(dl+1);
  while (*s) {
    if (*s == '"') {
      *d++ = '\\'; *d++ = *s++;
    } else if (*s == '\n') { // \n\^J
      *d++ = '\\'; *d++ = 'n'; *d++ = '\\'; *d++ = 10; s++;
    } else if (*s == '\t') { //ht
      *d++ = '\\'; *d++ = 't'; s++;
    } else if (*s == '\r') { //cr
      *d++ = '\\'; *d++ = 'r'; s++;
    } else if (*s == '\a') { //bel
      *d++ = '\\'; *d++ = 'a'; s++;
    } else if (*s == '\b') { //bs
      *d++ = '\\'; *d++ = 'b'; s++;
    } else if (*s == '\e') { //esc
      *d++ = '\\'; *d++ = 'e'; s++;
    } else if (*s == '\f') { //ff
      *d++ = '\\'; *d++ = 'f'; s++;
    } else if (*s == '\v') { //vt
      *d++ = '\\'; *d++ = 'v'; s++;
    } else if (*s == 92) { // '\'
      *d++ = '\\'; *d++ = '\\'; s++;
    } else if (*s<32||*s>128) {
      sprintf(d,"\\%03o", *s); // octal \000
      d += 4; s++;
    } else {
      *d++ = *s++;
    }
  }
  *d = 0;
  return dst;
}

static char *makeCharClass(unsigned char *cclass)
{
  unsigned char	 bits[32];
  setter	 set;
  int		 c, prev= -1;
  static char	 string[256];
  char		*ptr;

  if ('^' == *cclass)
    {
      memset(bits, 255, 32);
      set= charClassClear;
      ++cclass;
    }
  else
    {
      memset(bits, 0, 32);
      set= charClassSet;
    }
  while (0 != (c= readChar(&cclass)))
    {
      if ('-' == c && *cclass && prev >= 0)
	{
	  for (c= readChar(&cclass); prev <= c; ++prev)
	    set(bits, prev);
	  prev= -1;
	}
      else
  {
    set(bits, prev= c);
  }
    }

  ptr= string;
  for (c= 0;  c < 32;  ++c)
    ptr += sprintf(ptr, "\\%03o", bits[c]);

  return string;
}

static void begin(void)		{ fprintf(output, "\n  {"); }
static void end(void)		{ fprintf(output, "\n  }"); }
static void label(int n)	{ fprintf(output, "\n  l%d:;\n", n); }
static void jump(int n)		{ fprintf(output, "  goto l%d;", n); }
static void save(int n)		{ fprintf(output, "  int yypos%d= G->pos, yythunkpos%d= G->thunkpos;\n", n, n); }
static void restore(int n)	{ fprintf(output,     "  G->pos= yypos%d; G->thunkpos= yythunkpos%d;\n", n, n); }

static void callErrBlock(Node * node) {
    fprintf(output, " { YY_XTYPE YY_XVAR = (YY_XTYPE) G->data; int yyindex = G->offset + G->pos; %s; }", ((struct Any*) node)->errblock);
}

static void Node_compile_c_ko(Node *node, int ko)
{
  assert(node);
  switch (node->type)
    {
    case Rule:
      fprintf(stderr, "\ninternal error #1 (%s)\n", node->rule.name);
      exit(1);
      break;

    case Dot:
      fprintf(output, "  if (!yymatchDot(G)) goto l%d;\n", ko);
      break;

    case Name:
      fprintf(output, "  if (!yy_%s(G)) ", node->name.rule->rule.name);
      if(((struct Any*) node)->errblock) {
	fprintf(output, "{ ");
	callErrBlock(node);
	fprintf(output, " goto l%d; }\n", ko);
      } else {
	fprintf(output, " goto l%d;\n", ko);
      }
      if (node->name.variable)
	fprintf(output, "  yyDo(G, yySet, %d, 0, \"yySet\");\n", node->name.variable->variable.offset);
      break;

    case Character:
    case String:
      {
	int len= strlen(node->string.value);
	if (1 == len)
	  {
	    if ('\'' == node->string.value[0])
	      fprintf(output, "  if (!yymatchChar(G, '\\'')) goto l%d;\n", ko);
	    else
	      fprintf(output, "  if (!yymatchChar(G, '%s')) goto l%d;\n", node->string.value, ko);
	  }
	else
	  if (2 == len && '\\' == node->string.value[0])
	    fprintf(output, "  if (!yymatchChar(G, '%s')) goto l%d;\n", node->string.value, ko);
	  else
	    fprintf(output, "  if (!yymatchString(G, \"%s\")) goto l%d;\n", node->string.value, ko);
      }
      break;

    case Class:
      fprintf(output, "  if (!yymatchClass(G, (unsigned char *)\"%s\", \"%s\")) goto l%d;\n", makeCharClass(node->cclass.value), yyqq((char*)node->cclass.value), ko);
      break;

    case Action:
      fprintf(output, "  yyDo(G, yy%s, G->begin, G->end, \"yy%s\");\n", node->action.name, node->action.name);
      break;

    case Predicate:
      fprintf(output, "  yyText(G, G->begin, G->end);\n  if (!(%s)) goto l%d;\n", node->action.text, ko);
      break;

    case Alternate:
      {
	int ok= yyl();
	begin();
	save(ok);
	for (node= node->alternate.first;  node;  node= node->alternate.next)
	  if (node->alternate.next)
	    {
	      int next= yyl();
	      Node_compile_c_ko(node, next);
	      jump(ok);
	      label(next);
	      restore(ok);
	    }
	  else
	    Node_compile_c_ko(node, ko);
	end();
	label(ok);
      }
      break;

    case Sequence:
      for (node= node->sequence.first;  node;  node= node->sequence.next)
	Node_compile_c_ko(node, ko);
      break;

    case PeekFor:
      {
	int ok= yyl();
	begin();
	save(ok);
	Node_compile_c_ko(node->peekFor.element, ko);
	restore(ok);
	end();
      }
      break;

    case PeekNot:
      {
	int ok= yyl();
	begin();
	save(ok);
	Node_compile_c_ko(node->peekFor.element, ok);
	jump(ko);
	label(ok);
	restore(ok);
	end();
      }
      break;

    case Query:
      {
	int qko= yyl(), qok= yyl();
	begin();
	save(qko);
	Node_compile_c_ko(node->query.element, qko);
	jump(qok);
	label(qko);
	restore(qko);
	end();
	label(qok);
      }
      break;

    case Star:
      {
	int again= yyl(), out= yyl();
	label(again);
	begin();
	save(out);
	Node_compile_c_ko(node->star.element, out);
	jump(again);
	label(out);
	restore(out);
	end();
      }
      break;

    case Plus:
      {
	int again= yyl(), out= yyl();
	Node_compile_c_ko(node->plus.element, ko);
	label(again);
	begin();
	save(out);
	Node_compile_c_ko(node->plus.element, out);
	jump(again);
	label(out);
	restore(out);
	end();
      }
      break;

    default:
      fprintf(stderr, "\nNode_compile_c_ko: illegal node type %d\n", node->type);
      exit(1);
    }
}


static int countVariables(Node *node)
{
  int count= 0;
  while (node)
    {
      ++count;
      node= node->variable.next;
    }
  return count;
}

static void defineVariables(Node *node)
{
  int count= 0;
  while (node)
    {
      fprintf(output, "#define %s G->val[%d]\n", node->variable.name, --count);
      node->variable.offset= count;
      node= node->variable.next;
    }
}

static void undefineVariables(Node *node)
{
  while (node)
    {
      fprintf(output, "#undef %s\n", node->variable.name);
      node= node->variable.next;
    }
}


static void Rule_compile_c2(Node *node)
{
  assert(node);
  assert(Rule == node->type);

  if (!node->rule.expression)
    fprintf(stderr, "rule '%s' used but not defined\n", node->rule.name);
  else
    {
      int ko= yyl(), safe;

      if ((!(RuleUsed & node->rule.flags)) && (node != start))
	fprintf(stderr, "rule '%s' defined but not used\n", node->rule.name);

      safe= ((Query == node->rule.expression->type) || (Star == node->rule.expression->type));

      fprintf(output, "\nYY_RULE(int) yy_%s(GREG *G)\n{", node->rule.name);
      if (!safe) save(0);
      if (node->rule.variables)
	fprintf(output, "  yyDo(G, yyPush, %d, 0, \"yyPush\");\n", countVariables(node->rule.variables));
      fprintf(output, "  yyprintfv((stderr, \"%%s\\n\", \"%s\"));\n", node->rule.name);
      Node_compile_c_ko(node->rule.expression, ko);
      fprintf(output, "  yyprintf((stderr, \"  ok   %s\"));\n", node->rule.name);
      fprintf(output, "  yyprintfGcontext;\n");
      fprintf(output, "  yyprintf((stderr, \"\\n\"));\n");
      if (node->rule.variables)
	fprintf(output, "  yyDo(G, yyPop, %d, 0, \"yyPop\");", countVariables(node->rule.variables));
      fprintf(output, "\n  return 1;");
      if (!safe)
	{
	  label(ko);
	  restore(0);
	  fprintf(output, "  yyprintfv((stderr, \"  fail %%s\", \"%s\"));\n", node->rule.name);
	  fprintf(output, "  yyprintfvGcontext;\n");
	  fprintf(output, "  yyprintfv((stderr, \"\\n\"));\n");
	  fprintf(output, "\n  return 0;");
	}
      fprintf(output, "\n}");
    }

  if (node->rule.next)
    Rule_compile_c2(node->rule.next);
}

#ifdef YY_DEBUG
static void yyprintcontext(GREG *G, FILE *stream, char *s)
{
  char *context = s;
  char *nl = strchr(context, 10);
  if (nl) {
    context = (char*)malloc(nl-s+1);
    strncpy(context, s, nl-s);
    context[nl-s] = '\0'; /* replace nl by 0 */
  }
  fprintf(stream, " @ \"%s\"", context);
  if (nl) free(context);
}
#endif

static char *header= "\
#include <stdio.h>\n\
#include <stdlib.h>\n\
#include <string.h>\n\
struct _GREG;\n\
";

static char *preamble= "\
#ifndef YY_ALLOC\n\
#define YY_ALLOC(N, D) malloc(N)\n\
#endif\n\
#ifndef YY_CALLOC\n\
#define YY_CALLOC(N, S, D) calloc(N, S)\n\
#endif\n\
#ifndef YY_REALLOC\n\
#define YY_REALLOC(B, N, D) realloc(B, N)\n\
#endif\n\
#ifndef YY_FREE\n\
#define YY_FREE free\n\
#endif\n\
#ifndef YY_LOCAL\n\
#define YY_LOCAL(T)	static T\n\
#endif\n\
#ifndef YY_ACTION\n\
#define YY_ACTION(T)	static T\n\
#endif\n\
#ifndef YY_RULE\n\
#define YY_RULE(T)	static T\n\
#endif\n\
#ifndef YY_PARSE\n\
#define YY_PARSE(T)	T\n\
#endif\n\
#ifndef YY_NAME\n\
#define YY_NAME(N) yy##N\n\
#endif\n\
#ifndef YY_INPUT\n\
#define YY_INPUT(buf, result, max_size, D)		\\\n\
  {							\\\n\
    int yyc= getchar();					\\\n\
    if ('\\n' == c || '\\r' == c) ++lineNumber;	        \\\n\
    result= (EOF == yyc) ? 0 : (*(buf)= yyc, 1);	\\\n\
    yyprintf((stderr, \"<%c>\", yyc));			\\\n\
  }\n\
#endif\n\
#ifndef YY_BEGIN\n\
#define YY_BEGIN	( G->begin= G->pos, 1)\n\
#endif\n\
#ifndef YY_END\n\
#define YY_END		( G->end= G->pos, 1)\n\
#endif\n\
#ifdef YY_DEBUG\n\
# define yyprintf(args)	  if (G->debug & DEBUG_PARSE)         fprintf args\n\
# define yyprintfv(args)  if (G->debug & DEBUG_PARSE && G->debug & DEBUG_VERBOSE) fprintf args\n\
# define yyprintfGcontext  if (G->debug & DEBUG_PARSE)         yyprintcontext(G,stderr,G->buf+G->pos)\n\
# define yyprintfvGcontext if (G->debug & DEBUG_PARSE && G->debug & DEBUG_VERBOSE) yyprintcontext(G,stderr,G->buf+G->pos)\n\
#else\n\
# define yyprintf(args)\n\
# define yyprintfv(args)\n\
# define yyprintfGcontext\n\
# define yyprintfvGcontext\n\
#endif\n\
#ifndef YYSTYPE\n\
#define YYSTYPE	int\n\
#endif\n\
#ifndef YY_XTYPE\n\
#define YY_XTYPE void *\n\
#endif\n\
#ifndef YY_XVAR\n\
#define YY_XVAR yydata\n\
#endif\n\
\n\
#ifndef YY_STACK_SIZE\n\
#define YY_STACK_SIZE 128\n\
#endif\n\
\n\
#ifndef YY_BUFFER_START_SIZE\n\
#define YY_BUFFER_START_SIZE 1024\n\
#endif\n\
\n\
#ifndef YY_PART\n\
#define yydata G->data\n\
#define yy G->ss\n\
\n\
struct _yythunk; // forward declaration\n\
typedef void (*yyaction)(struct _GREG *G, char *yytext, int yyleng, struct _yythunk *thunkpos, YY_XTYPE YY_XVAR);\n\
typedef struct _yythunk { int begin, end;  yyaction  action; const char *name; struct _yythunk *next; } yythunk;\n\
\n\
typedef struct _GREG {\n\
  char *buf;\n\
  int buflen;\n\
  int   offset;\n\
  int   pos;\n\
  int   limit;\n\
  char *text;\n\
  int	textlen;\n\
  int	begin;\n\
  int	end;\n\
  yythunk *thunks;\n\
  int	thunkslen;\n\
  int thunkpos;\n\
  YYSTYPE ss;\n\
  YYSTYPE *val;\n\
  YYSTYPE *vals;\n\
  int valslen;\n\
  YY_XTYPE data;\n\
#ifdef YY_DEBUG\n\
  int debug;\n\
#endif\n\
} GREG;\n\
\n\
YY_LOCAL(int) yyrefill(GREG *G)\n\
{\n\
  int yyn;\n\
  YY_XTYPE YY_XVAR = (YY_XTYPE)G->data;\n\
  while (G->buflen - G->pos < 512)\n\
    {\n\
      G->buflen *= 2;\n\
      G->buf= (char*)YY_REALLOC(G->buf, G->buflen, G->data);\n\
    }\n\
  YY_INPUT((G->buf + G->pos), yyn, (G->buflen - G->pos), G->data);\n\
  if (!yyn) return 0;\n\
  G->limit += yyn;\n\
  return 1;\n\
}\n\
\n\
YY_LOCAL(int) yymatchDot(GREG *G)\n\
{\n\
  if (G->pos >= G->limit && !yyrefill(G)) return 0;\n\
  ++G->pos;\n\
  return 1;\n\
}\n\
\n\
#ifdef YY_DEBUG\n\
YY_LOCAL(void) yyprintcontext(GREG *G, FILE *stream, char *s)\n\
{\n\
  char *context = s;\n\
  char *nl = strchr(context, 10);\n\
  if (nl) {\n\
    context = (char*)malloc(nl-s+1);\n\
    strncpy(context, s, nl-s);\n\
    context[nl-s] = '\\0'; /* replace nl by 0 */\n\
  }\n\
  fprintf(stream, \" @ \\\"%s\\\"\", context);\n\
  if (nl) free(context);\n\
}\n\
#endif\n\
\n\
YY_LOCAL(int) yymatchChar(GREG *G, int c)\n\
{\n\
  if (G->pos >= G->limit && !yyrefill(G)) return 0;\n\
  if ((unsigned char)G->buf[G->pos] == c)\n\
    {\n\
      ++G->pos;\n\
      if (c<32) { yyprintf((stderr, \"  ok   yymatchChar '0x%x'\", c));}\n\
      else      { yyprintf((stderr, \"  ok   yymatchChar '%c'\", c));}\n\
      yyprintfGcontext;\n\
      yyprintf((stderr, \"\\n\"));\n\
      return 1;\n\
    }\n\
  if (c<32) { yyprintfv((stderr, \"  fail yymatchChar '0x%x'\", c));}\n\
  else      { yyprintfv((stderr, \"  fail yymatchChar '%c'\", c));}\n\
  yyprintfvGcontext;\n\
  yyprintfv((stderr, \"\\n\"));\n\
  return 0;\n\
}\n\
\n\
YY_LOCAL(int) yymatchString(GREG *G, const char *s)\n\
{\n\
  int yysav= G->pos;\n\
  while (*s)\n\
    {\n\
      if (G->pos >= G->limit && !yyrefill(G)) return 0;\n\
      if (G->buf[G->pos] != *s)\n\
        {\n\
          G->pos= yysav;\n\
          return 0;\n\
        }\n\
      ++s;\n\
      ++G->pos;\n\
    }\n\
  return 1;\n\
}\n\
\n\
YY_LOCAL(int) yymatchClass(GREG *G, unsigned char *bits, char *cclass)\n\
{\n\
  int c;\n\
  if (G->pos >= G->limit && !yyrefill(G)) return 0;\n\
  c= (unsigned char)G->buf[G->pos];\n\
  if (bits[c >> 3] & (1 << (c & 7)))\n\
    {\n\
      ++G->pos;\n\
      yyprintf((stderr, \"  ok   yymatchClass [%s]\", cclass));\n\
      yyprintfGcontext;\n\
      yyprintf((stderr, \"\\n\"));\n\
      return 1;\n\
    }\n\
  yyprintfv((stderr, \"  fail yymatchClass [%s]\", cclass));\n	\
  yyprintfvGcontext;\n\
  yyprintfv((stderr, \"\\n\"));\n\
  return 0;\n\
}\n\
\n\
YY_LOCAL(void) yyDo(GREG *G, yyaction action, int begin, int end, const char *name)\n\
{\n\
  while (G->thunkpos >= G->thunkslen)\n\
    {\n\
      G->thunkslen *= 2;\n\
      G->thunks= (yythunk*)YY_REALLOC(G->thunks, sizeof(yythunk) * G->thunkslen, G->data);\n\
    }\n\
  G->thunks[G->thunkpos].begin=  begin;\n\
  G->thunks[G->thunkpos].end=    end;\n\
  G->thunks[G->thunkpos].action= action;\n\
  G->thunks[G->thunkpos].name= name;\n\
  ++G->thunkpos;\n\
}\n\
\n\
YY_LOCAL(int) yyText(GREG *G, int begin, int end)\n\
{\n\
  int yyleng= end - begin;\n\
  if (yyleng <= 0)\n\
    yyleng= 0;\n\
  else\n\
    {\n\
      while (G->textlen < (yyleng + 1))\n\
        {\n\
          G->textlen *= 2;\n\
          G->text= (char*)YY_REALLOC(G->text, G->textlen, G->data);\n\
        }\n\
      memcpy(G->text, G->buf + begin, yyleng);\n\
    }\n\
  G->text[yyleng]= '\\0';\n\
  return yyleng;\n\
}\n\
\n\
YY_LOCAL(void) yyDone(GREG *G)\n\
{\n\
  int pos;\n\
  for (pos= 0;  pos < G->thunkpos;  ++pos)\n\
    {\n\
      yythunk *thunk= &G->thunks[pos];\n\
      int yyleng= thunk->end ? yyText(G, thunk->begin, thunk->end) : thunk->begin;\n\
      yyprintfv((stderr, \"DO [%d] %s @ \\\"%s\\\"\\n\", pos, thunk->name, G->text));\n\
      thunk->action(G, G->text, yyleng, thunk, G->data);\n\
    }\n\
  G->thunkpos= 0;\n\
}\n\
\n\
YY_LOCAL(void) yyCommit(GREG *G)\n\
{\n\
  if ((G->limit -= G->pos))\n\
    {\n\
      memmove(G->buf, G->buf + G->pos, G->limit);\n\
    }\n\
  G->offset += G->pos;\n\
  G->begin -= G->pos;\n\
  G->end -= G->pos;\n\
  G->pos= G->thunkpos= 0;\n\
}\n\
\n\
YY_LOCAL(int) yyAccept(GREG *G, int tp0)\n\
{\n\
  if (tp0)\n\
    {\n\
      fprintf(stderr, \"accept denied at %d\\n\", tp0);\n\
      return 0;\n\
    }\n\
  else\n\
    {\n\
      yyDone(G);\n\
      yyCommit(G);\n\
    }\n\
  return 1;\n\
}\n\
\n\
YY_LOCAL(void) yyPush(GREG *G, char *text, int count, yythunk *thunk, YY_XTYPE YY_XVAR)	{\n\
  size_t off = (G->val - G->vals) + count;\n\
  if (off > G->valslen) {\n\
    while (G->valslen < off + 1)\n\
      G->valslen *= 2;\n\
    G->vals= (YYSTYPE*)YY_REALLOC((void *)G->vals, sizeof(YYSTYPE) * G->valslen, G->data);\n\
    G->val= G->vals + off;\n\
  } else {\n\
    G->val += count;\n\
  }\n\
}\n\
YY_LOCAL(void) yyPop(GREG *G, char *text, int count, yythunk *thunk, YY_XTYPE YY_XVAR)	{ G->val -= count; }\n\
YY_LOCAL(void) yySet(GREG *G, char *text, int count, yythunk *thunk, YY_XTYPE YY_XVAR)	{ G->val[count]= G->ss; }\n\
\n\
#endif /* YY_PART */\n\
\n\
#define	YYACCEPT	yyAccept(G, yythunkpos0)\n\
\n\
";

static char *footer= "\n\
\n\
#ifndef YY_PART\n\
\n\
typedef int (*yyrule)(GREG *G);\n\
\n\
YY_PARSE(int) YY_NAME(parse_from)(GREG *G, yyrule yystart)\n\
{\n\
  int yyok;\n\
  if (!G->buflen)\n\
    {\n\
      G->buflen= YY_BUFFER_START_SIZE;\n\
      G->buf= (char*)YY_ALLOC(G->buflen, G->data);\n\
      G->textlen= YY_BUFFER_START_SIZE;\n\
      G->text= (char*)YY_ALLOC(G->textlen, G->data);\n\
      G->thunkslen= YY_STACK_SIZE;\n\
      G->thunks= (yythunk*)YY_ALLOC(sizeof(yythunk) * G->thunkslen, G->data);\n\
      G->valslen= YY_STACK_SIZE;\n\
      G->vals= (YYSTYPE*)YY_ALLOC(sizeof(YYSTYPE) * G->valslen, G->data);\n\
      G->begin= G->end= G->pos= G->limit= G->thunkpos= 0;\n\
    }\n\
  G->pos = 0;\n\
  G->begin= G->end= G->pos;\n\
  G->thunkpos= 0;\n\
  G->val= G->vals;\n\
  yyok= yystart(G);\n\
  if (yyok) yyDone(G);\n\
  yyCommit(G);\n\
  return yyok;\n\
  (void)yyrefill;\n\
  (void)yymatchDot;\n\
  (void)yymatchChar;\n\
  (void)yymatchString;\n\
  (void)yymatchClass;\n\
  (void)yyDo;\n\
  (void)yyText;\n\
  (void)yyDone;\n\
  (void)yyCommit;\n\
  (void)yyAccept;\n\
  (void)yyPush;\n\
  (void)yyPop;\n\
  (void)yySet;\n\
}\n\
\n\
YY_PARSE(int) YY_NAME(parse)(GREG *G)\n\
{\n\
  return YY_NAME(parse_from)(G, yy_%s);\n\
}\n\
\n\
YY_PARSE(void) YY_NAME(init)(GREG *G)\n\
{\n\
    memset(G, 0, sizeof(GREG));\n\
}\n\
YY_PARSE(void) YY_NAME(deinit)(GREG *G)\n\
{\n\
    if (G->buf) YY_FREE(G->buf);\n\
    if (G->text) YY_FREE(G->text);\n\
    if (G->thunks) YY_FREE(G->thunks);\n\
    if (G->vals) YY_FREE((void*)G->vals);\n\
}\n\
YY_PARSE(GREG *) YY_NAME(parse_new)(YY_XTYPE data)\n\
{\n\
  GREG *G = (GREG *)YY_CALLOC(1, sizeof(GREG), G->data);\n\
  G->data = data;\n\
  return G;\n\
}\n\
\n\
YY_PARSE(void) YY_NAME(parse_free)(GREG *G)\n\
{\n\
  YY_NAME(deinit)(G);\n\
  YY_FREE(G);\n\
}\n\
\n\
#endif\n\
";

void Rule_compile_c_header(void)
{
  fprintf(output, "/* A recursive-descent parser generated by greg %d.%d.%d */\n", GREG_MAJOR, GREG_MINOR, GREG_LEVEL);
  fprintf(output, "\n");
  fprintf(output, "%s", header);
  fprintf(output, "#define YYRULECOUNT %d\n", ruleCount);
}

int consumesInput(Node *node)
{
  if (!node) return 0;

  switch (node->type)
    {
    case Rule:
      {
	int result= 0;
	if (RuleReached & node->rule.flags)
	  fprintf(stderr, "possible infinite left recursion in rule '%s'\n", node->rule.name);
	else
	  {
	    node->rule.flags |= RuleReached;
	    result= consumesInput(node->rule.expression);
	    node->rule.flags &= ~RuleReached;
	  }
	return result;
      }
      break;

    case Dot:		return 1;
    case Name:		return consumesInput(node->name.rule);
    case Character:
    case String:	return strlen(node->string.value) > 0;
    case Class:		return 1;
    case Action:	return 0;
    case Predicate:	return 0;

    case Alternate:
      {
	Node *n;
	for (n= node->alternate.first;  n;  n= n->alternate.next)
	  if (!consumesInput(n))
	    return 0;
      }
      return 1;

    case Sequence:
      {
	Node *n;
	for (n= node->alternate.first;  n;  n= n->alternate.next)
	  if (consumesInput(n))
	    return 1;
      }
      return 0;

    case PeekFor:	return 0;
    case PeekNot:	return 0;
    case Query:		return 0;
    case Star:		return 0;
    case Plus:		return consumesInput(node->plus.element);

    default:
      fprintf(stderr, "\nconsumesInput: illegal node type %d\n", node->type);
      exit(1);
    }
  return 0;
}


void Rule_compile_c(Node *node)
{
  Node *n;

  for (n= rules;  n;  n= n->rule.next)
    consumesInput(n);

  fprintf(output, "%s", preamble);
  for (n= node;  n;  n= n->rule.next)
    fprintf(output, "YY_RULE(int) yy_%s(GREG *G); /* %d */\n", n->rule.name, n->rule.id);
  fprintf(output, "\n");
  for (n= actions;  n;  n= n->action.list)
    {
      char *block = n->action.text;
      fprintf(output, "YY_ACTION(void) yy%s(GREG *G, char *yytext, int yyleng, yythunk *thunk, YY_XTYPE YY_XVAR)\n{\n", n->action.name);
      defineVariables(n->action.rule->rule.variables);
      while (*block == 0x20) block++;
      fprintf(output, "  yyprintf((stderr, \"do yy%s\"));\n", n->action.name);
      fprintf(output, "#ifdef YY_DEBUG\n");
      fprintf(output, "  if (G->debug & DEBUG_PARSE) yyprintcontext(G,stderr,yytext);\n");
      fprintf(output, "#endif\n");
      fprintf(output, "  yyprintf((stderr, \"\\n  {%s}\\n\"));\n", yyqq(block));
      fprintf(output, "  %s;\n", block);
      undefineVariables(n->action.rule->rule.variables);
      fprintf(output, "}\n");
    }
  Rule_compile_c2(node);
  fprintf(output, footer, start->rule.name);
}
