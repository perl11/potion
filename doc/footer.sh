#!/bin/sh
cd bin
echo '<hr class="footer"/><address class="footer"><small>'
echo Generated on `date` for `./p2 --version|cut -f1-3 -d' '` 
echo ') by <a href="http://www.doxygen.org/index.html">doxygen</a> 1.8.1.2 '
echo '</small></address></body></html>'
cd ..
