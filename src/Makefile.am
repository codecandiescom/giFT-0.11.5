# $Id: Makefile.am,v 1.83 2003/12/26 21:37:00 jasta Exp $
###############################################################################

SUBDIRS = meta

AM_CFLAGS = \
	$(GIFT_CFLAGS) \
	$(PERL_CFLAGS) \
	$(DL_CFLAGS) \
	$(IMAGEMAGICK_CFLAGS) \
	$(VORBIS_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)

bin_PROGRAMS = giftd

giftd_SOURCES = \
	daemon.c          daemon.h \
	plugin.c          plugin.h \
	download.c        download.h \
	download_state.c  download_state.h \
	if_event.c        if_event.h \
	if_port.c         if_port.h \
	if_message.c      if_message.h \
	if_search.c       if_search.h \
	if_share.c        if_share.h \
	if_stats.c        if_stats.h \
	if_transfer.c     if_transfer.h \
	giftd.c           giftd.h \
	meta.c            meta.h \
	opt.c             opt.h \
	share_cache.c     share_cache.h \
	share_db.c        share_db.h \
	share_local.c     share_local.h \
	transfer.c        transfer.h \
	upload.c          upload.h

giftd_LDADD = \
	$(top_builddir)/lib/libgift.la \
	$(top_builddir)/plugin/libgiftproto.la \
	$(top_builddir)/src/meta/libmeta.la \
	-lm \
	$(GIFT_LIBS) \
	$(DL_LIBS) \
	$(PERL_LIBS) \
	$(VORBISFILE_LIBS) \
	$(VORBIS_LIBS) \
	$(IMAGEMAGICK_LIBS)

giftd_LDFLAGS = \
	$(GIFT_LDFLAGS) \
	$(DL_LDFLAGS) \
	$(IMAGEMAGICK_LDFLAGS) \
	-export-dynamic

EXTRA_DIST = \
	Makefile.msvc
