# -*- makefile -*-
include config.inc

SUDO = sudo
GREG = bin/greg${EXE}

ifeq (${PREFIX},)
	$(error need to make config first)
endif
ifeq (${DLL},)
	$(error need to make config first)
endif

VERSION   = $(shell ./tools/config.sh "${CC}" version)
PLATFORM  = $(shell ./tools/config.sh "${CC}" target)
RELEASE  ?= ${VERSION}.${REVISION}
PKG       = potion-${RELEASE}
PKGBIN    = ${PKG}-${PLATFORM}
ifeq (${WIN32},1)
BINDIST   = pkg/${PKGBIN}.zip
else
BINDIST   = pkg/${PKGBIN}.tar.gz pkg/${PKGBIN}-devel.tar.gz
endif

dist: bin-dist src-dist

release: dist

install: bin-dist
	${SUDO} tar xfz pkg/${PKGBIN}.tar.gz -C $(PREFIX)/

bin-dist: ${BINDIST}

pkg/${PKGBIN}.tar.gz: core/config.h core/version.h bin/potion${EXE} \
                      lib/libpotion${DLL} lib/potion/readline${LOADEXT}
	rm -rf dist
	mkdir -p dist dist/bin dist/include/potion dist/lib/potion \
                 dist/share/potion/doc dist/share/potion/example
	cp bin/potion${EXE}            dist/bin/
	cp lib/libpotion${DLL}         dist/lib/
	cp -r lib/potion/              dist/lib/potion/
	if [ ${WIN32} = 1 ]; then mv dist/lib/*.dll dist/bin/; fi
	cp core/potion.h               dist/include/potion/
	cp core/config.h               dist/include/potion/
	-cp doc/*.html doc/*.png       dist/share/potion/doc/
	-cp doc/core-files.txt         dist/share/potion/doc/
	-cp README COPYING LICENSE     dist/share/potion/doc/
	cp example/*                   dist/share/potion/example/
	-mkdir -p pkg
	(cd dist && tar czf ../pkg/${PKGBIN}.tar.gz * && cd ..)
	rm -rf dist

pkg/${PKGBIN}.zip: core/config.h core/version.h core/syntax.c bin/potion${EXE} \
                   lib/libpotion${DLL} lib/libuv.dll.a lib/potion/readline${LOADEXT}
	rm -rf dist
	mkdir -p dist dist/lib dist/include/potion dist/lib/potion dist/doc dist/example dist/test/
	cp bin/potion${EXE}            dist/
	cp ${GREG}                     dist/
	cp lib/libpotion${DLL}         dist/
	cp lib/libuv-*.dll             dist/
	cp lib/libuv.dll.a             dist/lib
	cp -r lib/potion/*             dist/lib/potion/
	cp core/potion.h               dist/include/potion/
	cp core/config.h               dist/include/potion/
	-cp doc/*.html doc/*.png       dist/doc/
	-cp -r doc/html doc/ref        dist/doc/
	-cp doc/core-files.txt         dist/doc/
	-cp README COPYING LICENSE     dist/doc/
	cp example/*                   dist/example/
	cp -r test/*                   dist/test/
	-mkdir -p pkg
	(cd dist && zip ../pkg/${PKGBIN}.zip -rm * && cd ..)
	rm -rf dist

pkg/${PKGBIN}-devel.tar.gz: ${GREG} bin/potion-s${EXE} lib/libpotion.a
	+${MAKE} doxygen GTAGS
	rm -rf dist
	mkdir -p dist dist/bin dist/include/potion dist/lib/potion \
                 dist/share/potion/doc/ref dist/share/potion/test
	cp ${GREG}                      dist/bin/
	cp bin/potion-s${EXE}           dist/bin/
	cp lib/libpotion.a              dist/lib/
	cp core/*.h                     dist/include/potion/
	rm dist/include/potion/potion.h dist/include/potion/config.h
	-cp -r doc/*.textile doc/html   dist/share/potion/doc/
	-cp -r doc/latex I*.md          dist/share/potion/doc/
	-cp -r doc/ref/*                dist/share/potion/doc/ref/
	cp -r test/*                    dist/share/potion/test/
	-mkdir -p pkg
	(cd dist && tar czf ../pkg/${PKGBIN}-devel.tar.gz * && cd ..)
	rm -rf dist

src-dist: pkg/${PKG}-src.tar.gz

pkg/${PKG}-src.tar.gz: tarball

#TODO: you should be able to build without git
tarball: core/version.h core/syntax.c
	-mkdir -p pkg
	rm -rf ${PKG}
	git checkout-index --prefix=${PKG}/ -a
	rm -f ${PKG}/.gitignore
	+${MAKE} MANIFEST
	cp MANIFEST ${PKG}/
	cp core/version.h ${PKG}/core/
	cp core/syntax.c ${PKG}/core/
	tar czf pkg/${PKG}-src.tar.gz ${PKG}
	rm -rf ${PKG}

GTAGS: ${SRC} core/*.h
	rm -rf ${PKG} doc/ref
	git checkout-index --prefix=${PKG}/ -a
	-cp core/version.h ${PKG}/core/
	cd ${PKG} && \
	  mv tools/greg.c tools/greg-c.tmp && \
	  gtags && htags && \
	  sed -e's,background-color: #f5f5dc,background-color: #ffffff,' < HTML/style.css > HTML/style.new && \
	  mv HTML/style.new HTML/style.css && \
	  mv tools/greg-c.tmp tools/greg.c && \
	  cd ..  && \
	  mv ${PKG}/HTML ${PKG}/ref && \
	  mv ${PKG}/ref doc/
	rm -rf ${PKG}

.PHONY: dist release install tarball src-dist bin-dist
