/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * GMAMEUI
 *
 * Copyright 2007-2010 Andrew Burton <adb@iinet.net.au>
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

#include "common.h"

#include <string.h>
#include <gio/gio.h>

#include "gui.h"
#include "directories.h"
#include "gmameui-gamelist-view.h"
#include "gmameui-main-win.h"   /* For gmameui_ui_set_items_sensitive */

/* Callbacks */
static void on_dir_browse_button_clicked (GtkWidget *widget, gpointer user_data);
static void on_file_browse_button_clicked (GtkWidget *widget, gpointer user_data);

static gboolean rom_path_changed (GValueArray *new_paths, GValueArray *old_paths);

static void add_path_to_tree_view (GtkWidget *button, gpointer user_data);
static void add_item_to_tree_view (GtkWidget *button, gpointer user_data);
static void remove_path_from_tree_view (GtkWidget *button, gpointer user_data);

static void mame_directories_dialog_save_changes (MameDirectoriesDialog *dialog);

typedef struct _url_prefs url_prefs;

struct _url_prefs {
	gchar *pref_name;
	gchar *url;
};

url_prefs urls [] = {
	{ "dir-artwork", "http://mrdo.mameworld.info/mame_artwork_ingame.html" },
	{ "dir-flyers", "http://www.arcadeflyers.com" },
	{ "dir-title", "http://www.mametitles.com/" },

	{ "dir-icons", "http://icons.mameworld.info/" },
	{ "file-mameinfo", "http://mameinfo.mameworld.info/" },
	{ "file-history", "http://www.arcade-history.com" },
	{ "file-cheat", "http://cheat.retrogames.com" },
	{ "file-hiscore", "http://highscore.mameworld.info/" },
	{ "file-catver", "http://www.progettoemma.net/?catlist" },
};

struct _MameDirectoriesDialogPrivate {
	GtkBuilder *builder;
	
	GtkTreeView  *xmame_execs_tree_view;
	GtkTreeModel *xmame_execs_tree_model;

	GtkTreeView  *roms_path_tree_view;
	GtkTreeModel *roms_path_tree_model;

	GtkTreeView  *samples_path_tree_view;
	GtkTreeModel *samples_path_tree_model;

	/* Value array containing the ROM paths when the dialog is
	   first opened. Compared when closing to see whether we
	   need to refresh the game list */
	GValueArray *va_orig_rom_paths;
};

#define MAME_DIRECTORIES_DIALOG_GET_PRIVATE(o)  (MAME_DIRECTORIES_DIALOG (o)->priv)

G_DEFINE_TYPE (MameDirectoriesDialog, mame_directories_dialog, GTK_TYPE_DIALOG)

/* Function prototypes */
static void
mame_directories_dialog_response             (GtkDialog *dialog, gint response);
static void
mame_directories_dialog_destroy              (GtkObject *object);

/* Boilerplate functions */
static GObject *
mame_directories_dialog_constructor (GType                  type,
				guint                  n_construct_properties,
				GObjectConstructParam *construct_properties)
{
	GObject          *obj;
	MameDirectoriesDialog *dialog;

	obj = G_OBJECT_CLASS (mame_directories_dialog_parent_class)->constructor (type,
									     n_construct_properties,
									     construct_properties);

	dialog = MAME_DIRECTORIES_DIALOG (obj);

	return obj;
}

static void
mame_directories_dialog_class_init (MameDirectoriesDialogClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (class);
	GtkObjectClass *gtkobject_class = GTK_OBJECT_CLASS (class);
	GtkDialogClass *gtkdialog_class = GTK_DIALOG_CLASS (class);

	gobject_class->constructor = mame_directories_dialog_constructor;
/*	gobject_class->get_property = mame_directories_dialog_get_property;
	gobject_class->set_property = mame_directories_dialog_set_property;*/

	/* Set destroy and response callbacks */
	gtkobject_class->destroy = mame_directories_dialog_destroy;
	gtkdialog_class->response = mame_directories_dialog_response;

	g_type_class_add_private (class,
				  sizeof (MameDirectoriesDialogPrivate));

	/* Signals and properties go here */

}

