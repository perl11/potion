# -*- makefile -*-
include config.inc

SUDO = sudo

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

dist: bin-dist src-dist

release: dist

install: bin-dist
	${SUDO} tar xfz pkg/${PKGBIN}.tar.gz -C $(PREFIX)/

bin-dist: pkg/${PKGBIN}.tar.gz pkg/${PKGBIN}-devel.tar.gz

pkg/${PKGBIN}.tar.gz: core/config.h core/version.h core/syntax.c potion${EXE} \
  libpotion.a libpotion${DLL} lib/readline${LOADEXT}
	rm -rf dist
	mkdir -p dist dist/bin dist/include/potion dist/lib/potion dist/share/potion/doc \
                 dist/share/potion/example
	cp potion${EXE}                dist/bin/
	cp libpotion.a                 dist/lib/
	cp libpotion${DLL}             dist/lib/
	if [ ${WIN32} = 1 ]; then mv dist/lib/*.dll dist/bin/; fi
	cp lib/readline${LOADEXT}      dist/lib/potion/
	cp core/potion.h               dist/include/potion/
	cp core/config.h               dist/include/potion/
	-cp doc/*.html doc/*.png       dist/share/potion/doc/
	-cp doc/core-files.txt         dist/share/potion/doc/
	-cp *.md README COPYING LICENSE dist/share/potion/doc/
	cp example/*                   dist/share/potion/example/
	-mkdir -p pkg
	(cd dist && tar czf ../pkg/${PKGBIN}.tar.gz * && cd ..)
	rm -rf dist

pkg/${PKGBIN}-devel.tar.gz: tools/greg${EXE} potion-s${EXE}
	+${MAKE} GTAGS
	rm -rf dist
	mkdir -p dist dist/bin dist/include/potion dist/lib/potion \
                 dist/share/potion/doc/ref
	cp tools/greg${EXE}             dist/bin/
	cp potion-s${EXE}               dist/bin/
	cp core/*.h                     dist/include/potion/
	rm dist/include/potion/potion.h dist/include/potion/config.h
	-cp -r doc/*.textile doc/html   dist/share/potion/doc/
	-cp -r doc/latex I*.md          dist/share/potion/doc/
	-cp -r HTML/*                   dist/share/potion/doc/ref/
	-mkdir -p pkg
	(cd dist && tar czf ../pkg/${PKGBIN}-devel.tar.gz * && cd ..)
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
	tar czf pkg/${PKG}-src.tar.gz ${PKG}
	rm -rf ${PKG}

GTAGS: ${SRC} core/*.h
	rm -rf ${PKG} HTML
	git checkout-index --prefix=${PKG}/ -a
	-cp core/version.h ${PKG}/core/
	cd ${PKG} && \
	  mv tools/greg.c tools/greg-c.tmp && \
	  gtags && htags && \
	  sed -e's,background-color: #f5f5dc,background-color: #ffffff,' < HTML/style.css > HTML/style.new && \
	  mv HTML/style.new HTML/style.css && \
	  mv tools/greg-c.tmp tools/greg.c && \
	  cd ..  && \
	  mv ${PKG}/HTML .
	rm -rf ${PKG}

.PHONY: dist release install tarball src-dist bin-dist
