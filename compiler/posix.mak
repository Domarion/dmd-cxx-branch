INSTALL_DIR=$(PWD)/../install
ECTAGS_LANGS = Make,C,C++,Sh
ECTAGS_FILES = src/*.(cpp|hpp) src/backend/*.(cpp|hpp) src/root/*.(cpp|hpp) src/tk/*.(cpp|hpp)

.PHONY: all clean test install auto-tester-build auto-tester-test

all:
	$(QUIET)$(MAKE) -C src -f posix.mak

auto-tester-build:
	$(QUIET)$(MAKE) -C src -f posix.mak auto-tester-build

auto-tester-test: test

clean:
	$(QUIET)$(MAKE) -C src -f posix.mak clean
	$(QUIET)$(MAKE) -C test -f Makefile clean
	$(RM) tags

test:
	$(QUIET)$(MAKE) -C test -f Makefile

# Creates Exuberant Ctags tags file
tags: posix.mak $(ECTAGS_FILES)
	ctags --sort=yes --links=no --excmd=number --languages=$(ECTAGS_LANGS) \
		--langmap='C++:+.cpp,C++:+.hpp' --extra=+f --file-scope=yes --fields=afikmsSt --totals=yes posix.mak $(ECTAGS_FILES)

install: all
	$(MAKE) INSTALL_DIR=$(INSTALL_DIR) -C src -f posix.mak install
	cp -r samples $(INSTALL_DIR)
	mkdir -p $(INSTALL_DIR)/man
	cp -r docs/man/* $(INSTALL_DIR)/man/

.DELETE_ON_ERROR: # GNU Make directive (delete output files on error)
