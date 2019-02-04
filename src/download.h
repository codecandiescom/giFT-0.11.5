/*
 * $Id: download.h,v 1.41 2003/12/25 15:16:16 hipnod Exp $
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

#ifndef __DOWNLOAD_H
#define __DOWNLOAD_H

/*****************************************************************************/

#include "transfer.h"

/*****************************************************************************/

void       download_report_attached (TCPC *c);
List      *download_list            (void);
Transfer  *download_new             (TCPC *c, if_event_id requested,
                                     char *uniq, char *filename,
                                     char *hash, off_t size);
void       download_free            (Transfer *transfer, int premature);
void       download_pause           (Transfer *transfer);
void       download_unpause         (Transfer *transfer);
void       download_stop            (Transfer *transfer, int cancel);
void       download_add_source      (Transfer *transfer, char *user, char *hash, char *url);
void       download_remove_source        (Transfer *transfer, Source *source);
void       download_remove_source_by_url (Transfer *transfer, char *url);
void       download_write           (Chunk *chunk, unsigned char *segment,
                                     size_t len);
BOOL       download_sync            (Transfer *transfer);
void       download_recover_state   (void);

GIFTD_EXPORT
  BOOL     download_make_source     (Transfer *transfer,
                                     char *user, char *hash, char *url);

GIFTD_EXPORT
  size_t   download_throttle        (Chunk *chunk, size_t len);

/*****************************************************************************/

#endif /* __DOWNLOAD_H */
