#!/bin/sh
# helper to generate config.inc and core/config.h

AC="tools/config.c"
AOUT="tools/config.out"
if [ -z "$CC" ]; then
    if [ -z "$1" -o "$1" = "compiler" ]; then
        case `uname -s` in
            *Linux) CC=clang ;;
            *) CC=cc ;;
        esac
    else
        CC="$1"
    fi
fi

CCEX="$CC $AC -o $AOUT"
LANG=C

CCv=`$CC -v 2>&1`
ICC=`echo "$CCv" | sed "/icc version/!d"`
TARGET=`echo "$CCv" | sed -e "/Target:/b" -e "/--target=/b" -e d | sed "s/.* --target=//; s/Target: //; s/ .*//" | head -1`
MINGW_GCC=`echo "$TARGET" | sed "/mingw/!d"`
if [ "$MINGW_GCC" = "" ]; then MINGW=0
else MINGW=1; fi
CYGWIN=`echo "$TARGET" | sed "/cygwin/!d"`
if [ "$CYGWIN" = "" ]; then CYGWIN=0
else CYGWIN=1; fi
BSD=`echo "$TARGET" | sed "/bsd/!d"`
JIT_X86=`echo "$TARGET" | sed "/86/!d"`
JIT_PPC=`echo "$TARGET" | sed "/powerpc/!d"`
JIT_I686=`echo "$TARGET" | sed "/i686/!d"`
JIT_AMD64=`echo "$TARGET" | sed "/amd64/!d"`
JIT_ARM=`echo "$TARGET" | sed "/arm/!d"`
JIT_X86_64=`echo "$TARGET" | sed "/x86_64/!d"`
if [ "$ICC" != "" ]; then
    TARGET=`$CC -V 2>&1 | sed -e "/running on/b" -e "s,.*running on,," -e d`
    JIT_PPC=`echo "$TARGET" | sed "/powerpc/!d"`
    JIT_X86=`echo "$TARGET" | sed "/IA-32/!d"`
    JIT_AMD64=`echo "$TARGET" | sed "/Intel(R) 64/!d"`
    JIT_X86_64=`echo "$TARGET" | sed "/Intel(R) 64/!d"`
    JIT_ARM=`echo "$TARGET" | sed "/arm/!d"`
fi
CROSS=0

if [ $MINGW -eq 0 ]; then
  EXE=""
  LIBEXT=".a"
  if [ "$CYGWIN" != "1" ]; then
    LOADEXT=".so"
    DLL=".so"
  else
    EXE=".exe"
    LOADEXT=".dll"
    DLL=".dll"
  fi
  OSX=`echo "$TARGET" | sed "/apple/!d"`
  if [ "$OSX" != "" ]; then
    OSX=1
    LOADEXT=".bundle"
    DLL=".dylib"
  else
    OSX=0
  fi
else
  EXE=".exe"
  LOADEXT=".dll"
  DLL=".dll"
  LIBEXT=".a"
  case `uname -s` in
      *Linux|CYGWIN*|Darwin) CROSS=1 ;;
      *) CROSS=0 ;;
  esac
fi

if [ "$1" = "compiler" ]; then
  echo $CC
elif [ "$2" = "mingw" ]; then
  if [ $MINGW -eq 0 ]; then echo "0"
  else echo "1"; fi
elif [ "$2" = "apple" ]; then
  if [ $OSX -eq 0 ]; then echo "0"
  else echo "1"; fi
elif [ "$2" = "cygwin" ]; then
  if [ $CYGWIN -eq 0 ]; then echo "0"
  else echo "1"; fi
elif [ "$2" = "clang" ]; then
  CLANG=`echo "$CCv" | sed "/clang/!d"`
  if [ "$CLANG" = "" ]; then echo "0"
  else echo "1"; fi
elif [ "$2" = "gcc" ]; then
  GCC=`echo "$CCv" | sed "/^gcc version/!d"`
  if [ "$GCC" = "" ]; then echo "0"
  else echo "1"; fi
elif [ "$2" = "icc" ]; then
  if [ "$ICC" = "" ]; then echo "0"
  else echo "1"; fi
elif [ "$2" = "bsd" ]; then
  if [ "$BSD" = "" ]; then echo "0"
  else echo "1"; fi
elif [ "$2" = "version" ]; then
  sed "/POTION_VERSION/!d; s/\\\"$//; s/.*\\\"//" < core/potion.h
elif [ "$2" = "target" ]; then
  if [ "$CC" = "gcc -m32" ]; then
      echo "$TARGET" | sed -e "s,x86_64,i686,; s,-unknown,,"
  else
      echo "$TARGET" | sed -e"s,-unknown,,"
  fi
elif [ "$2" = "cross" ]; then
    echo $CROSS
elif [ "$2" = "jit" ]; then
  if [ "$JIT_X86$MINGW_GCC" != "" -o "$JIT_I686" != "" -o "$JIT_AMD64" != "" ]; then
    echo "X86"
  elif [ "$JIT_PPC" != "" ]; then
    echo "PPC"
  elif [ "$JIT_ARM" != "" ]; then
    echo "ARM"
  fi
elif [ "$2" = "strip" ]; then
  if [ $MINGW -eq 0 ]; then
    if [ $CYGWIN -eq 0 ]; then
      echo "strip -x"
    else
      echo "strip"
    fi
  else
    if [ $CROSS -eq 1 ]; then
      echo "$CC" | sed -e"s,-gcc,-strip,"
    else
      echo "echo"
    fi
  fi
