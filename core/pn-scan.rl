//
// pn-scan.rl
// the tokenizer for Potion
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "potion.h"
#include "internal.h"
#include "pn-gram.h"
#include "pn-ast.h"

#define TOKEN2(id,v) LemonPotion(pParser, PN_TOK_##id, v, P); last = PN_TOK_##id
#define TOKEN1(id)   if (last != PN_TOK_##id) { TOKEN(id); }
#define TOKEN(id)    TOKEN2(id, PN_NIL)

%%{
  machine utf8;

  comma       = ",";
  newline     = "\r"? "\n" %{ lineno++; };
  whitespace  = " " | "\f" | "\t" | "\v";
  utfw        = alnum | "_" | "$" | "@" | "{" | "}" |
                (0xc4 0xa8..0xbf) | (0xc5..0xdf 0x80..0xbf) |
                (0xe0..0xef 0x80..0xbf 0x80..0xbf) |
                (0xf0..0xf4 0x80..0xbf 0x80..0xbf 0x80..0xbf);
  utf8        = 0x09 | 0x0a | 0x0d | (0x20..0x7e) |
                (0xc2..0xdf) (0x80..0xbf) |
                (0xe0..0xef 0x80..0xbf 0x80..0xbf) |
                (0xf0..0xf4 0x80..0xbf 0x80..0xbf 0x80..0xbf);
  braced      = '{' (any - '}')+ '}' | '[' (any - ']')+ ']';
  message     = utfw+ braced?;
}%%

#define SCHAR(ptr, len) PN_MEMCPY_N(PN_STR_PTR(sbuf) + nbuf, ptr, char, len); nbuf += len
  
%%{
  machine potion;
  include utf8;

  begin_block = ":";
  end_block   = ".";
  end_blocks  = "_" whitespace* utfw+;
  begin_table = "(";
  end_table   = ")";
  begin_data  = "[";
  end_data    = "]";
  path        = "/" ("/" | utfw)+;
  query       = "?" message;
  querypath   = "?" path;
  comment     = "#"+ (utf8 - newline)*;

  nil         = "nil";
  true        = "true";
  false       = "false";
  int         = [0-9]+;
  int2        = "-" int;
  float       = ("0" | [1-9] [0-9]*) ("." [0-9]+)? ("e" [\-+] [0-9]+)?;
  schar1      = utf8 -- "\\'";
  escape1     = "\\\"" | "\\\\" | "\\/";
  escn        = "\\n";
  escb        = "\\b";
  escf        = "\\f";
  escr        = "\\t";
  esct        = "\\t";
  escu        = "\\u" [0-9a-fA-F]{4};
  schar2      = utf8 -- (escn | escape1 | escb | escf | escn | escr | esct);
  quote1      = "'";
  quote2      = '"';
  string3     = "%% " (utf8+ -- newline{2});

  string1 := |*
    "\\'"       => { SCHAR(ts + 1, 1); };
    "'"         => { TOKEN2(STRING, potion_str2(P, PN_STR_PTR(sbuf), nbuf)); fgoto main; };
    schar1      => { SCHAR(ts, te - ts); };
  *|;

  string2 := |*
    escape1     => { SCHAR(ts + 1, 1); };
    escn        => { SCHAR("\n", 1); };
    escb        => { SCHAR("\b", 1); };
    escf        => { SCHAR("\f", 1); };
    escr        => { SCHAR("\r", 1); };
    esct        => { SCHAR("\t", 1); };
    escu        => {
      int cl = te - ts;
      char c = ts[cl];
      char *utfc = PN_STR_PTR(sbuf);
      unsigned long code;
      ts[cl] = '\0'; code = strtoul(ts+2, NULL, 16); ts[cl] = c;
      if (code < 0x80) {
        utfc[nbuf++] = code;
      } else if (code < 0x7ff) {
        utfc[nbuf++] = (code >> 6) | 0xc0;
        utfc[nbuf++] = (code & 0x3f) | 0x80;
      } else if (code < 0xffff) {
        utfc[nbuf++] = (code >> 12) | 0xe0;
        utfc[nbuf++] = ((code >> 6) & 0x3f) | 0x80;
        utfc[nbuf++] = (code & 0x3f) | 0x80;
      }
    };
    '"'         => { TOKEN2(STRING, potion_str2(P, PN_STR_PTR(sbuf), nbuf)); fgoto main; };
    schar2      => { SCHAR(ts, te - ts); };
  *|;

  main := |*
    comma       => { TOKEN1(SEP); };
    whitespace;
    comment;

    "||"|"or"   => { TOKEN(OR); };
    "&&"|"and"  => { TOKEN(AND); };
#   "!"|"not"   => { TOKEN(NOT); };
    "<=>"       => { TOKEN(CMP); };
    "=="        => { TOKEN(EQ); };
    "!="        => { TOKEN(NEQ); };
    ">"         => { TOKEN(GT); };
    ">="        => { TOKEN(GTE); };
    "<"         => { TOKEN(LT); };
    "<="        => { TOKEN(LTE); };
    "|"         => { TOKEN(PIPE); };
    "^"         => { TOKEN(CARET); };
    "&"         => { TOKEN(AMP); };
    "<<"        => { TOKEN(BITL); };
    ">>"        => { TOKEN(BITR); };
    "+"         => { TOKEN(PLUS); };
    "-"         => { TOKEN(MINUS); };
    "*"         => { TOKEN(TIMES); };
    "/"         => { TOKEN(DIV); };
    "%"         => { TOKEN(REM); };
    "**"        => { TOKEN(POW); };
#   "~"         => { TOKEN(WAVY); };
    "="         => { TOKEN(ASSIGN); };

    begin_table => { TOKEN2(BEGIN_TABLE, PN_TUP0()); };
    end_table   => { TOKEN(END_TABLE); };
    begin_data  => { TOKEN(BEGIN_DATA); };
    end_data    => { TOKEN(END_DATA); };
    begin_block => { TOKEN(BEGIN_BLOCK); };
    end_block   => { TOKEN(END_BLOCK); };
    end_blocks  => { TOKEN(END_BLOCK); };
    newline+    => { TOKEN1(SEP); };
    path        => { TOKEN2(PATH, potion_str2(P, ts, te - ts)); };

    nil         => { TOKEN2(NIL, PN_NIL); };
    true        => { TOKEN2(TRUE, PN_TRUE); };
    false       => { TOKEN2(FALSE, PN_FALSE); };
    int         => { TOKEN2(INT, PN_NUM(PN_ATOI(ts, te - ts))); };
    int2        => { TOKEN2(INT, PN_NUM(-PN_ATOI(ts + 1, te - (ts + 1)))); };
    float       => { TOKEN2(FLOAT, PN_NUM(PN_ATOI(ts, te - ts))); };
    quote1      => { nbuf = 0; fgoto string1; };
    quote2      => { nbuf = 0; fgoto string2; };
    string3     => { 
      if (te[-1] == '\n') te--; if (te[-1] == '\r') te--;
      TOKEN2(STRING2, potion_str2(P, ts + 3, (te - ts) - 3)); };

    message     => { TOKEN2(MESSAGE, potion_str2(P, ts, te - ts)); };
    query       => { TOKEN2(QUERY, potion_str2(P, ts + 1, (te - ts) - 1)); };
    querypath   => { TOKEN2(PATHQ, potion_str2(P, ts + 1, (te - ts) - 1)); };
  *|;

  write data nofinal;
}%%

