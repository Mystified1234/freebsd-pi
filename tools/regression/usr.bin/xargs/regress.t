#!/bin/sh
# $FreeBSD: projects/armv6/tools/regression/usr.bin/xargs/regress.t 137587 2004-11-11 19:47:55Z nik $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
