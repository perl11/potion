//
// contrib.c
// stuff written by other folks, seen on blogs, etc. 
//
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

//
// wonderful utf-8 counting trickery
// by colin percival
//
// http://www.daemonology.net/blog/2008-06-05-faster-utf8-strlen.html
//
#define ONEMASK ((size_t)(-1) / 0xFF)

size_t
potion_cp_strlen_utf8(const char * _s)
{
  const char * s;
  size_t count = 0;
  size_t u;
  unsigned char b;

  for (s = _s; (uintptr_t)(s) & (sizeof(size_t) - 1); s++) {
    b = *s;
    if (b == '\0') goto done;
    count += (b >> 7) & ((~b) >> 6);
  }

  for (; ; s += sizeof(size_t)) {
    __builtin_prefetch(&s[256], 0, 0);
    u = *(size_t *)(s);
    if ((u - ONEMASK) & (~u) & (ONEMASK * 0x80)) break;
    u = ((u & (ONEMASK * 0x80)) >> 7) & ((~u) >> 6);
    count += (u * ONEMASK) >> ((sizeof(size_t) - 1) * 8);
  }

  for (; ; s++) {
    b = *s;
    if (b == '\0') break;
    count += (b >> 7) & ((~b) >> 6);
  }
done:
  return ((s - _s) - count);
}

#ifdef __MINGW32__
#include <windows.h>
#include <sys/unistd.h>

void *potion_mmap(size_t length, const char exec)
{ 
  void *mem = VirtualAlloc(NULL, length, MEM_COMMIT,
    exec ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE);
  if (mem == NULL)
    fprintf(stderr, "** potion_mmap failed");
  return mem;
}

int potion_munmap(void *mem, size_t len)
{
  return VirtualFree(mem, len, MEM_DECOMMIT) != 0 ? 0 : -1;
}

#else
#include <sys/mman.h>

void *potion_mmap(size_t length, const char exec)
{
  int prot = exec ? PROT_EXEC : 0;
  void *mem = mmap(NULL, length, prot|PROT_READ|PROT_WRITE,
    (MAP_PRIVATE|MAP_ANON), -1, 0);
  if (mem == MAP_FAILED) return NULL;
  return mem;
}

int potion_munmap(void *mem, size_t len)
{
  return munmap(mem, len);
}

#endif