static void
mame_directories_dialog_init (MameDirectoriesDialog *dialog)
{
	MameDirectoriesDialogPrivate *priv;
	guint i;
	GtkAccelGroup *accel_group;

	GtkTreeIter iter;
	GtkTreeModel *store;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeSelection *select;
	
	GValueArray *va_exec_paths;
	GValueArray *va_rom_paths;
	GValueArray *va_sample_paths;
	
	GtkWidget *widget;
	GSList *widgets;	/* For use with processing multiple widgets at once */
	GSList *node;		/* For use with processing multiple widgets at once */

	GError* error = NULL;

	gchar *object_names[] = {
		"notebook1", NULL	/* Dialog */
	};

	priv = G_TYPE_INSTANCE_GET_PRIVATE (dialog,
					    MAME_TYPE_DIRECTORIES_DIALOG,
					    MameDirectoriesDialogPrivate);

	dialog->priv = priv;
GMAMEUI_DEBUG ("Creating new directories dialog...");
	/* Build the UI and connect signals here */
	priv->builder = gtk_builder_new ();
	gtk_builder_set_translation_domain (priv->builder, GETTEXT_PACKAGE);
	
	if (!gtk_builder_add_objects_from_file (priv->builder,
	                                        GLADEDIR "directories.builder",
	                                        (gchar **) object_names,
	                                        &error)) {
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
		return;
	}

	/* Get the dialog contents */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "notebook1"));

	/* Add our dialog contents to the vbox of the dialog class */
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    widget, TRUE, TRUE, 6);
		
	gtk_widget_show_all (GTK_WIDGET (widget));
	
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
	
	g_object_get (main_gui.gui_prefs,
		      "executable-paths", &va_exec_paths,
		      "rom-paths", &va_rom_paths,
		      "sample-paths", &va_sample_paths,
		      NULL);

	accel_group = gtk_accel_group_new ();
	
	/* Set the original ROM path list */
	priv->va_orig_rom_paths = g_value_array_copy (va_rom_paths);
	/*GMAMEUI_DEBUG ("Storing a copy of the ROM paths value array with %d elements",
		       priv->va_orig_rom_paths->n_values);*/

	/* If no executables are set, look for a MAME executable in the path */
	if (va_exec_paths->n_values == 0) {
		gchar *foundexec;

		GMAMEUI_DEBUG ("No executables found, looking in path...");
	
		foundexec = g_find_program_in_path ("sdlmame");

		/* Check whether an executable was found in the path */
		if (foundexec != NULL) {
			MameExec *exec;

			GMAMEUI_DEBUG ("Found %s", foundexec);
			
			/* Check whether any found executable is a valid MAME executable */
			exec = mame_exec_new_from_path (foundexec);
			if (exec != NULL) {
				/* Add the executable to the list */
				mame_exec_list_add (main_gui.exec_list, exec);

				/* Update the preferences */
				va_exec_paths = mame_exec_list_get_list_as_value_array (main_gui.exec_list);
				g_object_set (main_gui.gui_prefs,
					      "executable-paths", va_exec_paths,
					      NULL);
			}
		
			g_free (foundexec);
		}
	}
	
	/* XMame Exe List */
	store = (GtkTreeModel *) gtk_list_store_new (1, G_TYPE_STRING);
	if (va_exec_paths) {
		for (i = 0; i < va_exec_paths->n_values; i++) {
			gtk_list_store_append (GTK_LIST_STORE (store), &iter);  /* Acquire an iterator */
			gtk_list_store_set (GTK_LIST_STORE (store), &iter, 
					    0, g_value_get_string (g_value_array_get_nth (va_exec_paths, i)),
					    -1);
		}
	}
	priv->xmame_execs_tree_model = store;
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (" ", renderer,
							   "text", 0,
							   NULL);

	priv->xmame_execs_tree_view = GTK_TREE_VIEW (gtk_builder_get_object (priv->builder, "xmame_execs_tree_view"));
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->xmame_execs_tree_view), column);
	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->xmame_execs_tree_view), GTK_TREE_MODEL (store));
	g_object_set_data (G_OBJECT (priv->xmame_execs_tree_view), "SettingName", "executable-paths");

    	/* Roms Path List */
	store = (GtkTreeModel *) gtk_list_store_new (1, G_TYPE_STRING);
	if (va_rom_paths) {
		for (i = 0; i < va_rom_paths->n_values; i++) {
			gtk_list_store_append (GTK_LIST_STORE (store), &iter);  /* Acquire an iterator */
			gtk_list_store_set (GTK_LIST_STORE (store), &iter, 
					    0, g_value_get_string (g_value_array_get_nth (va_rom_paths, i)),
					    -1);
		}
	}
	priv->roms_path_tree_model = store;
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (" ", renderer,
							   "text", 0,
							   NULL);

	priv->roms_path_tree_view = GTK_TREE_VIEW (gtk_builder_get_object (priv->builder, "roms_path_tree_view"));
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->roms_path_tree_view), column);
	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->roms_path_tree_view), GTK_TREE_MODEL (store));
	g_object_set_data (G_OBJECT (priv->roms_path_tree_view), "SettingName", "rom-paths");

	/* Samples Path List */
	store = (GtkTreeModel *) gtk_list_store_new (1, G_TYPE_STRING);
	if (va_sample_paths) {
		for (i = 0; i < va_sample_paths->n_values; i++) {
			gtk_list_store_append (GTK_LIST_STORE (store), &iter);  /* Acquire an iterator */
			gtk_list_store_set (GTK_LIST_STORE (store), &iter, 
					    0, g_value_get_string (g_value_array_get_nth (va_sample_paths, i)),
					    -1);
		}
	}
	priv->samples_path_tree_model = store;
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (" ", renderer,
							   "text", 0,
							   NULL);

	priv->samples_path_tree_view = GTK_TREE_VIEW (gtk_builder_get_object (priv->builder, "samples_path_tree_view"));
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->samples_path_tree_view), column);
	gtk_tree_view_set_model (GTK_TREE_VIEW (priv->samples_path_tree_view), GTK_TREE_MODEL (store));
	g_object_set_data (G_OBJECT (priv->samples_path_tree_view), "SettingName", "sample-paths");
    
	
	widgets = gtk_builder_get_objects (priv->builder);
	node = g_slist_nth (widgets, 0);

	while (node)
	{
		const gchar *name;
		gchar *val;
		GtkWidget *widget, *home_icon;
		GError *error = NULL;
		
		widget = node->data;

		home_icon = gtk_image_new_from_stock (GTK_STOCK_HOME, GTK_ICON_SIZE_BUTTON);
		
		name = gtk_buildable_get_name (GTK_BUILDABLE (widget));

		/* Set the GtkEntry widgets with the parameter values */
		if (g_ascii_strncasecmp (name, "entry_", 6) == 0) {
			GMAMEUI_DEBUG ("Now processing widget %s", name);
			name += 6;      /* Skip over entry_ */
		
			g_object_get (main_gui.gui_prefs, name, &val, NULL);
			gtk_entry_set_text (GTK_ENTRY (widget), val);
			g_free (val);
		}

		/* Set the callbacks for the GtkButtons for each GtkEntry */
		if (g_ascii_strncasecmp (name, "button_", 7) == 0) {
			/*GMAMEUI_DEBUG ("Now processing widget %s", name);*/
			name += 7;      /* Skip over button_ */
			
			if (g_ascii_strncasecmp (name, "dir-", 4) == 0)
				g_signal_connect (G_OBJECT (widget), "clicked",
						  G_CALLBACK (on_dir_browse_button_clicked),
						  dialog);
			else
				g_signal_connect (G_OBJECT (widget), "clicked",
						  G_CALLBACK (on_file_browse_button_clicked),
						  dialog);
		}

		/* Configure the URL buttons */
		if (g_ascii_strncasecmp (name, "link_", 5) == 0) {
			/*GMAMEUI_DEBUG ("Now processing widget %s", name);*/
			name += 5;      /* Skip over link_ */

			/* Set stock button icon */
			gtk_button_set_image (GTK_BUTTON (widget), home_icon);

			for (i = 0; i < G_N_ELEMENTS (urls); i++) {
				GMAMEUI_DEBUG ("Comparing %s and %s", urls[i].pref_name, name);
				if (g_ascii_strcasecmp (urls[i].pref_name, name) == 0) {
					GMAMEUI_DEBUG ("  Showing URL %s", urls[i].url);

					gtk_link_button_set_uri (GTK_LINK_BUTTON (widget), urls[i].url);
					if (error) {
						GMAMEUI_DEBUG ("%s", error->message);
						g_error_free (error);
						error = NULL;
					}
				}

			}

		}
				    
		node = g_slist_next (node);
	}
	
	/* FIXME TODO Hiscore dir */
	
	/* Callbacks for adding/removing executable */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "xmame_execs_add_button"));
	g_signal_connect (G_OBJECT (widget), "clicked",
			  G_CALLBACK (add_item_to_tree_view), priv->xmame_execs_tree_model);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "xmame_execs_remove_button"));
	g_signal_connect (G_OBJECT (widget), "clicked",
			  G_CALLBACK (remove_path_from_tree_view), priv->xmame_execs_tree_view);

	/* Callbacks for adding/removing ROM paths */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "roms_add_button"));
	g_signal_connect (G_OBJECT (widget), "clicked",
			  G_CALLBACK (add_path_to_tree_view), priv->roms_path_tree_model);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "roms_remove_button"));
	g_signal_connect (G_OBJECT (widget), "clicked",
			  G_CALLBACK (remove_path_from_tree_view), priv->roms_path_tree_view);

	/* Callbacks for adding/removing sample paths */
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "samples_add_button"));
	g_signal_connect (G_OBJECT (widget), "clicked",
			  G_CALLBACK (add_path_to_tree_view), priv->samples_path_tree_model);
	widget = GTK_WIDGET (gtk_builder_get_object (priv->builder, "samples_remove_button"));
	g_signal_connect (G_OBJECT (widget), "clicked",
			  G_CALLBACK (remove_path_from_tree_view), priv->samples_path_tree_view);
	
	/* Set the selection mode to be single */
	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->xmame_execs_tree_view));
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);

	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->samples_path_tree_view));
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);

	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->roms_path_tree_view));
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);

	gtk_window_add_accel_group (GTK_WINDOW (dialog), accel_group);
	
	widget = gtk_dialog_add_button (GTK_DIALOG (dialog),
					GTK_STOCK_CLOSE,
					GTK_RESPONSE_CLOSE);

	g_value_array_free (va_exec_paths);
	va_exec_paths = NULL;
	g_value_array_free (va_rom_paths);
	va_rom_paths = NULL;
	g_value_array_free (va_sample_paths);
	va_sample_paths = NULL;
