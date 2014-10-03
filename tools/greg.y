# -*- mode: antlr; tab-width:8 -*-
# LE Grammar for LE Grammars
# 
# Copyright (c) 2007 by Ian Piumarta
# Copyright (c) 2011 by Amos Wenger nddrylliog@gmail.com
# Copyright (c) 2013 by perl11 org
# All rights reserved.
# 
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the 'Software'),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, provided that the above copyright notice(s) and this
# permission notice appear in all copies of the Software.  Acknowledgement
# of the use of this Software in supporting documentation would be
# appreciated but is not required.
# 
# THE SOFTWARE IS PROVIDED 'AS IS'.  USE ENTIRELY AT YOUR OWN RISK.
# 
# Last edited: 2013-10-01 11:36:41 rurban

%{
# include "greg.h"

# include <unistd.h>
# include <libgen.h>
# include <assert.h>

  typedef struct Header Header;

  struct Header {
    char   *text;
    Header *next;
  };

  int   verboseFlag= 0;

  static char	*trailer= 0;
  static Header	*headers= 0;
  static char   *deftrailer= "\n\
#ifdef YY_MAIN\n\
int main()\n\
{\n\
  GREG g;\n\
  yyinit(&g);\n\
  while (yyparse(&g));\n\
  yydeinit(&g);\n\
  return 0;\n\
}\n\
#endif\n\
";

  void makeHeader(char *text);
  void makeTrailer(char *text);
%}

# Hierarchical syntax

grammar=	- ( declaration | definition )+ trailer? end-of-file

declaration=	'%{' < ( !'%}' . )* > RPERCENT		{ makeHeader(yytext); }						#{YYACCEPT}

trailer=	'%%' < .* >				{ makeTrailer(yytext); }					#{YYACCEPT}

definition=	s:identifier 				{ if (push(beginRule(findRule(yytext, s)))->rule.expression)
							    fprintf(stderr, "rule '%s' redefined\n", yytext); }
		EQUAL expression			{ Node *e= pop();  Rule_setExpression(pop(), e); }
		SEMICOLON?												#{YYACCEPT}

expression=	sequence (BAR sequence			{ Node *f= pop();  push(Alternate_append(pop(), f)); }
			    )*

sequence=	error (error				{ Node *f= pop();  push(Sequence_append(pop(), f)); }
			  )*

error=		prefix  (TILDE action			{ push(makeError(pop(), yytext)); }
			)?

prefix=		AND action				{ push(makePredicate(yytext)); }
|		AND suffix				{ push(makePeekFor(pop())); }
|		NOT suffix				{ push(makePeekNot(pop())); }
|		    suffix

suffix=		primary (QUESTION			{ push(makeQuery(pop())); }
                        | STAR			        { push(makeStar (pop())); }
			| PLUS			        { push(makePlus (pop())); }
			)?

primary=	identifier				{ push(makeVariable(yytext)); }
		COLON identifier !EQUAL			{ Node *name= makeName(findRule(yytext, 0));  name->name.variable= pop();  push(name); }
|		identifier !EQUAL			{ push(makeName(findRule(yytext, 0))); }
|		OPEN expression CLOSE
|		literal					{ push(makeString(yytext)); }
|		class					{ push(makeClass(yytext)); }
|		DOT					{ push(makeDot()); }
|		action					{ push(makeAction(yytext)); }
|		BEGIN					{ push(makePredicate("YY_BEGIN")); }
|		END					{ push(makePredicate("YY_END")); }

# Lexical syntax

identifier=	< [-a-zA-Z_][-a-zA-Z_0-9]* > -

