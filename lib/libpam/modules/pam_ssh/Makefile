# PAM module for SSH
# $FreeBSD: projects/armv6/lib/libpam/modules/pam_ssh/Makefile 201381 2010-01-02 09:58:07Z ed $

SSHDIR=	${.CURDIR}/../../../../crypto/openssh

LIB=	pam_ssh
MAN=	pam_ssh.8
SRCS=	pam_ssh.c

# required when linking with a dynamic libssh
SRCS+=	roaming_dummy.c

WARNS?=	3
CFLAGS+= -I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPTO} ${LIBCRYPT}
LDADD=	-lssh -lcrypto -lcrypt

.include <bsd.lib.mk>

.PATH: ${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