GMAMEUI_DEBUG ("Creating new directories dialog... done");
}

GtkWidget *
mame_directories_dialog_new (GtkWindow *parent)
{
	GtkWidget *dialog;

	dialog = g_object_new (MAME_TYPE_DIRECTORIES_DIALOG,
			       "title", _("Directories Selection"),
			       NULL);

	if (parent)
		gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

	return dialog;

}

static void
mame_directories_dialog_response (GtkDialog *dialog, gint response)
{
	MameDirectoriesDialogPrivate *priv;
	gint num_execs, num_rom_paths;
	gint result;
	
	priv = G_TYPE_INSTANCE_GET_PRIVATE (MAME_DIRECTORIES_DIALOG (dialog),
					    MAME_TYPE_DIRECTORIES_DIALOG,
					    MameDirectoriesDialogPrivate);

	switch (response)
	{
		case GTK_RESPONSE_CLOSE:
			/* Close button clicked */
			num_execs = gtk_tree_model_iter_n_children (priv->xmame_execs_tree_model, NULL);
			num_rom_paths = gtk_tree_model_iter_n_children (priv->roms_path_tree_model, NULL);
			
			/* Check if there are no executables selected */
			if (num_execs == 0) {
				GtkWidget *msg_dlg;

				msg_dlg = gtk_message_dialog_new (NULL,
							GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_MESSAGE_WARNING,
							GTK_BUTTONS_YES_NO,
							_("No MAME executables have been chosen"));

				gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (msg_dlg),
									  _("Without a MAME executable, you will not be able to emulate any ROMs. "
									    "Are you sure you want to close the window?"));
				result = gtk_dialog_run (GTK_DIALOG (msg_dlg));
				gtk_widget_destroy (msg_dlg);
			} else if (num_rom_paths == 0) {
				GtkWidget *msg_dlg;

				msg_dlg = gtk_message_dialog_new (NULL,
							GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_MESSAGE_WARNING,
							GTK_BUTTONS_YES_NO,
							_("No ROM directories have been chosen"));

				gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (msg_dlg),
									  _("Without any ROM directories, you will not be able to emulate any ROMs. "
									    "Are you sure you want to close the window?"));
				result = gtk_dialog_run (GTK_DIALOG (msg_dlg));
				gtk_widget_destroy (msg_dlg);
			} else
				result = GTK_RESPONSE_YES;
			
			if (result == GTK_RESPONSE_YES) {
				/* If the user chose to close the window */
				gtk_widget_hide (GTK_WIDGET (dialog));
				
				mame_directories_dialog_save_changes (MAME_DIRECTORIES_DIALOG (dialog));
			
				gtk_widget_destroy (GTK_WIDGET (dialog));

				mame_gamelist_view_repopulate_contents (main_gui.displayed_list);
			}
			
			break;
		case GTK_RESPONSE_DELETE_EVENT:
			/* Dialog closed */
			gtk_widget_destroy (GTK_WIDGET (dialog));
			
			break;
		default:
			g_assert_not_reached ();
	}
}

