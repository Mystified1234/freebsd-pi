/*-
 * Copyright (c) 2001 Brian Fundakowski Feldman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "includes.h"
__RCSID("$FreeBSD: projects/armv6/crypto/openssh/version.c 224638 2011-08-03 19:14:22Z brooks $");

#include <string.h>

#include "version.h"
#include "xmalloc.h"


static char *version = NULL;

const char *
ssh_version_get(void) {

	if (version == NULL)
		version = xstrdup(SSH_VERSION);
	return (version);
}

void
ssh_version_set_addendum(const char *add) {
	char *newvers;
	size_t size;

	if (add != NULL) {
		size = strlen(SSH_VERSION_BASE) + strlen(SSH_VERSION_HPN) + 1 +
		    strlen(add) + 1;
		newvers = xmalloc(size);
		snprintf(newvers, size, "%s %s", SSH_VERSION_BASE,
		    SSH_VERSION_HPN, add);
	} else {
		newvers = xstrdup(SSH_VERSION_BASE SSH_VERSION_HPN);
	}
	if (version != NULL)
		xfree(version);
	version = newvers;
}
