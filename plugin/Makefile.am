# $Id: Makefile.am,v 1.12 2003/12/26 21:37:00 jasta Exp $
###############################################################################

SUBDIRS =

AM_CFLAGS = \
	$(GIFT_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)

lib_LTLIBRARIES = libgiftproto.la

libgiftproto_la_SOURCES = \
	protocol.c    protocol.h \
	share.c       share.h \
	share_hash.c  share_hash.h

libgiftproto_la_LDFLAGS = \
	$(GIFT_LDFLAGS)

libgiftproto_la_LIBADD = \
	$(GIFT_LIBS) \
	$(top_builddir)/lib/libgift.la

pkgincludedir = $(libgiftincdir)/proto
pkginclude_HEADERS = \
	protocol.h \
	protocol_ver.h \
	share.h \
	share_hash.h \
	transfer_api.h \
	if_event_api.h

EXTRA_DIST = \
	Makefile.msvc
