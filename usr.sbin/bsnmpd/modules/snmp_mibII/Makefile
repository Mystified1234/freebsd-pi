# $FreeBSD: projects/armv6/usr.sbin/bsnmpd/modules/snmp_mibII/Makefile 156085 2006-02-27 20:46:34Z ru $
#
# Author: Harti Brandt <harti@freebsd.org>

CONTRIB= ${.CURDIR}/../../../../contrib/bsnmp
.PATH: ${CONTRIB}/snmp_mibII

MOD=	mibII
SRCS=	mibII.c mibII_begemot.c mibII_ifmib.c mibII_ifstack.c		\
	mibII_interfaces.c mibII_ip.c mibII_ipaddr.c mibII_nettomedia.c	\
	mibII_rcvaddr.c mibII_route.c mibII_tcp.c mibII_udp.c
XSYM=	ipAddrTable ifTable ifRcvAddressEntry ifMIB ipMIB tcpMIB udpMIB	\
	ipForward ifIndex linkDown linkUp
MAN=	snmp_mibII.3

CFLAGS+= -I${CONTRIB}/lib -I${CONTRIB}/snmpd
CFLAGS+= -DHAVE_ERR_H -DHAVE_GETADDRINFO -DHAVE_STRLCPY -DHAVE_SYS_TREE_H

DEFS=	${MOD}_tree.def
INCS=	snmp_${MOD}.h
BMIBS=	BEGEMOT-IP-MIB.txt BEGEMOT-MIB2-MIB.txt

.include <bsd.snmpmod.mk>

smilint:
	env SMIPATH=/usr/share/snmp/mibs:/usr/local/share/snmp/mibs \
	    smilint -c /dev/null -l6 -i group-membership ${BMIBS:C/^/${CONTRIB}\/snmp_mibII\//}
