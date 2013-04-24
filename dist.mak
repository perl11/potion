# -*- makefile -*-
include config.inc

SUDO = sudo

ifeq (${PREFIX},)
	$(error need to make config first)
endif
ifeq (${DLL},)
	$(error need to make config first)
endif

VERSION  = $(shell ./tools/config.sh ${CC} p2version)
PLATFORM = $(shell ./tools/config.sh ${CC} target)
RELEASE ?= ${VERSION}.${REVISION}
PKG      = p2-${RELEASE}
PKGBIN   = ${PKG}-${PLATFORM}

dist: bin-dist src-dist

release: dist

install: bin-dist
	${SUDO} tar xfz pkg/${PKGBIN}.tar.gz -C $(PREFIX)/

bin-dist: pkg/${PKGBIN}.tar.gz

pkg/${PKGBIN}.tar.gz: core/config.h core/version.h syn/syntax-p5.c \
  potion${EXE} p2${EXE} doc \
  libpotion.a libpotion${DLL} libp2.a libp2${DLL} lib/readline${LOADEXT}
	rm -rf dist
	mkdir -p dist dist/bin dist/include/potion dist/lib/potion \
                 dist/share/potion/doc dist/share/potion/example
	cp core/*.h                    dist/include/potion/
	cp potion${EXE}                dist/bin/
	cp p2${EXE}                    dist/bin/
	cp libp2.a libp2${DLL}         dist/lib/
	cp libpotion.a libpotion${DLL} dist/lib/
	cp lib/readline${LOADEXT}      dist/lib/potion/
	-cp doc/* README COPYING       dist/share/potion/doc/
	-cp README.p2 I*.md            dist/share/potion/doc/
	cp example/*                   dist/share/potion/example/
	-mkdir -p pkg
	(cd dist && tar czvf ../pkg/${PKGBIN}.tar.gz * && cd ..)
	rm -rf dist

src-dist: pkg/${PKG}-src.tar.gz

pkg/${PKG}-src.tar.gz: tarball

tarball: core/version.h syn/syntax.c
	-mkdir -p pkg
	rm -rf ${PKG}
	git checkout-index --prefix=${PKG}/ -a
	rm -f ${PKG}/.gitignore
	cp core/version.h ${PKG}/core/
	cp syn/syntax*.c ${PKG}/syn/
	tar czvf pkg/${PKG}-src.tar.gz ${PKG}
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
