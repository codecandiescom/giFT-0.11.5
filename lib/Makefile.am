# $Id: Makefile.am,v 1.27 2003/12/26 21:37:00 jasta Exp $
###############################################################################

SUBDIRS =

AM_CFLAGS = \
	$(GIFT_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)

lib_LTLIBRARIES = libgift.la

libgift_la_SOURCES = \
	giftconfig.h \
	array.c         array.h \
	conf.c          conf.h \
	tcpc.c          tcpc.h \
	dataset.c       dataset.h \
	event.c         event.h \
	fdbuf.c         fdbuf.h \
	file.c          file.h \
	interface.c     interface.h \
	libgift.c       libgift.h \
	list.c          list.h \
	list_lock.c     list_lock.h \
	tree.c          tree.h \
	log.c           log.h \
	memory.c        memory.h \
	mime.c          mime.h \
	network.c       network.h \
	parse.c         parse.h \
	platform.c      platform.h \
	stopwatch.c     stopwatch.h \
	strobj.c        strobj.h

libgift_la_LDFLAGS = \
	$(GIFT_LDFLAGS)

libgift_la_LIBADD = \
	$(GIFT_LIBS)

pkgincludedir = $(libgiftincdir)
pkginclude_HEADERS = \
	giftconfig.h \
	array.h \
	conf.h \
	tcpc.h \
	dataset.h \
	event.h \
	fdbuf.h \
	file.h \
	interface.h \
	libgift.h \
	list.h \
	list_lock.h \
	mime.h \
	tree.h \
	log.h \
	memory.h \
	network.h \
	parse.h \
	platform.h \
	stopwatch.h \
	strobj.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgift.pc

EXTRA_DIST = \
	Makefile.msvc \
	libgift.pc.in

DISTCLEANFILES = \
	giftconfig.h \
	libgift.pc
