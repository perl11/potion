#include <stdlib.h>
#include <stdio.h>
#include "p2.h"
#include "linenoise.h"

PN pn_readline(Potion *P, PN cl, PN self, PN start) {
  char *line = linenoise(PN_STR_PTR(start));
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
