//
// pn-scan.rl
// the tokenizer for Potion
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include "potion.h"
#include "internal.h"
#include "pn-gram.h"
#include "pn-ast.h"

#define TOKEN2(id,v) \
  LemonPotion(pParser, PN_TOK_##id, v, P); \
  if (P->yerror > 0) cs = potion_error; \
  last = PN_TOK_##id
#define TOKEN1(id)   if (last != PN_TOK_##id) { TOKEN(id); }
#define TOKEN(id)    TOKEN2(id, PN_NIL)

%%{
  machine utf8;

  comma       = ",";
  newline     = "\r"? "\n" %{ lineno++; nl = p; };
  whitespace  = " " | "\f" | "\t" | "\v";
  eats        = (newline | whitespace)*;
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

  begin_block = ":" eats;
  end_block   = ".";
  ellipsis    = "_" | "…";
  end_blocks  = ellipsis whitespace* %{ tm = p; } utfw+;
  begin_table = "(" eats;
  end_table   = ")";
  begin_data  = "[" eats;
  end_data    = "]";
  path        = "/" ("/" | utfw)+;
  query       = "?" message;
  querypath   = "?" path;
  comment     = "#"+ (utf8 - newline)*;

  nil         = "nil";
  true        = "true";
  false       = "false";
  int         = [0-9]{1,9};
  dec         = ("0" | [1-9] [0-9]*) %{ tm = NULL; }
                ("." %{ tm = p; } [0-9]+)? ("e" [\-+] [0-9]+)?;
  schar1      = utf8 -- "\\'";
  escape1     = "\\\"" | "\\\\" | "\\/";
  escn        = "\\n";
  escb        = "\\b";
  escf        = "\\f";
  escr        = "\\r";
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
    newline+    => { if (last != PN_NIL) TOKEN1(SEP); };
    whitespace;
    comment;

    ("!" | "not") eats  => { TOKEN(NOT); };
    ("||" | "or") eats  => { TOKEN(OR); };
    ("&&" | "and") eats => { TOKEN(AND); };

    "<=>" eats  => { TOKEN(CMP); };
    "==" eats   => { TOKEN(EQ); };
    "!=" eats   => { TOKEN(NEQ); };
    ">" eats    => { TOKEN(GT); };
    ">=" eats   => { TOKEN(GTE); };
    "<" eats    => { TOKEN(LT); };
    "<=" eats   => { TOKEN(LTE); };
    "|" eats    => { TOKEN(PIPE); };
    "^" eats    => { TOKEN(CARET); };
    "&" eats    => { TOKEN(AMP); };
    "<<" eats   => { TOKEN(BITL); };
    ">>" eats   => { TOKEN(BITR); };
    "+" eats    => { TOKEN(PLUS); };
    "++"        => { TOKEN(PPLUS); };
    "-" eats    => { TOKEN(MINUS); };
    "--"        => { TOKEN(MMINUS); };
    "*" eats    => { TOKEN(TIMES); };
    "/" eats    => { TOKEN(DIV); };
    "%" eats    => { TOKEN(REM); };
    "**" eats   => { TOKEN(POW); };
#   "~"         => { TOKEN(WAVY); };
    "=" eats    => { TOKEN(ASSIGN); };

    begin_table => { TOKEN2(BEGIN_TABLE, PN_TUP0()); };
    end_table   => { TOKEN(END_TABLE); };
    begin_data  => { TOKEN(BEGIN_LICK); };
    end_data    => { TOKEN(END_LICK); };
    begin_block => { TOKEN(BEGIN_BLOCK); P->dast++; };
    end_block   => { TOKEN(END_BLOCK); P->dast--; };
    end_blocks  => { 
      P->unclosed = potion_str2(P, tm, te - tm);
      while (P->dast && P->unclosed != PN_NIL) {
        TOKEN(END_BLOCK); P->dast--; } };

    nil         => { TOKEN2(NIL, PN_NIL); };
    true        => { TOKEN2(TRUE, PN_TRUE); };
    false       => { TOKEN2(FALSE, PN_FALSE); };
    int         => { TOKEN2(INT, PN_NUM(PN_ATOI(ts, te - ts))); };
    dec         => { TOKEN2(DECIMAL, 
      potion_decimal(P, te - ts, (tm == NULL ? te : tm - 1) - ts, ts)); };
    quote1      => { nbuf = 0; fgoto string1; };
    quote2      => { nbuf = 0; fgoto string2; };
    string3     => { 
      if (te[-1] == '\n') te--; if (te[-1] == '\r') te--;
      TOKEN2(STRING2, potion_str2(P, ts + 3, (te - ts) - 3)); };

    message     => { TOKEN2(MESSAGE, potion_str2(P, ts, te - ts)); };
    path        => { TOKEN2(PATH, potion_str2(P, ts + 1, te - (ts + 1))); };
    query       => { TOKEN2(QUERY, potion_str2(P, ts + 1, (te - ts) - 1)); };
    querypath   => { TOKEN2(PATHQ, potion_str2(P, ts + 1, (te - ts) - 1)); };
  *|;

  write data nofinal;
}%%

static char *potion_token_friendly(int tok) {
  switch (tok) {
    case 0: return "end of file";
    case PN_TOK_OR: return "an `or` keyword";
    case PN_TOK_AND: return "an `and` keyword";
    case PN_TOK_ASSIGN: return "an equals sign `=` (used for assignment)";
    case PN_TOK_CMP: return "a comparison operator `<=>`";
    case PN_TOK_EQ: return "a double equals sign `==` (used to check equality)";
    case PN_TOK_NEQ: return "a not equals operator `!=`";
    case PN_TOK_GT: return "a greater-than sign `>`";
    case PN_TOK_GTE: return "a greater-than-or-equals sign `>=`";
    case PN_TOK_LT: return "a less-than sign `<`";
    case PN_TOK_LTE: return "a less-than-or-equals sign `<=`";
    case PN_TOK_PIPE: return "a pipe character `|` (used for bitwise OR)";
    case PN_TOK_CARET: return "a caret symbol `^` (used for bitwise XOR)";
    case PN_TOK_AMP: return "an ampersand `&` (used for bitwise AND)";
    case PN_TOK_BITL: return "a left bitshift `<<`";
    case PN_TOK_BITR: return "a right bitshift `>>`";
    case PN_TOK_PLUS: return "a plus sign `+` (used for addition)";
    case PN_TOK_MINUS: return "a minus sign `-` (used for subtraction)";
    case PN_TOK_TIMES: return "an asterisk `*` (used for multiplication)";
    case PN_TOK_DIV: return "a slash `/` (used for division and instance variables)";
    case PN_TOK_REM: return "a percent `%` (used for modulo math)";
    case PN_TOK_PPLUS: return "a plus-plus `++` (used to increment a number)";
    case PN_TOK_MMINUS: return "a minus-minus `--` (used to decrement a number)";
    case PN_TOK_NOT: return "a logical not (a `!` or `not`)";
    case PN_TOK_SEP: return "a newline or comma";
    case PN_TOK_NIL: return "`nil`";
    case PN_TOK_TRUE: return "`true`";
    case PN_TOK_FALSE: return "`false`";
    case PN_TOK_BEGIN_BLOCK: return "a colon `:` (used to start off a block or function)";
    case PN_TOK_END_BLOCK: return "a period `.` (used to finish a block or function)";
    case PN_TOK_BEGIN_TABLE: return "an opening paren `(` (used to start off a table or list)";
    case PN_TOK_END_TABLE: return "a closing paren `)` (used to start off a table or list)";
    case PN_TOK_BEGIN_LICK: return "an opening bracket `[` (used to start off a lick)";
    case PN_TOK_END_LICK: return "a closing bracket `]` (used to start off a lick)";
  }
  return NULL;
}

static int potion_sample_len = 32;

char *potion_code_excerpt(char *sample, char *nl, char *p, char *pe) {
  char *nl2 = p;
  while (nl2 < pe && *nl2 != 0 && *nl2 != '\r' && *nl2 != '\n') nl2++;
  if (nl2 - nl <= potion_sample_len) {
    sprintf(sample, "%.*s", (int)(nl2 - nl), nl);
  } else if (p - nl <= potion_sample_len) {
    sprintf(sample, "%.*s", potion_sample_len, nl);
  } else {
    sprintf(sample, "...%.*s", potion_sample_len, p - 16);
  }
  return sample;
}

// TODO: make `sbuf` and `code` safe for gc movement
PN potion_parse(Potion *P, PN code) {
  int cs, act;
  char *p, *pe, *nl, *ts, *te, *tm = 0, *eof = 0;
  int lineno = 0, nbuf = 0;
  void * volatile pParser = LemonPotionAlloc(P);
  PN last = PN_NIL;
  PN sbuf = potion_bytes(P, 4096);

  P->dast = 0;
  P->xast = 1;
  P->yerror = -1;
  P->source = PN_NIL;
  nl = p = PN_STR_PTR(code);
  eof = pe = p + PN_STR_LEN(code) + 1;

  %% write init;
  %% write exec;

  LemonPotion(pParser, 0, 0, P);

  // TODO: figure out why a closing newline is throwing a ragel error
  if (cs == potion_error) {
    char sample[potion_sample_len + 6];
    lineno++;
    if (P->yerror >= 0) {
      int yerror = P->yerror;
      if (yerror & PN_TOK_MISSING) yerror ^= PN_TOK_MISSING;
      char *name = potion_token_friendly(yerror);
      if (name == NULL) name = P->yerrname;

      if (P->yerror & PN_TOK_MISSING) {
        printf("** Syntax error: missing %s.\n", name);
      } else {
        printf("** Syntax error: %s was found in the wrong place.\n", name);
      }
      printf("** Where: (line %d, character %d) %s\n",
        lineno, (int)((p - nl) + 1), potion_code_excerpt(sample, nl, p, pe));
      P->source = PN_NIL;
    } else if (*p != 0) {
      printf("** Syntax error: bad character, Potion only supports UTF-8 encoding.\n"
             "** Where: (line %d, character %d) %s\n",
        lineno, (int)((p - nl) + 1), potion_code_excerpt(sample, nl, p, pe));
      P->source = PN_NIL;
    }
  }

  last = P->source;
  P->source = PN_NIL;
  return last;
}

//
// The format of Potion method signatures (used when
// defining methods from C.)
//
// s - coerce the object to a string
// S - only string types
// n - coerce the object to a number
// N - only number types
// b - coerce to a boolean
// B - only boolean types
// m - a mixin
// t - a tuple or table
// o - anything
// O - any object but nil
// - - an IO or File type responding to `read'
// & - a closure
//

#define ARG_NEXT() cast = 0; eql = NULL

%%{
  machine signature;
  include utf8;

  action cast { cast = p[0]; }
  action name { eql = p; }

  optional = "|" >cast;

  type = ("s" | "S" | "n" | "N" | "b" | "B" | "k" |
    "t" | "o" | "O" | "-" | "&") >cast;
  sep = ".";
  key = message "=" >name;
  arg = key type | type;
  
  main := |*
    whitespace;
    arg => {
      if (eql)
        sig = PN_PUSH(PN_PUSH(PN_PUSH(sig,
          potion_str2(P, ts, eql - ts)),
          PN_NUM(cast)),
          PN_NIL);
      else
        sig = PN_PUSH(sig, PN_NUM(cast));
    };
    optional => { sig = PN_PUSH(sig, PN_NUM('|')); };
    comma => { ARG_NEXT(); };
    sep => { 
      sig = PN_PUSH(sig, PN_NUM('.'));
      ARG_NEXT();
    };
  *|;

  write data nofinal;
}%%

PN potion_sig(Potion *P, char *fmt) {
  PN sig;
  int cs, act;
  char *p, *pe, *ts, *te, *eof = 0;
  char cast = 0, *eql = NULL;

  if (fmt == NULL) return PN_NIL; // no signature, arg check off
  if (fmt[0] == '\0') return PN_FALSE; // empty signature, no args

  fmt = strdup(fmt);
  p = fmt, pe = fmt + strlen(fmt);
  sig = PN_TUP0();

  %% write init;
  %% write exec;

  free(fmt);
  return sig;
}

int potion_sig_find(Potion *P, PN cl, PN name)
{
  PN_SIZE idx = 0;
  PN sig;
  if (!PN_IS_CLOSURE(cl))
    return -1;

  if (PN_CLOSURE(cl)->extra > 0 && PN_IS_PROTO(PN_CLOSURE(cl)->data[0]))
    sig = PN_PROTO(PN_CLOSURE(cl)->data[0])->sig;
  else
    sig = PN_CLOSURE(cl)->sig;

  if (!PN_IS_TUPLE(sig))
    return -1;

  PN_TUPLE_EACH(sig, i, v, {
    if (v == PN_NUM(idx) || v == name)
      return idx;
    if (PN_IS_NUM(v))
      idx++;
  });

  return -1;
}
