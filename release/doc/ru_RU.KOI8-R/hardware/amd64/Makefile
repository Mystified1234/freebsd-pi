# The FreeBSD Russian Documentation Project
#
# $FreeBSD: projects/armv6/release/doc/ru_RU.KOI8-R/hardware/amd64/Makefile 143357 2005-03-10 14:15:08Z den $
# $FreeBSDru: frdp/release/doc/ru_RU.KOI8-R/hardware/amd64/Makefile,v 1.3 2005/03/10 11:19:53 den Exp $
#
# Original revision: 1.2

RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

JADEFLAGS+=	-V %generate-article-toc%

# SGML content
SRCS+=	article.sgml 
SRCS+=	proc-amd64.sgml 
SRCS+=	../common/hw.ent
SRCS+=	../common/artheader.sgml
SRCS+=	../common/dev.sgml
SRCS+=	${DEV-AUTODIR}/catalog-auto
SRCS+=	${DEV-AUTODIR}/dev-auto.sgml
SRCS+= ../../share/sgml/dev-auto-ru.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"
