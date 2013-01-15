PREFIX ?= /usr/local
ECHO = /bin/echo

VERSION   = $(shell ./tools/config.sh ${CC} version)
P2VERSION = $(shell ./tools/config.sh ${CC} p2version)
REVISION  = $(shell git rev-list --abbrev-commit HEAD | wc -l | sed "s/ //g")
RELEASE   ?= ${VERSION}.${REVISION}
P2RELEASE ?= ${P2VERSION}.${REVISION}
PKG       = potion-${RELEASE}
P2PKG     = p2-${P2RELEASE}

dist: bin-dist src-dist

install: bin-dist
	sudo tar xfz pkg/${PKG}.tar.gz -C $(PREFIX)/

bin-dist: pkg/${PKG}.tar.gz

pkg/${PKG}.tar.gz: core/config.h core/version.h core/syntax.c potion libpotion.a libpotion.so \
  lib/readline/readline.so
	rm -rf dist
	mkdir -p dist dist/bin dist/include/potion dist/lib/potion dist/share/potion/doc \
	  dist/share/potion/example
	cp core/*.h dist/include/potion/
	cp potion dist/bin/
	cp libpotion.a dist/lib/
	cp libpotion.so dist/lib/
	cp lib/readline/readline.so dist/lib/potion/
	cp doc/* dist/share/potion/doc/
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
	cp core/syntax.c ${PKG}/core/
	tar czvf pkg/${PKG}-src.tar.gz ${PKG}
	rm -rf ${PKG}

.PHONY: all config clean doc rebuild test bench tarball src-dist bin-dist dist
