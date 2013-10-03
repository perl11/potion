#!/bin/sh
echo '<hr class="footer"/><address class="footer"><small>'
echo Generated on `date` for `bin/potion --version|cut -f1-3 -d' '`
echo ') by <a href="http://www.doxygen.org/index.html">doxygen</a> ' `doxygen --version`
echo '</small></address></body></html>'
