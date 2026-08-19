#ident @(#)Makefile	1.3 19/10/28 
###########################################################################
# Sample makefile for sub directory makes
###########################################################################
SRCROOT=	../..
RULESDIR=	RULES
include		$(SRCROOT)/$(RULESDIR)/rules.top
###########################################################################

DIRS=			lib/bsd/src lib/mksh/src lib/vroot/src
$(SHLIB_BUILD)DIRS +=	lib/makestate/src
DIRS +=			bin/make/common

###########################################################################
include		$(SRCROOT)/$(RULESDIR)/rules.dir
###########################################################################
