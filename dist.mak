# -*- makefile -*-
include config.inc

SUDO = sudo

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

pkg/${PKGBIN}.tar.gz: bin/potion${EXE} bin/p2${EXE} doc \
  lib/potion/libp*${DLL} lib/potion/*${LOADEXT} lib/potion/libsyntax*${DLL}
	rm -rf dist
	mkdir -p dist dist/bin dist/include/potion dist/lib/potion \
                 dist/share/potion/doc dist/share/potion/example
	cp bin/potion${EXE}            dist/bin/
	cp bin/p2${EXE}                dist/bin/
	cp lib/potion/readline${LOADEXT} dist/lib/potion/
	cp lib/potion/lib*${DLL}       dist/lib/potion/
	if [ ${WIN32} = 1 ]; then mv dist/lib/potion/*.dll dist/bin/; fi
	cp core/potion.h               dist/include/potion/
	cp core/config.h               dist/include/potion/
	-cp doc/*.html doc/*.png       dist/share/potion/doc/
	-cp doc/core-files.txt         dist/share/potion/doc/
	-cp README COPYING LICENSE README.p2   dist/share/potion/doc/
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
	-cp -r doc/*.textile doc/html   dist/share/potion/doc/
	-cp -r doc/latex I*.md          dist/share/potion/doc/
	-cp -r HTML/*                   dist/share/potion/doc/ref/
	-mkdir -p pkg
	(cd dist && tar czf ../pkg/${PKGBIN}-devel.tar.gz * && cd ..)
	rm -rf dist

src-dist: pkg/${PKG}-src.tar.gz

pkg/${PKG}-src.tar.gz: tarball

tarball: core/version.h
	-mkdir -p pkg
	rm -rf ${PKG}
	git checkout-index --prefix=${PKG}/ -a
	rm -f ${PKG}/.gitignore
	cp core/version.h ${PKG}/core/
	cp syn/syntax*.c ${PKG}/syn/
	tar czf pkg/${PKG}-src.tar.gz ${PKG}
	rm -rf ${PKG}

GTAGS: ${SRC} core/*.h
	rm -rf ${PKG} HTML
	git checkout-index --prefix=${PKG}/ -a
	-cp core/version.h ${PKG}/core/
	cd ${PKG} && \
	  mv syn/greg.c syn/greg-c.tmp && \
	  gtags && htags && \
	  mv syn/greg-c.tmp syn/greg.c && \
	  cd ..  && \
	  mv ${PKG}/HTML .
	rm -rf ${PKG}

.PHONY: dist release install tarball src-dist bin-dist
