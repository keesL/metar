# $Id: Makefile.am,v 1.1.1.1 2005/01/15 10:33:34 kees-guest Exp $

man_MANS = metar.1

CLEANFILES = metar.1
DISTCLEANFILES = $(CLEANFILES)

EXTRA_DIST = metar.pod metar.1

metar.1: metar.pod
	pod2man --release="$(PACKAGE) $(VERSION)" --center=metar $< $@