PN potion_parse(Potion *P, PN code) {
  int cs, act;
  char *p, *pe, *ts, *te, *eof = 0;
  int lineno = 0, nbuf = 0;
  void *pParser = LemonPotionAlloc(malloc);
  PN last = PN_NIL, sbuf = potion_bytes(P, 4096);

  P->xast = 1;
  P->source = PN_NIL;
  p = PN_STR_PTR(code);
  pe = p + PN_STR_LEN(code) + 1;

  %% write init;
  %% write exec;

  LemonPotion(pParser, 0, 0, P);
  LemonPotionFree(pParser, free);

  return P->source;
}

#define ARG_NEXT() cast = 0; eql = NULL
#define ARG_END() \
  int l = PN_TUPLE_LEN(sig), a = l - (x + 1); \
  if (a) PN_TUPLE_AT(sig, x) = PN_NUM(a)

%%{
  machine signature;
  include utf8;

  action cast { cast = p[0]; }
  action name { eql = p; }

  optional = "|" >cast;

  type = ("s" | "S" | "n" | "N" | "l" | "L" | "m" |
    "t" | "o" | "-" | "&") >cast;
  sep = ".";
  key = message "=" >name;
  arg = key type | type;
  
  main := |*
    whitespace;
    arg => {
      if (eql) sig = PN_PUSH(sig, potion_str2(P, ts, eql - ts));
      sig = PN_PUSH(sig, PN_NUM(cast));
    };
    optional => { sig = PN_PUSH(sig, PN_NIL); };
    comma => { ARG_NEXT(); };
    sep => { 
      ARG_END();
      x = l, sig = PN_PUSH(sig, PN_NUM(0)); \
      ARG_NEXT();
    };
  *|;

  write data nofinal;
}%%

PN potion_sig(Potion *P, char *fmt) {
  PN sig;
  int cs, act, x = 0;
  char *p, *pe, *ts, *te, *eof = 0;
  char cast = 0, *eql = NULL;

  if (fmt == NULL) return PN_NIL; // no signature, arg check off
  if (fmt[0] == '\0') return PN_FALSE; // empty signature, no args

  fmt = strdup(fmt);
  p = fmt, pe = fmt + strlen(fmt);
  sig = PN_TUP(PN_NUM(0));

  %% write init;
  %% write exec;

  ARG_END();

  free(fmt);
  return sig;
}
