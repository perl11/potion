#!/bin/sh
GENERATED=`date +"%a %b %d %Y"`
P2_VERSION=`perl -ane'/P2_VERSION/ && print $F[2]' core/p2.h`
POTION_DATE=`perl -ane'/POTION_DATE/ && print $F[2]' core/config.h`
DOXYGEN_VERSION=`doxygen --version`

echo '<hr class="footer"/><address class="footer"><small>'
echo "Generated on $GENERATED for p2 $P2_VERSION (date=$POTION_DATE)"
echo 'by <a href="http://www.doxygen.org/index.html">doxygen</a> ' $DOXYGEN_VERSION
echo '</small></address></body></html>'
