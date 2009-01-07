#!/bin/sh

CC=$1
CCEX="$CC -x c - -o config.out"

TARGET=`gcc -v 2>&1 | sed -e "/Target:/b" -e "/--target=/b" -e d | sed "s/.* --target=//; s/Target: //; s/ .*//" | head -1`
MINGW_GCC=`echo "$TARGET" | sed "/mingw/!d"`
if [ "$MINGW_GCC" = "" ]; then
  MINGW=0
else
  MINGW=1
fi

if [ $MINGW -eq 0 ]; then
  LONG=`echo "#include <stdio.h>int main() { printf(\\"%d\\", (int)sizeof(long)); return 0; }" | $CCEX && ./config.out && rm -f config.out`
  INT=`echo "#include <stdio.h>int main() { printf(\\"%d\\", (int)sizeof(int)); return 0; }" | $CCEX && ./config.out && rm -f config.out`
  SHORT=`echo "#include <stdio.h>int main() { printf(\\"%d\\", (int)sizeof(short)); return 0; }" | $CCEX && ./config.out && rm -f config.out`
  CHAR=`echo "#include <stdio.h>int main() { printf(\\"%d\\", (int)sizeof(char)); return 0; }" | $CCEX && ./config.out && rm -f config.out`
  LLONG=`echo "#include <stdio.h>int main() { printf(\\"%d\\", (int)sizeof(long long)); return 0; }" | $CCEX && ./config.out && rm -f config.out`
  DOUBLE=`echo "#include <stdio.h>int main() { printf(\\"%d\\", (int)sizeof(double)); return 0; }" | $CCEX && ./config.out && rm -f config.out`
  LILEND=`echo "#include <stdio.h>int main() { short int word = 0x0001; char *byte = (char *) &word; printf(\\"%d\\", (int)byte[0]); return 0; }" | $CCEX && ./config.out && rm -f config.out`
else
  CHAR="1"
  SHORT="2"
  LONG="4"
  INT="4"
  DOUBLE="8"
  LLONG="8"
  LILEND="1"
fi

if [ "$2" = "mingw" ]; then
  if [ $MINGW -eq 0 ]; then
    echo "0"
  else
    echo "1"
  fi
elif [ "$2" = "strip" ]; then
  if [ $MINGW -eq 0 ]; then
    echo "strip -x"
  else
    echo "ls"
  fi
else
  echo "#define POTION_PLATFORM \"$TARGET\""
  echo "#define POTION_WIN32    $MINGW"
  echo
  echo "#define PN_SIZE_T     $LONG"
  echo "#define LONG_SIZE_T   $LONG"
  echo "#define DOUBLE_SIZE_T $DOUBLE"
  echo "#define INT_SIZE_T    $INT"
  echo "#define SHORT_SIZE_T  $SHORT"
  echo "#define CHAR_SIZE_T   $CHAR"
  echo "#define LONGLONG_SIZE_T   $LLONG"
  echo "#define PN_LITTLE_ENDIAN  $LILEND"
fi
