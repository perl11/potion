//
// mingw.c
// compatibility for mingw compilation
//
// (c) 2008 why the lucky stiff, the freelance professor
//
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <sys/unistd.h>
#include "potion.h"
#include "internal.h"

void *mingw_mmap(size_t length)
{ 
  void *start;
  HANDLE handle = CreateFileMapping(0, NULL, PAGE_EXECUTE_READWRITE,
    0, length, NULL);
 
  if (handle == NULL)
    fprintf(stderr, "** mingw_mmap failed");
  
  start = MapViewOfFile(handle, FILE_MAP_ALL_ACCESS, 0, 0, length);
  CloseHandle(handle);
  
  return start;
}
