##
## Copyright (c) 2010 Alexey Smirnov <alexey.smirnov@gmx.com>
##
## See the file LICENSE for information on usage and redistribution
## of this file, and for a DISCLAMER OF ALL WARRANTIES.
##

AUTOMAKE_OPTIONS = foreign
EXTRA_DIST = INSTALL LICENSE

SUBDIRS = src

realclean: distclean
	@rm -fr *~ autom4te.cache/ m4/ config.h.in configure
	@rm -f aclocal.m4 install-sh missing depcomp
	@find $(top_srcdir) -name Makefile.in -exec rm -f {} \;
