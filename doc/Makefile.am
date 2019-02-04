# $Id: Makefile.am,v 1.6 2003/10/16 18:50:54 jasta Exp $
###############################################################################

SUBDIRS =

docs_IN = \
	install-unix.dbk \
	install-win32.dbk \
	interface.dbk \
	faq.dbk

docs_INSCRIPTS = \
	docpp.pl

docs_INDATA = \
	doc-html.xsl

docs_OUT = \
	$(addsuffix .html,$(basename $(docs_IN)))

EXTRA_DIST = \
	$(docs_IN) $(docs_INSCRIPTS) $(docs_INDATA) $(docs_OUT)

###############################################################################

if BUILD_DOCS

all:	$(docs_OUT)

clean:
	rm -f $(docs_OUT)

%.html:		%.dbk $(docs_INSCRIPTS) $(docs_INDATA)
	$(XSLTPROC) $(docs_INDATA) $< | $(PERL) docpp.pl $< > $@

endif
