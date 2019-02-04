/*
 * $Id: protocol.c,v 1.16 2003/07/21 08:25:04 jasta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "lib/libgift.h"

#include "protocol.h"

/*****************************************************************************/

#define COMPAT_MASK (LIBGIFTPROTO_MKVERSION (0xff, 0xff, 0x00))
#define COMPAT_CMP(a,b) (INTCMP ((a & COMPAT_MASK), (b & COMPAT_MASK)))

/*****************************************************************************/

static int check_link_compat (uint32_t linked_ver)
{
	int   ret;
	char *quality;

	ret = COMPAT_CMP (linked_ver, LIBGIFTPROTO_VERSION);

	if (ret > 0)
		quality = "older";
	else if (ret < 0)
		quality = "newer";
	else
		quality = NULL;

	if (quality)
	{
		GIFT_ERROR (("libgiftproto is %s than linked daemon or plugin",
		             quality));
	}

	return ret;
}

static int check_dev_compat (Protocol *p, uint32_t dev_ver)
{
	int ret;

	ret = COMPAT_CMP (dev_ver, LIBGIFTPROTO_VERSION);

	if (ret != 0)
	{
		GIFT_ERROR (("the plugin %s has not been updated "
		             "for the current libgiftproto runtime", p->name));
	}

	return ret;
}

int protocol_compat_ex (Protocol *p, uint32_t linked_ver, uint32_t dev_ver)
{
	int ret;

	if ((ret = check_link_compat (linked_ver)))
		return ret;

	/* giftd will only be checking link compat, but plugins will check dev
	 * compat as well */
	if (p && dev_ver > 0)
	{
		if ((ret = check_dev_compat (p, dev_ver)))
			return ret;
	}

	return ret;
}