static void
mame_directories_dialog_destroy (GtkObject *object)
{
	MameDirectoriesDialog *dlg;
	
GMAMEUI_DEBUG ("Destroying mame directories dialog...");	
	dlg = MAME_DIRECTORIES_DIALOG (object);
	
	if (dlg->priv->builder)
		g_object_unref (dlg->priv->builder);

	/* Empty models */
	gtk_list_store_clear (GTK_LIST_STORE (dlg->priv->xmame_execs_tree_model));
	g_object_unref (dlg->priv->xmame_execs_tree_model);
	gtk_list_store_clear (GTK_LIST_STORE (dlg->priv->roms_path_tree_model));
	g_object_unref (dlg->priv->roms_path_tree_model);
	gtk_list_store_clear (GTK_LIST_STORE (dlg->priv->samples_path_tree_model));
	g_object_unref (dlg->priv->samples_path_tree_model);
	
	if (dlg->priv->va_orig_rom_paths)
		g_value_array_free (dlg->priv->va_orig_rom_paths);
	
/*	if (dlg->priv)
		g_object_unref (dlg->priv);*/
	
/*	GTK_OBJECT_CLASS (mame_directories_dialog_parent_class)->destroy (object);*/
	
GMAMEUI_DEBUG ("Destroying mame directories dialog... done");
}

/**
 * update_other_entries
 * @dialog: the #MameDirectoriesDialog
 * @filename: the selected path from the #GtkFileChooser
 *
 * This function is called whenever a user updates the value of a directory or file.
 * It looks to see whether the parent folder is called MAME; if so, we look to see
 * whether the parent folder has other subfolders (e.g. artwork, flyers, snap, etc)
 * that can be set automatically.
 *
 * It also looks for expected files (e.g. cheat.dat, mameinfo.dat).
 *
 * FIXME TODO This should also be performed when a new ROM or Sample path is added
 */
