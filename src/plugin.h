/*
 * $Id: plugin.h,v 1.7 2003/08/11 18:18:51 jasta Exp $
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

#ifndef __PLUGIN_H
#define __PLUGIN_H

/*****************************************************************************/

/**
 * @file plugin.h
 *
 * @brief Interface for managing protocol plugins.
 *
 * This is only used to load the object (when dynamic loading support is
 * present) and call the initializer protocol functions.
 */

/*****************************************************************************/

/**
 * Load and initialize the protocol plugin.
 *
 * @param file   Path to the plugin object, if using dynamic loading.
 * @param pname  Case-sensitive and fully qualified protocol name.  The plugin
 *               must "agree" with this name.  For example, OpenFT defines
 *               itself literally as "OpenFT", so in order to load OpenFT
 *               you would need exactly "OpenFT" here.  This can usually
 *               be parsed from the object filename.
 *
 * @return Initialized protocol object.  That is, the Protocol::init method
 *         will be called.  If the initialization fails, NULL will be
 *         returned.
 */
Protocol *plugin_init (const char *file, const char *pname);

/**
 * Effectively execute the initialized protocol plugin.  This will tell the
 * plugin that it is OK to begin network connections and start the full giFT
 * event cycle.
 *
 * @return Direct reply from \em p->start.
 */
BOOL plugin_start (Protocol *p);

/**
 * Destroy the plugin completely, including the plugins destroy symbol.  This
 * will also remove from the internal table that ::plugin_init implicitly
 * added the plugin to.
 */
void plugin_unload (Protocol *p);

/**
 * Remove all plugins as though plugin_unload was called from a loop.  This
 * is only useful when giFT is closing.
 */
void plugin_unloadall (void);

/*****************************************************************************/

/**
 * Lookup a plugin entry by name.  This is used primarily by the interface
 * protocol code when a specific plugin is requested by the giFT client.
 */
Protocol *plugin_lookup (const char *pname);

/**
 * Basic interator interface for the loaded plugins.
 */
void plugin_foreach (DatasetForeachFn foreachfn, void *udata);

/*****************************************************************************/

/**
 * Temporarily direct method for querying the p->features dataset.  At the
 * next minor release, this will be moving into the Protocol structure as
 * p->support_get or something.
 */
BOOL plugin_support (Protocol *p, const char *feature);

/*****************************************************************************/

#endif /* __PLUGIN_H */
