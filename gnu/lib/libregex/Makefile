# $FreeBSD: projects/armv6/gnu/lib/libregex/Makefile 195767 2009-07-19 17:25:24Z kensmith $

SUBDIR=		doc

LIB=		gnuregex
SHLIB_MAJOR=	5

SRCS=		gnuregex.c
INCSGROUPS=	INCS WRINCS PXINCS
INCS=		regex.h.patched
INCSNAME=	regex.h
INCSDIR=	${INCLUDEDIR}/gnu
WRINCS=		gnuregex.h
PXINCS=		posix/regex.h
PXINCSDIR=	${INCSDIR}/posix

CFLAGS+=-DHAVE_CONFIG_H -I${.CURDIR}

CLEANFILES=	regex.h.patched gnuregex.c
regex.h.patched: regex.h
	sed 's=<posix/regex\.h>=<gnu/posix/regex.h>=g' \
		< ${.ALLSRC} > ${.TARGET}

gnuregex.c: regex.c
	sed 's=<regex\.h>=<gnu/regex.h>=g' \
		< ${.ALLSRC} > ${.TARGET}

.include <bsd.lib.mk>
