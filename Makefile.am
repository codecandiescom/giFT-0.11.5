# $Id: Makefile.am,v 1.34 2003/12/26 21:37:00 jasta Exp $
###############################################################################

SUBDIRS = \
	data \
	doc \
	etc \
	lib \
	m4 \
	plugin \
	src \
	win32

man_MANS = \
	giftd.1

EXTRA_DIST = \
	HACKING \
	Make.msvc \
	Makefile.msvc \
	QUICKSTART \
	autogen.sh \
	gift.spec \
	$(man_MANS)

setupdir = $(prefix)/bin
setup_SCRIPTS = gift-setup

DISTCLEANFILES = \
	gconfdefs.h

###############################################################################

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
