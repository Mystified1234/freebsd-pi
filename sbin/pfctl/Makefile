# $FreeBSD: projects/armv6/sbin/pfctl/Makefile 198236 2009-10-19 16:00:24Z ru $

.PATH:	${.CURDIR}/../../contrib/pf/pfctl
.PATH:	${.CURDIR}/../../sys/contrib/pf/net
.PATH:	${.CURDIR}/../../contrib/pf/man

PROG=	pfctl
MAN=	pfctl.8 pf.4 pflog.4 pfsync.4 pf.conf.5 pf.os.5

SRCS = pfctl.c parse.y pfctl_parser.c pf_print_state.c pfctl_altq.c
SRCS+= pfctl_osfp.c pfctl_radix.c pfctl_table.c pfctl_qstats.c
SRCS+= pfctl_optimize.c
SRCS+= pf_ruleset.c

WARNS?=	2
CFLAGS+= -Wall -Wmissing-prototypes -Wno-uninitialized
CFLAGS+= -Wstrict-prototypes -I${.CURDIR}/../../contrib/pf/pfctl

# XXX ALTQ
CFLAGS+= -DENABLE_ALTQ
#CFLAGS+= -I${.CURDIR}/missing

YFLAGS=

LDADD+=	-lm -lmd
DPADD+=	${LIBM} ${LIBMD}

.include <bsd.prog.mk>