static void update_other_entries (MameDirectoriesDialog *dialog, gchar *filename)
{
	GFile *folder, *parentfolder;
	gchar *parentfoldername;
	
	/* Here we want to look for all the other files, as well. so check the parent folder */
	folder = g_file_new_for_path (filename);
	parentfolder = g_file_get_parent (folder);

	g_return_if_fail (parentfolder != NULL);

	/* Get the containing folder (in the case of a file) or the parent folder
	   (in the case of a folder) */
	parentfoldername = g_file_get_basename (parentfolder);

	/* If the parent folder is "MAME", look inside to see which other folders it has */
	if (g_ascii_strcasecmp (parentfoldername, "mame") == 0) {
		guint i;
	
		/* Look for other subfolders */
		for (i = 0; i < G_N_ELEMENTS (directory_prefs); i++) {
			GFile *tmpdir;
			gchar *testname;
			
			GMAMEUI_DEBUG ("  Looking for directory/file at %d called %s", i, directory_prefs[i].default_dir);

			tmpdir = g_file_get_child (parentfolder, directory_prefs[i].default_dir);
			testname = g_file_get_parse_name (tmpdir);

			if (((g_ascii_strncasecmp (directory_prefs[i].name, "dir-", 4) == 0) && (g_file_test (testname, G_FILE_TEST_IS_DIR))) ||
			    ((g_ascii_strncasecmp (directory_prefs[i].name, "file-", 5) == 0) && (g_file_test (testname, G_FILE_TEST_IS_REGULAR)))) {
				gchar *val;
			
				GMAMEUI_DEBUG ("     Directory/file exists");

				g_object_get (main_gui.gui_prefs, directory_prefs[i].name, &val, NULL);

				/* If there is no existing value set (or if there is but it
				   is invalid), and the MAME/<dirname> directory exists,
				   then update the preferences value and the widget */
				if ((val == NULL) || (strlen (val) == 0) || (g_file_test (val, G_FILE_TEST_EXISTS) == FALSE)) {
					GtkWidget *widget;
					gchar *widgetname;

					/* Update the preferences */
					g_object_set (main_gui.gui_prefs, directory_prefs[i].name, testname, NULL);

					/* Update the widget */
					widgetname = g_strdup_printf ("entry_%s", directory_prefs[i].name);
					widget = GTK_WIDGET (gtk_builder_get_object (dialog->priv->builder, widgetname));
					if (widget)
						gtk_entry_set_text (GTK_ENTRY (widget), testname);
					g_free (widgetname);
				
				} else {
					GMAMEUI_DEBUG ("       Pre-existing value is %s", val);
				}
			
				if (val);
					g_free (val);

			 } else
				GMAMEUI_DEBUG ("     Directory/file %s does not exist", testname);
			g_free (testname);
		}
	}
	g_free (parentfoldername);
	g_object_unref (folder);
	g_object_unref (parentfolder);
}

/* This function loads the catver file once as an idle function, and returns
   FALSE so the file is not repeatedly loaded */
static gboolean
on_idle_load_catver ()
{
	load_catver_ini ();

	/* Return FALSE so this function is not repeatedly called */
	return FALSE;
}

/* Handle specifying a directory (excluding Exec, ROM or Sample directory).
   If a directory is chosen, update the associated GtkEntry, and update the
   associated preferences entry */
static void
on_dir_browse_button_clicked (GtkWidget *widget, gpointer user_data)
{
	MameDirectoriesDialog *dialog;
	GtkWidget *path_entry;
	GtkWidget *filechooser;
	const gchar *name;
	gchar *widget_name;
	gchar *entry_text;

	dialog = (MameDirectoriesDialog *) user_data;
	
	name = gtk_buildable_get_name (GTK_BUILDABLE (widget));
	name += 7;      /* Skip over button_ */
	/* GMAMEUI_DEBUG ("Browsing for directory for path %s", name); */
	
	widget_name = g_strdup_printf ("entry_%s", name);
	
	path_entry = GTK_WIDGET (gtk_builder_get_object (dialog->priv->builder, widget_name));
	
	filechooser = gtk_file_chooser_dialog_new (_("Browse for Folder"),
						   GTK_WINDOW (dialog),
						   GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
						   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						   GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
						   NULL);
	entry_text = gtk_editable_get_chars (GTK_EDITABLE (path_entry), 0, -1);      
	gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (filechooser), entry_text);    
	g_free (entry_text);
	
	if (gtk_dialog_run (GTK_DIALOG (filechooser)) == GTK_RESPONSE_ACCEPT) {
		gchar *filename;
		
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filechooser));
		gtk_entry_set_text (GTK_ENTRY (path_entry), filename);
		g_object_set (main_gui.gui_prefs, name, filename, NULL);

		/* See if the other directories can also be set based on this choice */
		update_other_entries (dialog, filename);
		
		g_free (filename);
	}
	gtk_widget_destroy (filechooser);

	g_free (widget_name);
}

/* Handle specifying a file. If a file is chosen, update the associated
   GtkEntry, and update the associated preferences entry */
