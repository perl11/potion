//
// internal.h
//
// (c) 2008 why the lucky stiff, the freelance professor
//

void *LemonPotionAlloc(void *(*)(size_t));
void LemonPotion(void *, int, int, char *);
void LemonPotionFree(void *, void (*)(void*));
