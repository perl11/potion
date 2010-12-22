#include <stdlib.h>
#include <stdio.h>
#include <potion.h>
#include <readline/readline.h>

PN pn_readline(Potion *P, PN cl, PN self, PN start) {
  char *line = readline(PN_STR_PTR(start));
  PN r;
  if (line) {
    add_history(line);
    r = potion_str(P, line);
    free(line);
    return r;
  }
  return PN_NIL;
}

void Potion_Init_readline(Potion *P) {
  potion_method(P->lobby, "readline", pn_readline, "start=S");
}