static void
on_file_browse_button_clicked (GtkWidget *widget, gpointer user_data)
{
	MameDirectoriesDialog *dialog;
	GtkWidget *path_entry;
	GtkWidget *filechooser;
	const gchar *name;
	gchar *widget_name;
	gchar *entry_text;
	
	dialog = (MameDirectoriesDialog *) user_data;
	
	name = gtk_buildable_get_name (GTK_BUILDABLE (widget));
	name += 7;      /* Skip over button_ */
	/* GMAMEUI_DEBUG ("Browsing for file for %s", name); */
	
	widget_name = g_strdup_printf ("entry_%s", name);
	
	path_entry = GTK_WIDGET (gtk_builder_get_object (dialog->priv->builder, widget_name));
	
	filechooser = gtk_file_chooser_dialog_new (_("Browse for File"),
						   GTK_WINDOW (dialog),
						   GTK_FILE_CHOOSER_ACTION_OPEN,
						   GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						   GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
						   NULL);
	entry_text = gtk_editable_get_chars (GTK_EDITABLE (path_entry), 0, -1);      
	gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (filechooser), entry_text);    
	g_free (entry_text);
	
	if (gtk_dialog_run (GTK_DIALOG (filechooser)) == GTK_RESPONSE_ACCEPT) {
		gchar *path;
		path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filechooser));
		
		gtk_entry_set_text (GTK_ENTRY (path_entry), path);

		/* If we are updating the catver file, this will need to trigger a refresh of the main screen */
		if (g_ascii_strcasecmp (widget_name, "entry_file-catver") == 0) {
			gchar *current_val;
			
			g_object_get (main_gui.gui_prefs, name, &current_val, NULL);
			
			/* If the values are different, trigger the catver file to be loaded */
			if (g_ascii_strcasecmp (current_val, path) != 0)
				g_idle_add (on_idle_load_catver, NULL);

			g_free (current_val);
		}
		
		g_object_set (main_gui.gui_prefs, name, path, NULL);

		/* See if the other files can also be set based on this choice */
		update_other_entries (dialog, path);

		g_free (path);
	}
	gtk_widget_destroy (filechooser);

	g_free (widget_name);
}

/* Create a new file chooser to select a path for either ROMs or Samples. Once
   selected, the executable is added to the corresponding TreeView widget for
   the Directories Selection window. Note that it doesn't update the preferences
   until the window is closed. */
static void
add_path_to_tree_view (GtkWidget *button, gpointer user_data)
{
	GtkTreeIter iter;
	gchar *temp_text, *text;
	gint i, size;
	gboolean already_exist = FALSE;
	
	GtkTreeModel *model = (gpointer) user_data;
	
	/* Create new directory file chooser */
	GtkWidget *chooser = gtk_file_chooser_dialog_new (_("Select directory"),
							  NULL, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
							  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							  GTK_STOCK_OK, GTK_RESPONSE_OK,
							  NULL);
	gint response = gtk_dialog_run (GTK_DIALOG (chooser));
	switch (response)
	{
		case GTK_RESPONSE_OK:
			temp_text = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
			gtk_widget_destroy (chooser);    

			/* Check if what we are trying to add is not already in the list */
			already_exist = FALSE;
			size = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (model), NULL);

			for (i = 0; i < size; i++) {
				gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (model), &iter, NULL, i);
				gtk_tree_model_get (model, &iter, 0, &text, -1);
				if (g_ascii_strcasecmp (text, temp_text) == 0)
					already_exist = TRUE;
			}

			if (!already_exist) {    
				/* Acquire an iterator */
				gtk_list_store_append (GTK_LIST_STORE (model), &iter);
				gtk_list_store_set (GTK_LIST_STORE (model), &iter,
						    0, temp_text,
						    -1);

			} else
				GMAMEUI_DEBUG ("Trying to add %s to the tree model but it already exists", temp_text);

			g_free (temp_text);
			break;
		default:
			gtk_widget_destroy (chooser);
			break;
	}				    
}

/* Create a new file chooser to select a MAME executable. Once selected, the executable
   is added to the MameExecList and added to the TreeView widget for the Directories
   Selection window */
