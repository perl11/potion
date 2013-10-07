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

VERSION  = $(shell ./tools/config.sh "${CC}" p2version)
PLATFORM = $(shell ./tools/config.sh "${CC}" target)
RELEASE ?= ${VERSION}.${REVISION}
PKG      = p2-${RELEASE}
PKGBIN   = ${PKG}-${PLATFORM}

dist: bin-dist src-dist

release: dist

install: bin-dist
	${SUDO} tar xfz pkg/${PKGBIN}.tar.gz -C $(PREFIX)/

bin-dist: pkg/${PKGBIN}.tar.gz pkg/${PKGBIN}-devel.tar.gz

pkg/${PKGBIN}.tar.gz: core/config.h core/version.h bin/potion${EXE} bin/p2${EXE}  \
  lib/libp*${DLL} lib/potion/*${LOADEXT} lib/potion/libsyntax*${DLL}
	rm -rf dist
	mkdir -p dist dist/bin dist/include/potion dist/lib/potion \
                 dist/share/potion/doc dist/share/potion/example
	cp bin/potion${EXE}            dist/bin/
	cp bin/p2${EXE}                dist/bin/
	cp lib/libp*${DLL}             dist/lib/
	cp lib/potion/*${LOADEXT}      dist/lib/potion/
	cp lib/potion/libsyntax*${DLL} dist/lib/potion/
	if [ ${WIN32} = 1 ]; then mv dist/lib/*.dll dist/bin/; fi
	cp core/potion.h               dist/include/potion/
	cp core/config.h               dist/include/potion/
	-cp doc/*.html doc/*.png       dist/share/potion/doc/
	-cp doc/core-files.txt         dist/share/potion/doc/
	-cp README COPYING LICENSE README.potion   dist/share/potion/doc/
	cp example/*                   dist/share/potion/example/
	-mkdir -p pkg
	(cd dist && tar czf ../pkg/${PKGBIN}.tar.gz * && cd ..)
	rm -rf dist

pkg/${PKGBIN}-devel.tar.gz: ${GREG} lib/libpotion.a lib/libp2.a bin/p2-s${EXE} bin/potion-s${EXE}
	+${MAKE} doxygen GTAGS
	rm -rf dist
	mkdir -p dist dist/bin dist/include/potion dist/lib/potion \
                 dist/share/potion/doc/ref dist/share/potion/example
	cp syn/greg${EXE}               dist/bin/
	cp bin/p*-s${EXE}               dist/bin/
	cp lib/libp*.a                  dist/lib/
	cp core/*.h                     dist/include/potion/
	rm dist/include/potion/potion.h dist/include/potion/config.h
	-cp -r doc/*.textile doc/html   dist/share/potion/doc/
	-cp -r doc/latex I*.md          dist/share/potion/doc/
	-cp -r doc/ref/*                dist/share/potion/doc/ref/
	-mkdir -p pkg
	(cd dist && tar czf ../pkg/${PKGBIN}-devel.tar.gz * && cd ..)
	rm -rf dist

src-dist: pkg/${PKG}-src.tar.gz

pkg/${PKG}-src.tar.gz: tarball

#TODO: you should be able to build without git
tarball: core/version.h syn/syntax.c
	-mkdir -p pkg
	rm -rf ${PKG}
	git checkout-index --prefix=${PKG}/ -a
	rm -f ${PKG}/.gitignore
	+${MAKE} MANIFEST
	cp MANIFEST ${PKG}/
	cp core/version.h ${PKG}/core/
	cp syn/syntax*.c ${PKG}/syn/
	tar czf pkg/${PKG}-src.tar.gz ${PKG}
	rm -rf ${PKG}

GTAGS: ${SRC} core/*.h
	rm -rf ${PKG} doc/ref
	git checkout-index --prefix=${PKG}/ -a
	-cp core/version.h ${PKG}/core/
	cd ${PKG} && \
	  mv syn/greg.c syn/greg-c.tmp && \
	  gtags && htags && \
	  sed -e's,background-color: #f5f5dc,background-color: #ffffff,' < HTML/style.css > HTML/style.new && \
	  mv HTML/style.new HTML/style.css && \
	  mv syn/greg-c.tmp syn/greg.c && \
	  cd ..  && \
	  mv ${PKG}/HTML ${PKG}/ref && \
	  mv ${PKG}/ref doc/
	rm -rf ${PKG}

.PHONY: dist release install tarball src-dist bin-dist
