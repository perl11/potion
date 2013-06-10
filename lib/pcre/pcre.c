/** \file lib/pcre.c
  pcre bindings

  "test" match("(?:i)(abc*)|(a)")         => PNMatch (scalar context: bool, list context: results)
  "test" replace("(?:g)(abc*)|(a)", "\2") => PNString

  (c) 2013 perl.org */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "p2.h"

#include "pcre.h"
typedef struct { PNType vt; PNUniq uniq; PN_SIZE len; PN_SIZE siz; pcre *ptr; } PNMatch;
//PN_FLEX(PNMatch, struct real_pcre);
//PN_FLEX(PNMatchExtra, pcre_extra);
//PN_FLEX(PNMatchCB, pcre_callout_block);

// return true/false, utf8
static PN potion_string_boolmatch(Potion *P, PN cl, PN self, PN str) {
}
// no-utf8
static PN potion_bytes_boolmatch(Potion *P, PN cl, PN self, PN str) {
}
// return list of matches
static PN potion_string_listmatch(Potion *P, PN cl, PN self, PN str) {
}
static PN potion_bytes_listmatch(Potion *P, PN cl, PN self, PN str) {
}
// match against pre-compiled regex (result of compile method)
static PN potion_string_qr(Potion *P, PN cl, PN self, PN qr) {
}
static PN potion_bytes_qr(Potion *P, PN cl, PN self, PN qr) {
}
//\returns PNMatchExtra
static PN potion_string_study(Potion *P, PN cl, PN self) {
}
//\return copy of string
static PN potion_string_replace(Potion *P, PN cl, PN self, PN str, PN with) {
}
//\return destructive replace
static PN potion_bytes_replace(Potion *P, PN cl, PN self, PN str, PN with) {
}
//\returns PNMatch (qr)
static PN potion_string_compile(Potion *P, PN cl, PN self) {
}
//\returns i-th match
static PN potion_match_at(Potion *P, PN cl, PN self) {
}
//\returns named match
static PN potion_match_result(Potion *P, PN cl, PN self, PN str) {
}

void potion_pcre_init(Potion *P) {
  PN str_vt = PN_VTABLE(PN_TSTRING);
  PN byt_vt = PN_VTABLE(PN_TBYTES);
  //PN mat_vt = PN_VTABLE(PNMatch);
  PN mat_vt = potion_type_new2(P, PN_TUSER, PN_VTABLE(PN_TLICK), potion_str(P, "Match"));
  //PN mat_vt = PN_VTABLE(PNMatch);
  potion_method(str_vt, "match", potion_string_boolmatch, "str=S");     //match utf8 string against string
  potion_method(byt_vt, "match", potion_bytes_boolmatch, "str=S");      //on no-utf8 buffer 
  potion_method(str_vt, "listmatch", potion_string_listmatch, "str=S"); //return results as list
  potion_method(byt_vt, "listmatch", potion_bytes_listmatch, "str=S");
  potion_method(str_vt, "qr", potion_string_qr, "qr=o");  // match against pre-compiled regex
  potion_method(byt_vt, "qr", potion_bytes_qr, "qr=o");   
  potion_method(str_vt, "study", potion_string_study, 0);    //< \returns PNMatchExtra

  potion_method(str_vt, "replace", potion_string_replace, "str=S,with=S"); //copy, on utf8 string
  potion_method(byt_vt, "replace", potion_bytes_replace, "str=S,with=S");  //destructive, on no-utf8 buffer
  potion_method(str_vt, "compile", potion_string_compile, 0);          //< \returns PNMatch (qr)
  potion_type_call_is(mat_vt, PN_FUNC(potion_match_at, 0));         //< \returns i-th match
  potion_method(mat_vt, "result", potion_match_result, "str=S");    //named result
}