static void
add_item_to_tree_view (GtkWidget *button, gpointer user_data)
{
	GtkTreeModel *model = (gpointer) user_data;
	GtkTreeIter iter;
	gchar *temp_text, *text;
	gint i, size;
	gboolean already_exist = FALSE;
	
	/* Create new directory file chooser */
	GtkWidget *chooser = gtk_file_chooser_dialog_new (_("Select MAME executable"),
							  NULL, GTK_FILE_CHOOSER_ACTION_OPEN,
							  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
							  GTK_STOCK_OK, GTK_RESPONSE_OK,
							  NULL);
	gint response = gtk_dialog_run (GTK_DIALOG (chooser));
	switch (response)
	{
		case GTK_RESPONSE_OK:
			temp_text = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
			gtk_widget_destroy (chooser);	
			
			/* Check if what we are trying to add is not already in the list */
			already_exist = FALSE;
			size = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (model), NULL);

			for (i = 0; i < size; i++) {
				gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (model), &iter, NULL, i);
				gtk_tree_model_get (model, &iter, 0, &text, -1);
				if (g_ascii_strcasecmp (text, temp_text) == 0)
					already_exist = TRUE;
			}

			if (!already_exist) {
				/* Check if executable is valid, and if so, add to mame_exec_list */
				MameExec *exec;
				exec = mame_exec_new_from_path (temp_text);
				if (exec) {
					/* Add to the MameExecList */
					mame_exec_list_add (main_gui.exec_list, exec);
					
					/* Update the preferences */
					GValueArray *va_paths = mame_exec_list_get_list_as_value_array (main_gui.exec_list);
					g_object_set (main_gui.gui_prefs,
						      "executable-paths", va_paths,
						      NULL);
					g_value_array_free (va_paths);
				
					/* Add to the tree view in the directories window */
					gtk_list_store_append (GTK_LIST_STORE (model), &iter);  /* Acquire an iterator */
					gtk_list_store_set (GTK_LIST_STORE (model), &iter,
							    0, temp_text,
							    -1);
				} else {
					/* This was not a valid executable */
					GtkWidget *dlg;
					dlg = gtk_message_dialog_new (NULL,
					                              GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					                              GTK_MESSAGE_ERROR,
					                              GTK_BUTTONS_CLOSE,
					                              _("Not a valid MAME executable"));
					gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dlg),
					                                          _("The selected executable is not a valid MAME executable"));
					gtk_dialog_run (GTK_DIALOG (dlg));
					gtk_widget_destroy (dlg);
				}
			}

			g_free (temp_text);
			break;
		default:
			gtk_widget_destroy (chooser);
			break;
	}
}

/* Removes the selected directory from the selected tree view, and trigger an update to the
   relevant MameGuiPrefs entry; the parameter referencing the Prefs entry is added to the
   GtkTreeView using g_object_set_data when the dialog is first opened */
static void
remove_path_from_tree_view (GtkWidget *button, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *path;
	
	GtkTreeView *view = (gpointer) user_data;

	GtkTreeSelection *select;
	select = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (view));
	if (gtk_tree_selection_get_selected (select, &model, &iter)) {
		gchar *param_name;
		
		gtk_tree_selection_unselect_all (select);
		
		/* Get the path of the item to be removed */
		gtk_tree_model_get (model, &iter, 0, &path, -1);
		
		/* Get the associated parameter name */
		param_name = (gchar *) g_object_get_data (G_OBJECT (view), "SettingName");
		
		GMAMEUI_DEBUG ("Removing path from list for %s", param_name);
		
		if (g_ascii_strcasecmp (param_name, "executable-paths") == 0) {
			/* Remove the relevant executable from the list of executables */
			mame_exec_list_remove_by_path (main_gui.exec_list, path);
		}

		/* Update the preferences */
		GValueArray *va_paths = mame_exec_list_get_list_as_value_array (main_gui.exec_list);

		g_return_if_fail (va_paths != NULL);
		g_return_if_fail (main_gui.gui_prefs != NULL);

		g_object_set (main_gui.gui_prefs, param_name, va_paths, NULL);
		g_value_array_free (va_paths);
		
		/* Remove from the tree view */
		gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	}
}

/* Checks whether the value array when the dialog is closed is different from that
   when the dialog is first opened */
static gboolean
rom_path_changed (GValueArray *new_paths, GValueArray *old_paths)
{
	gboolean changed_flag;
	int i;
	int num_new_paths, num_old_paths;
	
	changed_flag = FALSE;
	
	num_new_paths = (!new_paths) ? 0 : new_paths->n_values;
	num_old_paths = (!old_paths) ? 0 : old_paths->n_values;
	
	/* GMAMEUI_DEBUG ("Num of existing paths is %d and new paths is %d", num_old_paths, num_new_paths); */

	if (num_new_paths != num_old_paths) {
		GMAMEUI_DEBUG ("Number of paths is different - %d to %d", num_new_paths, num_old_paths);
		changed_flag = TRUE;
	} else {
		/* Sort the paths first */
		new_paths = g_value_array_sort (new_paths, (GCompareFunc) g_ascii_strcasecmp);
		old_paths = g_value_array_sort (old_paths, (GCompareFunc) g_ascii_strcasecmp);
		
		for (i = 0;
		     i < (num_new_paths > num_old_paths ? num_new_paths : num_old_paths);
		     i++) {
			if (g_ascii_strcasecmp (g_value_get_string (g_value_array_get_nth (new_paths, i)),
						g_value_get_string (g_value_array_get_nth (old_paths, i))) != 0) {
				GMAMEUI_DEBUG ("Value of directories has changed");
				changed_flag = TRUE;
			}
		}
	}

	return changed_flag;
}

