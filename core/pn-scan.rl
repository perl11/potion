//
// pn-scan.rl
// the tokenizer for Potion
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "internal.h"
#include "potion.h"
#include "pn-gram.h"
#include "pn-ast.h"

#define TOK(id)   printf("%s: %.*s\n", "" # id, (int)(te - ts), ts)
#define TOKEN(id) LemonPotion(pParser, PN_TOK_##id, 0, NULL)

%%{
  machine potion;

  comma       = ",";
  newline     = "\r"? "\n";
  whitespace  = " " | "\f" | "\t" | "\v";
  utfw        = alnum | "_" | "$" | "@" | "{" | "}" |
                (0xc4 0xa8..0xbf) | (0xc5..0xdf 0x80..0xbf) |
                (0xe0..0xef 0x80..0xbf 0x80..0xbf) |
                (0xf0..0xf4 0x80..0xbf 0x80..0xbf 0x80..0xbf);
  utf8        = 0x09 | 0x0a | 0x0d | (0x20..0x7e) |
                (0xc2..0xdf) (0x80..0xbf) |
                (0xe0..0xef 0x80..0xbf 0x80..0xbf) |
                (0xf0..0xf4 0x80..0xbf 0x80..0xbf 0x80..0xbf);
  ops         = "~" | "!" | "+" | "-" | "*" | "**" | "/" | "//" |
                "^" | ">" | "<" | "<=" | ">=" | "&" | "&&" | "|" |
                "||" | "\\";
  braced      = '{' (any - '}')+ '}' | '[' (any - ']')+ ']';

  message     = utfw+ braced?;
  query       = "?" utfw+;
  assign      = "=";
  begin_block = ":";
  end_block   = ".";
  begin_table = "(";
  end_table   = ")";
  begin_data  = "[";
  end_data    = "]";
  path        = "/" ("/" | utfw)+;
  comment     = "#"+ (utf8 - newline)*;

  nil         = "nil";
  true        = "true";
  false       = "false";
  int         = [0-9]+;
  float       = ("0" | [1-9] [0-9]*) ("." [0-9]+)? ("e" [\-+] [0-9]+)?;
  schar1      = (utf8 - "'") | "\\'";
  schar2      = (utf8 - ["\\]) | "\\u" [0-9]{4} | '\\"' | "\\\\" | "\\/" |
                "\\b" | "\\f" | "\\n" | "\\r" | "\\t";
  string      = "'" schar1* "'" | '"' schar2* '"';
  string2     = "% " (utf8 - newline)+;

  main := |*
    comma       => { TOKEN(SEP); };
    whitespace;
    comment;
    assign      => { TOKEN(ASSIGN); };
    begin_table => { TOKEN(BEGIN_TABLE); };
    end_table   => { TOKEN(END_TABLE); };
    begin_data  => { TOKEN(BEGIN_DATA); };
    end_data    => { TOKEN(END_DATA); };
    begin_block => { TOKEN(BEGIN_BLOCK); };
    end_block   => { TOKEN(END_BLOCK); };
    newline     => { TOKEN(SEP); lineno++; };
    ops         => { TOKEN(OPS); };
    path        => { TOKEN(PATH); };

    nil         => { TOKEN(NIL); };
    true        => { TOKEN(TRUE); };
    false       => { TOKEN(FALSE); };
    int         => { TOKEN(INT); };
    float       => { TOKEN(FLOAT); };
    string      => { TOKEN(STRING); };
    string2     => { TOKEN(STRING2); };

    message     => { TOKEN(MESSAGE); };
    query       => { TOK(QUERY); };
  *|;

  write data nofinal;
}%%

PN potion_parse(Potion *P, PN code) {
  int cs, act;
  char *p, *pe, *ts, *te, *eof = 0;
  int lineno = 0;
  PN src = potion_source(P, AST_CODE);
  void *pParser = LemonPotionAlloc(malloc);

  p = PN_STR_PTR(code);
  pe = p + PN_STR_LEN(code) + 1;

  %% write init;
  %% write exec;

  LemonPotion(pParser, 0, 0, NULL);
  LemonPotionFree(pParser, free);
}

void potion_run() {
  PN code;
  Potion *P = potion_create();
  code = potion_parse(P, potion_str(P,
    "average = (x, y): (x + y) / 2."
  ));
  code = potion_parse(P, potion_str(P,
    "if (/ball top < 0): 'You win!'. else: 'Computer wins'."
  ));
  code = potion_parse(P, potion_str(P,
    "if (/ball top + ball_diameter < 0 or /ball top > app height):\n"
    "  para (top=140, align=center):\n"
    "    [strong \"GAME OVER\" (size=32), \"\\n\"]. # <strong size=32>GAME OVER</strong>\n"
    "  /ball hide, /anim stop."
  ));
  code = potion_parse(P, potion_str(P,
    "get (/test/hello):\n"
    "  ['Hello, world!']."
  ));
  potion_destroy(P);
}
