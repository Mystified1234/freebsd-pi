# $FreeBSD: projects/armv6/release/doc/ja_JP.eucJP/relnotes/sparc64/Makefile 98819 2002-06-25 18:54:39Z hrs $
# Original revision: 1.1

RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

JADEFLAGS+=	-V %generate-article-toc%

# SGML content
SRCS+=	article.sgml
SRCS+=	../common/relnotes.ent
SRCS+=	../common/new.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"
