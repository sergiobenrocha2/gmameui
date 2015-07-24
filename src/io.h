/*
 * GMAMEUI
 *
 * Copyright 2007-2009 Andrew Burton <adb@iinet.net.au>
 * based on GXMame code
 * 2002-2005 Stephane Pontier <shadow_walker@users.sourceforge.net>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef __IO_H__
#define __IO_H__

G_BEGIN_DECLS

#include "common.h"

#include <glib.h>
#include <stdio.h>

#define GMAMEUI_TYPE_IO_HANDLER            (gmameui_io_handler_get_type ())
#define GMAMEUI_IO_HANDLER(o)            (G_TYPE_CHECK_INSTANCE_CAST((o), GMAMEUI_TYPE_IO_HANDLER, GMAMEUIIOHandler))
#define GMAMEUI_IO_HANDLER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), GMAMEUI_TYPE_IO_HANDLER, GMAMEUIIOHandlerClass))
#define GMAMEUI_IS_IO_HANDLER(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), GMAMEUI_TYPE_IO_HANDLER))
#define GMAMEUI_IS_IO_HANDLER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), GMAMEUI_TYPE_IO_HANDLER))
#define GMAMEUI_IO_HANDLER_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), GMAMEUI_TYPE_IO_HANDLER, GMAMEUIIOHandlerClass))

typedef struct _GMAMEUIIOHandler GMAMEUIIOHandler;
typedef struct _GMAMEUIIOHandlerClass GMAMEUIIOHandlerClass;
typedef struct _GMAMEUIIOHandlerPrivate GMAMEUIIOHandlerPrivate;

struct _GMAMEUIIOHandler {
	GObject parent;
	
	GMAMEUIIOHandlerPrivate *priv;
	/* define public instance variables here */
};

struct _GMAMEUIIOHandlerClass {
	GObjectClass parent;
	/* define vtable methods and signals here */

};

GType gmameui_io_handler_get_type (void);
GMAMEUIIOHandler* gmameui_io_handler_new (void);

#define FLOAT_BUF_SIZE G_ASCII_DTOSTR_BUF_SIZE
const gchar *
my_dtostr(char* buf, gdouble d);

gboolean
gmameui_file_save_to_file(GKeyFile *kf, const gchar *file);

gboolean
load_games_ini(void);

gboolean
load_catver_ini(void);

void
quick_check(void);

gboolean
save_games_ini(void);

GList *
get_ctrlr_list (void);

gboolean
check_rom_exists_as_file (gchar *romname);

gboolean
check_samples_exists_as_file (gchar *samplename);

G_END_DECLS

#endif /* __IO_H__ */
