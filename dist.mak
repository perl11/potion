# -*- makefile -*-
include config.inc

ifeq (${PREFIX},)
	$(error need to make config first)
endif
ifeq (${DLL},)
	$(error need to make config first)
endif

#VERSION   = $(shell ./tools/config.sh ${CC} version)
#RELEASE   ?= ${VERSION}.${REVISION}
#PKG       = potion-${RELEASE}

VERSION = $(shell ./tools/config.sh ${CC} p2version)
RELEASE ?= ${VERSION}.${REVISION}
PKG     = p2-${RELEASE}

dist: bin-dist src-dist

install: bin-dist
	sudo tar xfz pkg/${PKG}.tar.gz -C $(PREFIX)/

bin-dist: pkg/${PKG}.tar.gz

pkg/${PKG}.tar.gz: core/config.h core/version.h core/syntax-p5.c \
  potion${EXE} p2${EXE} \
  libpotion.a libpotion${DLL} libp2.a libp2${DLL} lib/readline${LOADEXT}
	rm -rf dist
	mkdir -p dist dist/bin dist/include/potion dist/lib/potion dist/share/potion/doc \
	  dist/share/potion/example
	cp core/*.h dist/include/potion/
	cp potion${EXE} dist/bin/
	cp p2${EXE} dist/bin/
	cp libp2.a libp2${DLL} dist/lib/
	cp libpotion.a libpotion${DLL} dist/lib/
	cp lib/readline${LOADEXT} dist/lib/potion/
	cp doc/* *.md README COPYING dist/share/potion/doc/
	cp example/* dist/share/potion/example/
	-mkdir -p pkg
	(cd dist && tar czvf ../pkg/${PKG}.tar.gz * && cd ..)
	rm -rf dist

src-dist: pkg/${PKG}-src.tar.gz

pkg/${PKG}-src.tar.gz: tarball

tarball: core/version.h core/syntax.c
	-mkdir -p pkg
	rm -rf ${PKG}
	git checkout-index --prefix=${PKG}/ -a
	rm -f ${PKG}/.gitignore
	cp core/version.h ${PKG}/core/
	cp core/syntax-p5.c core/syntax.c ${PKG}/core/
	tar czvf pkg/${PKG}-src.tar.gz ${PKG}
	rm -rf ${PKG}

.PHONY: dist install tarball src-dist bin-dist
