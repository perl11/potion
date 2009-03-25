//
// asm.h
// some assembler macros
//
// (c) 2008 why the lucky stiff, the freelance professor
//

typedef struct {
  u8 *start, *ptr;
  long capa;
} PNAsm;

typedef struct {
  size_t from;
  PN_OP *to;
} PNJumps;

typedef struct {
  void (*setup)    (PNAsm *);
  void (*stack)    (PNAsm *, long);
  void (*registers)(PNAsm *, long);
  void (*local)    (PNAsm *, long, long);
  void (*upvals)   (PNAsm *, long, int);
  void (*op)       (PNAsm *, PN_OP *, long, PN, PN);
  void (*finish)   (PNAsm *);
} PNTarget;

#define ASM(ins) potion_asm_put(asmb, (PN)ins, sizeof(u8))
#define ASMI(pn) potion_asm_put(asmb, (PN)(pn), sizeof(int))
#define ASMN(pn) potion_asm_put(asmb, (PN)pn, sizeof(PN))

PNAsm *potion_asm_new();
void potion_asm_put(PNAsm *, PN, size_t);

void potion_x86_setup(PNAsm *);
void potion_x86_stack(PNAsm *, long);
void potion_x86_registers(PNAsm *, long);
void potion_x86_local(PNAsm *, long, long);
void potion_x86_upvals(PNAsm *, long, int);
void potion_x86_op(PNAsm *, PN_OP *, long, PN, PN);
void potion_x86_finish(PNAsm *);
