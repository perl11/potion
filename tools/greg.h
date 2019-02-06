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
 * Last edited: 2019-02-06 12:05:37 rurban
 */

#include <stdio.h>
#ifdef WIN32
# undef inline
# define inline __inline
#endif

#define GREG_MAJOR	0
#define GREG_MINOR	4
#define GREG_LEVEL	6

typedef enum { Freed = -1, Unknown= 0, Rule, Variable, Name, Dot, Character, String,
               Class, Action, Predicate, Error, Alternate, Sequence, PeekFor, PeekNot,
               Query, Star, Plus, Any } NodeType;

enum {
  RuleUsed	= 1<<0,
  RuleReached	= 1<<1,
};

typedef union Node Node;

#define NODE_COMMON NodeType type;  Node *next
struct Rule	 { NODE_COMMON; char *name; Node *variables; Node *expression; int id; int flags; };
struct Variable	 { NODE_COMMON; char *name; Node *value;  int offset;		   };
struct Name	 { NODE_COMMON; Node *rule; Node *variable;			   };
struct Dot	 { NODE_COMMON;							   };
struct Character { NODE_COMMON; char *value;					   };
struct String	 { NODE_COMMON; char *value;					   };
struct Class	 { NODE_COMMON; unsigned char *value;				   };
struct Action	 { NODE_COMMON; char *text;  Node *list;  char *name;  Node *rule; };
struct Predicate { NODE_COMMON; char *text;					   };
struct Error     { NODE_COMMON; Node *element; char *text;			   };
struct Alternate { NODE_COMMON; Node *first;  Node *last;			   };
struct Sequence	 { NODE_COMMON; Node *first;  Node *last;			   };
struct PeekFor	 { NODE_COMMON; Node *element;					   };
struct PeekNot	 { NODE_COMMON; Node *element;					   };
struct Query	 { NODE_COMMON; Node *element;					   };
struct Star	 { NODE_COMMON; Node *element;					   };
struct Plus	 { NODE_COMMON; Node *element;					   };
struct Any	 { NODE_COMMON;							   };
#undef NODE_COMMON

#ifndef YYSTYPE
#define YYSTYPE	int
#endif
#ifndef YY_XTYPE
#define YY_XTYPE void *
#endif
struct _yythunk; // forward declaration
typedef struct _GREG {
  char *buf;
  int   buflen;
  int   offset;
  int   pos;
  int   limit;
  char *text;
  int	textlen;
  int	begin;
  int	end;
  struct _yythunk *thunks;
  int	thunkslen;
  int   thunkpos;
  int	lineno;
  char	*filename;
  FILE  *input;
  YYSTYPE ss;
  YYSTYPE *val;
  YYSTYPE *vals;
  int valslen;
  YY_XTYPE data;
#ifdef YY_DEBUG
  int debug;
#endif
} GREG;

union Node
{
  NodeType		type;
  struct Rule		rule;
  struct Variable	variable;
  struct Name		name;
  struct Dot		dot;
  struct Character	character;
  struct String		string;
  struct Class		cclass;
  struct Action		action;
  struct Predicate	predicate;
  struct Error	 	error;
  struct Alternate	alternate;
  struct Sequence	sequence;
  struct PeekFor	peekFor;
  struct PeekNot	peekNot;
  struct Query		query;
  struct Star		star;
  struct Plus		plus;
  struct Any		any;
};

extern Node *actions;
extern Node *rules;
extern Node *start;

extern int   ruleCount;

extern FILE *output;

extern Node *makeRule(char *name, int starts);
extern Node *findRule(char *name, int starts);
extern Node *beginRule(Node *rule);
extern void  Rule_setExpression(Node *rule, Node *expression);
extern Node *Rule_beToken(Node *rule);
extern Node *makeVariable(char *name);
extern Node *makeName(Node *rule);
extern Node *makeDot(void);
extern Node *makeCharacter(char *text);
extern Node *makeString(char *text);
extern Node *makeClass(char *text);
extern Node *makeAction(char *text);
extern Node *makePredicate(char *text);
extern Node *makeError(Node *e, char *text);
extern Node *makeAlternate(Node *e);
extern Node *Alternate_append(Node *e, Node *f);
extern Node *makeSequence(Node *e);
extern Node *Sequence_append(Node *e, Node *f);
extern Node *makePeekFor(Node *e);
extern Node *makePeekNot(Node *e);
extern Node *makeQuery(Node *e);
extern Node *makeStar(Node *e);
extern Node *makePlus(Node *e);
extern Node *push(Node *node);
extern Node *top(void);
extern Node *pop(void);

extern void  Rule_compile_c_header(void);
extern void  Rule_compile_c(Node *node);

extern void  Node_print(Node *node);
extern void  Rule_print(Node *node);
extern void  Rule_free(Node *node);
extern void  freeRules(void);
