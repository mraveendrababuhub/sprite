#
# Prototype Makefile for machine-dependent directories.
#
# A file of this form resides in each ".md" subdirectory of a
# command.  Its name is typically "md.mk".  During makes in the
# parent directory, this file (or a similar file in a sibling
# subdirectory) is included to define machine-specific things
# such as additional source and object files.
#
# This Makefile is automatically generated.
# DO NOT EDIT IT OR YOU MAY LOSE YOUR CHANGES.
#
# Generated from /sprite/lib/mkmf/Makefile.md
# Sun Apr 26 18:31:43 PDT 1992
#
# For more information, refer to the mkmf manual page.
#
# $Header: /sprite/lib/mkmf/RCS/Makefile.md,v 1.6 90/03/12 23:28:42 jhh Exp $
#
# Allow mkmf

SRCS		= fgrep.c kwset.c obstack.c std.c
HDRS		= kwset.h obstack.h std.h unix.h
MDPUBHDRS	= 
OBJS		= ds3100.md/fgrep.o ds3100.md/kwset.o ds3100.md/obstack.o ds3100.md/std.o
CLEANOBJS	= ds3100.md/fgrep.o ds3100.md/kwset.o ds3100.md/obstack.o ds3100.md/std.o
INSTFILES	= Makefile local.mk
SACREDOBJS	= 
