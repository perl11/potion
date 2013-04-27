#!/bin/sh
GENERATED=`date +"%a %b %d %Y"`
POTION_VERSION=`perl -ane'/POTION_VERSION/ && print $F[2]' core/potion.h`
POTION_DATE=`perl -ane'/POTION_DATE/ && print $F[2]' core/config.h`
POTION_REV=`perl -ane'/POTION_REV/ && print $F[2]' core/config.h`
DOXYGEN_VERSION=`doxygen --version`

echo '<hr class="footer"/><address class="footer"><small>'
echo "Generated on $GENERATED for potion $POTION_VERSION.$POTION_REV (date=$POTION_DATE)"
echo ') by <a href="http://www.doxygen.org/index.html">doxygen</a> ' $DOXYGEN_VERSION
echo '</small></address></body></html>'