elif [ "$2" = "lib" ]; then
  prefix=$5
  if [ -n $prefix ]; then
    CC="$CC -I$prefix/include -L$prefix/lib"
    CCEX="$CC $AC -o $AOUT"
  fi
  LIBOUT=`echo "#include <stdio.h>#include \\"$4\\"int main() { puts(\\"1\\"); return 0; }" > $AC && $CCEX $3 2>/dev/null && $AOUT; rm -f $AOUT`
  echo $LIBOUT
else
  if [ $MINGW -eq 0 ]; then
      LONG=`echo "#include <stdio.h>int main() { printf(\\"%d\\", (int)sizeof(long)); return 0; }" > $AC && $CCEX && $AOUT && rm -f $AOUT`
      INT=`echo "#include <stdio.h>int main() { printf(\\"%d\\", (int)sizeof(int)); return 0; }" > $AC && $CCEX && $AOUT && rm -f $AOUT`
      SHORT=`echo "#include <stdio.h>int main() { printf(\\"%d\\", (int)sizeof(short)); return 0; }" > $AC && $CCEX && $AOUT && rm -f $AOUT`
      CHAR=`echo "#include <stdio.h>int main() { printf(\\"%d\\", (int)sizeof(char)); return 0; }" > $AC && $CCEX && $AOUT && rm -f $AOUT`
      LLONG=`echo "#include <stdio.h>int main() { printf(\\"%d\\", (int)sizeof(long long)); return 0; }" > $AC && $CCEX && $AOUT && rm -f $AOUT`
      DOUBLE=`echo "#include <stdio.h>int main() { printf(\\"%d\\", (int)sizeof(double)); return 0; }" > $AC && $CCEX && $AOUT && rm -f $AOUT`
      LILEND=`echo "#include <stdio.h>int main() { short int word = 0x0001; char *byte = (char *) &word; printf(\\"%d\\", (int)byte[0]); return 0; }" > $AC && $CCEX && $AOUT && rm -f $AOUT`
      PAGESIZE=`echo "#include <stdio.h>#include <unistd.h>int main() { printf(\\"%d\\", (int)sysconf(_SC_PAGE_SIZE)); return 0; }" > $AC && $CCEX && $AOUT && rm -f $AOUT`
      #This depends on CFLAGS: -O3 => 1 vs -O0 => -1
      if [ "$JIT_X86$MINGW_GCC" != "" -o "$JIT_I686" != "" -o "$JIT_AMD64" != "" ]; then
        # override icc optimization
        STACKDIR="-1"
      else
        STACKDIR=`echo "#include <stdlib.h>#include <stdio.h>void a2(int *a, int b, int c) { printf(\\"%d\\", (int)((&b - a) / abs(&b - a))); }void a1(int a) { a2(&a,a+4,a+2); }int main() { a1(9); return 0; }" > $AC && $CCEX && $AOUT && rm -f $AOUT`
      fi
      #ARGDIR=`echo "#include <stdio.h>void a2(int *a, int b, int c) { printf(\\"%d\\", (int)(&c - &b)); }void a1(int a) { a2(&a,a+4,a+2); }int main() { a1(9); return 0; }" > $AC && $CCEX && $AOUT && rm -f $AOUT`
      HAVE_ASAN=`echo "#include <stdio.h>__attribute__((no_address_safety_analysis)) int main() { puts(\\"1\\"); return 0; }" > $AC && $CCEX -Werror $3 2>&1 && $AOUT && rm -f $AOUT`
    if [ "$HAVE_ASAN" = "1" ]; then HAVE_ASAN=1; else HAVE_ASAN=0; fi
  else
      # hard coded win32 values
      if [ "$JIT_X86_64" != "" -o "$JIT_AMD64" != "" ]; then
        LONG="8"
      else
        LONG="4"
      fi
      INT="4"
      SHORT="2"
      CHAR="1"
      DOUBLE="8"
      LLONG="8"
      LILEND="1"
      # in fact the AllocationGranularity=65536, and PageSize=4096
      if [ "$LONG" = "8" ]; then
        PAGESIZE="65536"
        #ARGDIR="1"
      else
        PAGESIZE="4096"
        #ARGDIR="1"
      fi
      STACKDIR="-1"
      HAVE_ASAN="0"
  fi

  echo "#define POTION_PLATFORM   \"$TARGET\""
  echo "#define POTION_LIBEXT     \"$LIBEXT\""
  echo
  echo "#define PN_SIZE_T         $LONG"
  echo "#define LONG_SIZE_T       $LONG"
  echo "#define DOUBLE_SIZE_T     $DOUBLE"
  echo "#define INT_SIZE_T        $INT"
  echo "#define SHORT_SIZE_T      $SHORT"
  echo "#define CHAR_SIZE_T       $CHAR"
  echo "#define LONGLONG_SIZE_T   $LLONG"
  echo "#define PN_LITTLE_ENDIAN  $LILEND"
  echo "#define POTION_PAGESIZE   $PAGESIZE"
  echo "#define POTION_STACK_DIR  $STACKDIR"
  #echo "#define POTION_ARGS_DIR   $ARGDIR"
  echo "#define HAVE_ASAN_ATTR    $HAVE_ASAN"
fi
