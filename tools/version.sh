#/bin/sh
ECHO=${1:-echo}
if [ ! -d .git -a ! -e core/version.h ]; then
    POTION_COMMIT=
    POTION_REV=
    POTION_DATE=$(date +%Y-%m-%d)
else
    if [ ! -d .git ]; then
        exit
    else
        POTION_COMMIT=$(git rev-list HEAD -1 --abbrev=7 --abbrev-commit)
        POTION_REV=$(git rev-list --abbrev-commit HEAD | wc -l | ${SED} "s/ //g")
        POTION_DATE=$(date +%Y-%m-%d)
    fi
fi
$ECHO "/* created by ${MAKE} -f config.mak */" > core/version.h
$ECHO "#define POTION_MAJOR    ${POTION_MAJOR}" >> core/version.h
$ECHO "#define POTION_MINOR    ${POTION_MINOR}" >> core/version.h
$ECHO "#define POTION_VERSION \"${POTION_MAJOR}.${POTION_MINOR}\"" >> core/version.h
$ECHO "#define POTION_DATE    \"${POTION_DATE}\"" >> core/version.h
$ECHO "#define POTION_COMMIT  \"${POTION_COMMIT}\"" >> core/version.h
$ECHO "#define POTION_REV     \"${POTION_REV}\"" >> core/version.h
$ECHO "REVISION = ${POTION_REV}" >> config.inc
