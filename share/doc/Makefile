#	From: @(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: projects/armv6/share/doc/Makefile 226747 2011-10-25 19:47:28Z cognet $

.include <bsd.own.mk>

SUBDIR=	${_bind9} IPv6 legal ${_llvm} ${_roffdocs}

.if ${MK_BIND} != "no"
_bind9=	bind9
.endif

.if ${MK_CLANG} != "no"
_llvm=	llvm
.endif

# FIXME this is not a real solution ...
.if ${MK_GROFF} != "no"
_roffdocs=	papers psd smm usd
.endif

# Default output format for troff documents is ascii.
# To generate postscript versions of troff documents, use:
#  make PRINTERDEVICE=ps

.include <bsd.subdir.mk>