/* Parses a list store and returns the content as a ValueArray */
static GValueArray *
get_tree_model_as_value_array (GtkTreeModel *list_store)
{
	GValueArray *array;
	GValue val = { 0, };
	GtkTreeIter iter;
	gboolean valid;
	
	/* Initialise the GValueArray and GValue */
	array = g_value_array_new (0);
	g_value_init (&val, G_TYPE_STRING);
	
	/* Get the first iter in the list */
	valid = gtk_tree_model_get_iter_first (list_store, &iter);
	
	while (valid) {
		/* Walk through the list, reading each row */
		gchar *str;
		
		gtk_tree_model_get (list_store, &iter, 0, &str, -1);
		
		g_value_set_string (&val, str);
		
		array = g_value_array_append (array, &val);
		
		g_free (str);
		
		valid = gtk_tree_model_iter_next (list_store, &iter);
	}
	
	return array;
}


static void
mame_directories_dialog_save_changes (MameDirectoriesDialog *dialog)
{
	GValueArray *va_paths;  /* Used for processing list stores into gchar* arrays */
	gchar *ctrlr_dir;

	ListMode current_mode;

	g_return_if_fail (dialog->priv->roms_path_tree_model != NULL);
	g_return_if_fail (dialog->priv->samples_path_tree_model != NULL);
	
	g_object_get (main_gui.gui_prefs,
		      "current-mode", &current_mode,
		      "dir-ctrlr", &ctrlr_dir,
		      NULL);
	
	/* GMAMEUI */
/*
	gchar *catver_path;
	catver_path = gtk_editable_get_chars (GTK_EDITABLE (catver_path_entry), 0, -1);

	if (g_ascii_strcasecmp (catver_dir, catver_path) != 0) {

		g_object_set (main_gui.gui_prefs, "file-catver", catver_path, NULL);
		
		* Updating with the new catver file *
		g_idle_add (load_catver_ini, NULL);

		* Updating the UI if necessary *
		Columns_type type;
		g_object_get (selected_filter, "type", &type, NULL);

		if ( (type == CATEGORY) || (type == MAMEVER)
		     || (gui_prefs.FolderID == CATEGORIES) || (gui_prefs.FolderID == VERSIONS) ) {
			gui_prefs.FolderID = AVAILABLE;
			gmameui_message (WARNING, GTK_WINDOW (directories_selection), _("Current Folder may not exist after loading the new catver file.\nMoving to the \"Available\" game folder"));
			create_gamelist_content ();
		} else if ( (current_mode == DETAILS || current_mode == DETAILS_TREE)
			  && (gui_prefs.ColumnShown[MAMEVER]==TRUE || gui_prefs.ColumnShown[CATEGORY] == TRUE)) {
			create_gamelist_content ();
		}

	}

	g_free (catver_path);*/

	/* If the ctrlr directory has changed, we reload the default options */
/* FIXME TODO
	if (!gtk_editable_get_chars (GTK_EDITABLE (ctrlr_directory_entry), 0, -1)
	    || strcmp (ctrlr_dir, gtk_editable_get_chars (GTK_EDITABLE (ctrlr_directory_entry), 0, -1)))
	{
		g_free (ctrlr_dir);
		g_object_set (main_gui.gui_prefs,
			      "dir-ctrlr", gtk_editable_get_chars (GTK_EDITABLE (ctrlr_directory_entry), 0, -1),
			      NULL);
	}*/
	
	/* Get sample paths from tree model */
	va_paths = get_tree_model_as_value_array (GTK_TREE_MODEL (dialog->priv->samples_path_tree_model));
	g_object_set (main_gui.gui_prefs, "sample-paths", va_paths, NULL);
	g_value_array_free (va_paths);
	va_paths = NULL;
	

	/* Get ROM paths from tree model and determine whether the list is different from that
	   in the preferences; if so, we should prompt the user to  update the game list */
	va_paths = get_tree_model_as_value_array (GTK_TREE_MODEL (dialog->priv->roms_path_tree_model));
	if (rom_path_changed (va_paths, dialog->priv->va_orig_rom_paths))
		g_object_set (main_gui.gui_prefs, "rom-paths", va_paths, NULL);
	g_value_array_free (va_paths);
	va_paths = NULL;
	
	/* FIXME TODO Prompt user to rebuild the gamelist */
/*	
	* FIXME TODO hiscore dir *
	if (changed_flag) {
		* Do we perform the quickcheck? *
		* FIXME TODO Do this automatically as a background thread, updating the
		   progress bar
		GtkWidget *dialog;
		gint result;
		dialog = gtk_message_dialog_new (GTK_WINDOW (directories_selection),
						GTK_DIALOG_MODAL,
						GTK_MESSAGE_WARNING,
						GTK_BUTTONS_YES_NO,
						_("The Rom Path has been modified.\nDo you want to refresh the gamelist after the directory window is closed?"));
		result = gtk_dialog_run (GTK_DIALOG (dialog));
		switch (result)
		{
			case GTK_RESPONSE_YES:
				refresh_game_list=TRUE;
				break;
			default:
				break;
		}
		gtk_widget_destroy (dialog);
		*
	}*/

	/* remove and destroy the submenu */
	add_exec_menu ();

	/* Update the state of the menus and toolbars */
	gmameui_ui_set_items_sensitive ();

	g_free (ctrlr_dir);

}