literal=	['] < ( !['] char )* > ['] -   #'
|		["] < ( !["] char )* > ["] -

class=		'[' < ( !']' range )* > ']' -

range=		char '-' char | char

char=		'\\' [-abefnrtv'"\[\]\\]
|		'\\' 'x'[0-9A-Fa-f][0-9A-Fa-f]
|		'\\' 'x'[0-9A-Fa-f]
|		'\\' [0-3][0-7][0-7]
|		'\\' [0-7][0-7]?
|		!'\\' .


action=		'{' < braces* > '}' -

braces=		'{' braces* '}'
|		!'}' .

EQUAL=		'=' -
COLON=		':' -
SEMICOLON=	';' -
BAR=		'|' -
AND=		'&' -
NOT=		'!' -
QUESTION=	'?' -
STAR=		'*' -
PLUS=		'+' -
OPEN=		'(' -
CLOSE=		')' -
DOT=		'.' -
BEGIN=		'<' -
END=		'>' -
TILDE=		'~' -
RPERCENT=	'%}' -

-=		(space | comment)*
space=		' ' | '\t' | end-of-line
comment=	'#' (!end-of-line .)* end-of-line
end-of-line=	'\r\n' | '\n' | '\r'
end-of-file=	!.

%%

void makeHeader(char *text)
{
  Header *header= (Header *)malloc(sizeof(Header));
  header->text= strdup(text);
  header->next= headers;
  headers= header;
}

void makeTrailer(char *text)
{
  trailer= strdup(text);
}

static void version(char *name)
{
  printf("%s version %d.%d.%d\n", name, GREG_MAJOR, GREG_MINOR, GREG_LEVEL);
}

static void usage(char *name)
{
  version(name);
  fprintf(stderr, "usage: %s [<option>...] [<file>...]\n", name);
  fprintf(stderr, "where <option> can be\n");
  fprintf(stderr, "  -h          print this help information\n");
  fprintf(stderr, "  -o <ofile>  write output to <ofile>\n");
  fprintf(stderr, "  -v          be verbose\n");
#ifdef YY_DEBUG
  fprintf(stderr, "  -vv         be more verbose\n");
#endif
  fprintf(stderr, "  -V          print version number and exit\n");
  fprintf(stderr, "if no <file> is given, input is read from stdin\n");
  fprintf(stderr, "if no <ofile> is given, output is written to stdout\n");
  exit(1);
}

int main(int argc, char **argv)
{
  GREG *G;
  Node *n;
  int   c;

  output= stdout;

  while (-1 != (c= getopt(argc, argv, "Vho:v")))
    {
      switch (c)
	{
	case 'V':
	  version(basename(argv[0]));
	  exit(0);

	case 'h':
	  usage(basename(argv[0]));
	  break;

	case 'o':
	  if (!(output= fopen(optarg, "w")))
	    {
	      perror(optarg);
	      exit(1);
	    }
	  break;

	case 'v':
	  verboseFlag++;
	  break;

	default:
	  fprintf(stderr, "for usage try: %s -h\n", argv[0]);
	  exit(1);
	}
    }
  argc -= optind;
  argv += optind;

  G = yyparse_new(NULL);
#ifdef YY_DEBUG
  if (verboseFlag > 0) { yydebug = YYDEBUG_PARSE;
    if (verboseFlag > 1) yydebug = YYDEBUG_PARSE + YYDEBUG_VERBOSE;
  }
#endif

  if (argc)
    {
      for (;  argc;  --argc, ++argv)
	{
	  if (strcmp(*argv, "-"))
	    {
	      G->filename= *argv;
	      if (!(G->input= fopen(G->filename, "r")))
		{
		  perror(G->filename);
		  exit(1);
		}
	    }
	  if (!yyparse(G))
	    YY_ERROR("syntax error");
	  if (G->input != stdin)
	    fclose(G->input);
	}
    }
  else
    if (!yyparse(G))
      YY_ERROR("syntax error");
  yyparse_free(G);

  if (verboseFlag)
    for (n= rules;  n;  n= n->any.next)
      Rule_print(n);

  Rule_compile_c_header();

  while (headers) {
    Header *tmp = headers;
    fprintf(output, "%s\n", headers->text);
    free(headers->text);
    tmp= headers->next;
    free(headers);
    headers= tmp;
  }

  if (rules) {
    Rule_compile_c(rules);
    freeRules();
  }

  if (trailer) {
    fputs(trailer, output);
    fputs("\n", output);
    free(trailer);
  }
  else
    fputs(deftrailer, output);

  return 0;
}
