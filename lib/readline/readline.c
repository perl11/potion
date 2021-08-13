#include <stdlib.h>
#include <stdio.h>
#include <potion.h>
#include "linenoise.h"

PN pn_readline(Potion *P, PN cl, PN self, PN start) {
  char *tmp;
  char *line = linenoise(PN_STR_PTR(start, tmp));
  PN r;
  if (line == NULL) return PN_NIL;

  linenoiseHistoryLoad("history.txt");
  linenoiseHistoryAdd(line);
  linenoiseHistorySave("history.txt");
  r = potion_str(P, line);
  free(line);
  return r;
}

void Potion_Init_readline(Potion *P) {
  potion_method(P->lobby, "readline", pn_readline, "start=S");
}
