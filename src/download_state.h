/*
 * $Id: download_state.h,v 1.1 2003/05/29 10:05:34 jasta Exp $
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

#ifndef __DOWNLOAD_STATE_H
#define __DOWNLOAD_STATE_H

/*****************************************************************************/

/**
 * @file download_state.h
 */

/*****************************************************************************/

/**
 * Write the statefile information for the supplied transfer object.  This
 * operation will override any previously written state files.  No action
 * will be taken if there are no chunk divisions on the transfer.
 *
 * @return Boolean success or failure.  If this function returns FALSE, the
 *         transfer will need to be aborted and/or paused immediately, and
 *         you will have to roll back the current transmit values.
 */
BOOL download_state_save (Transfer *t);

/**
 * Recover all state files from the incoming path directory.  This will
 * effectively instantiate the objects, add them to the list, and release the
 * download mechanism to download them.
 *
 * @note This is not safe to call more than once.  Use it at startup only.
 *
 * @return Number of state files successfully recovered.
 */
int download_state_recover (void);

/*****************************************************************************/

#endif /* __DOWNLOAD_STATE_H */
