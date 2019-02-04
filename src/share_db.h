/*
 * $Id: share_db.h,v 1.8 2003/05/29 09:37:43 jasta Exp $
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

#ifndef __SHARE_DB_H
#define __SHARE_DB_H

/*****************************************************************************/

/**
 * @file share_db.h
 *
 * @brief Handles low-level storage and retrieval of individual local shares.
 */

/*****************************************************************************/

struct sdb_t;
typedef struct sdb_t SDB;

/*****************************************************************************/

/**
 * Create the database handle and open the supplied file.
 *
 * @param path  Passed directly to fopen.
 * @param mode  Passed directly to fopen.
 *
 * @return Database handle or NULL if the open failed.
 */
SDB *sdb_open (char *path, char *mode);

/**
 * Same as ::sdb_open except that the underlying file will be created/truncated
 * regardless of \em mode.
 */
SDB *sdb_create (char *path, char *mode);

/**
 * Destroy the database handle and close the file opened within, if any.
 */
void sdb_close (SDB *sdb);

/*****************************************************************************/

/**
 * Write a single share entry to the database.
 *
 * @param sdb
 * @param file
 *
 * @return Boolean success or failure.
 */
BOOL sdb_write (SDB *sdb, FileShare *file);

/**
 * Read a single share entry from the database at the current position.  The
 * records data will be moved off of the database and subsequent calls will
 * return a new record.
 *
 * @param sdb
 *
 * @return The resulting FileShare object or NULL on error.
 */
FileShare *sdb_read (SDB *sdb);

/*****************************************************************************/

#endif /* __SHARE_DB_H */
