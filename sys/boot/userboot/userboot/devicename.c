/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: projects/armv6/sys/boot/userboot/userboot/devicename.c 223695 2011-06-30 16:08:56Z dfr $");

#include <stand.h>
#include <string.h>
#include <sys/disklabel.h>

#include "bootstrap.h"
#include "disk.h"
#include "libuserboot.h"

static int	userboot_parsedev(struct disk_devdesc **dev, const char *devspec, const char **path);

/* 
 * Point (dev) at an allocated device specifier for the device matching the
 * path in (devspec). If it contains an explicit device specification,
 * use that.  If not, use the default device.
 */
int
userboot_getdev(void **vdev, const char *devspec, const char **path)
{
    struct disk_devdesc **dev = (struct disk_devdesc **)vdev;
    int				rv;
    
    /*
     * If it looks like this is just a path and no
     * device, go with the current device.
     */
    if ((devspec == NULL) || 
	(devspec[0] == '/') || 
	(strchr(devspec, ':') == NULL)) {

	if (((rv = userboot_parsedev(dev, getenv("currdev"), NULL)) == 0) &&
	    (path != NULL))
		*path = devspec;
	return(rv);
    }
    
    /*
     * Try to parse the device name off the beginning of the devspec
     */
    return(userboot_parsedev(dev, devspec, path));
}

/*
 * Point (dev) at an allocated device specifier matching the string version
 * at the beginning of (devspec).  Return a pointer to the remaining
 * text in (path).
 *
 * In all cases, the beginning of (devspec) is compared to the names
 * of known devices in the device switch, and then any following text
 * is parsed according to the rules applied to the device type.
 *
 * For disk-type devices, the syntax is:
 *
 * disk<unit>[s<slice>][<partition>]:
 * 
 */
static int
userboot_parsedev(struct disk_devdesc **dev, const char *devspec, const char **path)
{
    struct disk_devdesc *idev;
    struct devsw	*dv;
    int			i, unit, slice, partition, err;
    char		*cp;
    const char		*np;

    /* minimum length check */
    if (strlen(devspec) < 2)
	return(EINVAL);

    /* look for a device that matches */
    for (i = 0, dv = NULL; devsw[i] != NULL; i++) {
	if (!strncmp(devspec, devsw[i]->dv_name, strlen(devsw[i]->dv_name))) {
	    dv = devsw[i];
	    break;
	}
    }
    if (dv == NULL)
	return(ENOENT);
    idev = malloc(sizeof(struct disk_devdesc));
    err = 0;
    np = (devspec + strlen(dv->dv_name));
        
    switch(dv->dv_type) {
    case DEVT_NONE:			/* XXX what to do here?  Do we care? */
	break;

    case DEVT_DISK:
	unit = -1;
	slice = -1;
	partition = -1;
	if (*np && (*np != ':')) {
	    unit = strtol(np, &cp, 10);	/* next comes the unit number */
	    if (cp == np) {
		err = EUNIT;
		goto fail;
	    }
#ifdef LOADER_GPT_SUPPORT
	    if (*cp == 'p') {		/* got a GPT partition */
		np = cp + 1;
		slice = strtol(np, &cp, 10);
		if (cp == np) {
		    err = ESLICE;
		    goto fail;
		}
		if (*cp && (*cp != ':')) {
		    err = EINVAL;
		    goto fail;
		}
		partition = 0xff;
	    } else {
#endif
		if (*cp == 's') {		/* got a slice number */
		    np = cp + 1;
		    slice = strtol(np, &cp, 10);
		    if (cp == np) {
			err = ESLICE;
			goto fail;
		    }
		}
		if (*cp && (*cp != ':')) {
		    partition = *cp - 'a';	/* got a partition number */
		    if ((partition < 0) || (partition >= MAXPARTITIONS)) {
			err = EPART;
			goto fail;
		    }
		    cp++;
		}
#ifdef LOADER_GPT_SUPPORT
	    }
#endif
	} else {
		cp = np;
	}
	if (*cp && (*cp != ':')) {
	    err = EINVAL;
	    goto fail;
	}

	idev->d_unit = unit;
	idev->d_slice = slice;
	idev->d_partition = partition;
	if (path != NULL)
	    *path = (*cp == 0) ? cp : cp + 1;
	break;

    case DEVT_CD:
    case DEVT_NET:
    case DEVT_ZFS:
	unit = 0;

	if (*np && (*np != ':')) {
	    unit = strtol(np, &cp, 0);	/* get unit number if present */
	    if (cp == np) {
		err = EUNIT;
		goto fail;
	    }
	} else {
		cp = np;
	}
	if (*cp && (*cp != ':')) {
	    err = EINVAL;
	    goto fail;
	}

	idev->d_unit = unit;
	if (path != NULL)
	    *path = (*cp == 0) ? cp : cp + 1;
	break;

    default:
	err = EINVAL;
	goto fail;
    }
    idev->d_dev = dv;
    idev->d_type = dv->dv_type;
    if (dev == NULL) {
	free(idev);
    } else {
	*dev = idev;
    }
    return(0);

 fail:
    free(idev);
    return(err);
}


char *
userboot_fmtdev(void *vdev)
{
    struct disk_devdesc	*dev = (struct disk_devdesc *)vdev;
    static char		buf[128];	/* XXX device length constant? */
    char		*cp;
    
    switch(dev->d_type) {
    case DEVT_NONE:
	strcpy(buf, "(no device)");
	break;

    case DEVT_CD:
	sprintf(buf, "%s%d:", dev->d_dev->dv_name, dev->d_unit);
	break;

    case DEVT_DISK:
	cp = buf;
	cp += sprintf(cp, "%s%d", dev->d_dev->dv_name, dev->d_unit);
#ifdef LOADER_GPT_SUPPORT
	if (dev->d_partition == 0xff) {
	    cp += sprintf(cp, "p%d", dev->d_slice);
	} else {
#endif
	    if (dev->d_slice > 0)
		cp += sprintf(cp, "s%d", dev->d_slice);
	    if (dev->d_partition >= 0)
		cp += sprintf(cp, "%c", dev->d_partition + 'a');
#ifdef LOADER_GPT_SUPPORT
	}
#endif
	strcat(cp, ":");
	break;

    case DEVT_NET:
    case DEVT_ZFS:
	sprintf(buf, "%s%d:", dev->d_dev->dv_name, dev->d_unit);
	break;
    }
    return(buf);
}


/*
 * Set currdev to suit the value being supplied in (value)
 */
int
userboot_setcurrdev(struct env_var *ev, int flags, const void *value)
{
    struct disk_devdesc	*ncurr;
    int			rv;

    if ((rv = userboot_parsedev(&ncurr, value, NULL)) != 0)
	return(rv);
    free(ncurr);
    env_setenv(ev->ev_name, flags | EV_NOHOOK, value, NULL, NULL);
    return(0);
}
