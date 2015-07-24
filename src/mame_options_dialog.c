/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * GMAMEUI
 *
 * Copyright 2007-2010 Andrew Burton <adb@iinet.net.au>
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
#include <glib/gstdio.h>

#include "mame_options_dialog.h"
#include "mame_options_dialog_helper.h"
#include "mame_options.h"
#include "cell-renderer-captioned-image.h"
#include "gui.h"

struct _MameOptionsDialogPrivate {
	GtkBuilder *builder;
	
	GtkWidget *treeview;	/* This is for functionality of the sidebar */
	GtkListStore *store;	/* This is for functionality of the sidebar */

	GtkWidget *notebook;
};

#define MAME_OPTIONS_DIALOG_GET_PRIVATE(o)  (MAME_OPTIONS_DIALOG (o)->priv)

G_DEFINE_TYPE (MameOptionsDialog, mame_options_dialog, GTK_TYPE_DIALOG)

/* Function prototypes */
static void
mame_options_dialog_response             (GtkDialog *dialog, gint response);
static void
mame_options_dialog_destroy              (GtkObject *object);

static void
mame_options_dialog_add_page (MameOptionsDialog *dlg, const gchar *widget_name,
			      const gchar *title, const gchar *icon_filename);

static gchar *
get_scanlines ();

static const WidgetRelationship widget_relationships[] =
{
	/* Parent - Dependent - Relationship */
	{ "Video-video", "Video-scalemode", "soft" },
	{ "Video-autoframeskip", "Video-frameskip", "0" },
	{ "OpenGL-gl_glsl", "OpenGL-gl_glsl_filter", "0" },
	{ "Sound-sound", "Sound-samples", "1" },
	{ "Sound-sound", "Sound-samplerate", "1" },
	{ "Sound-sound", "Sound-volume", "1" },
	{ "Sound-sound", "Sound-audio_latency", "1" },
	{ "Performance-autoframeskip", "Performance-frameskip", "0" },
	{ "Debugging-log", "Debugging-verbose", "1" },
	{ "Debugging-debug", "Debugging-debugscript", "1" },
	{ "Debugging-debug", "Debugging-oslog", "1" },
	{ "Input-keymap", "Input-keymap_file", "1" },
};

/* List of functions for handling custom functions. The key is defined in the
   combo_links struct for the Combo widgets, and is used
   to determine which function is invoked to populate the widget.
   Currently this is used to support the -effect property only */
typedef struct {
	gchar * (*fn)();	/* The functions will take no parameters, and return a gchar* */
	const char* key;
} fnc_lookup;

static fnc_lookup custom_function_lookup_table[] = {
	{ &get_scanlines, "get_scanlines" },
};

static _combo_link combo_links[] = {
	{ "Video-video", "soft:opengl" },
	{ "Video-effect", "function:get_scanlines" },   /* Use custom function */
	{ "Video-scalemode", "none:async:yv12:yuy2:yv12x2:yuy2x2" },
	{ "Sound-samplerate", "48000:44100:22050:11025:0" },
	{ "OpenGL-gl_glsl_filter", "0:1:2" },
};

/* Get all the available effect files and add them to a ':'-delimited string
   suitable for processing in the GtkCombobox. Effect files are .png files
   in the MAME artwork directory */
static gchar *
get_scanlines ()
{
	GDir *dir;
	gchar *artworkdir;
	const gchar *current_file;
	gchar *scans;
	
	g_object_get (main_gui.gui_prefs, "dir-artwork", &artworkdir, NULL);

	g_return_val_if_fail (artworkdir != NULL, NULL);
	
	dir = g_dir_open (artworkdir, 0, NULL); /* FIXME TODO GError handling */

	scans = g_strdup ("none");

	current_file = g_dir_read_name (dir);
	while (current_file != NULL) {

		/* If file is .png, add to the list */
		if (g_str_has_suffix (g_utf8_strdown (current_file, -1), ".png")) {
			gchar **s;
			s = g_strsplit (current_file, ".", 0);  /* Want the name, but not the suffix */
			GMAMEUI_DEBUG ("  Found effect %s", s[0]);
			scans = g_strconcat (scans, ":", s[0], NULL);
			g_strfreev (s);
		}
		
		current_file = g_dir_read_name (dir);
	}

	return scans;
}

/* Boilerplate functions */
static GObject *
mame_options_dialog_constructor (GType                  type,
				guint                  n_construct_properties,
				GObjectConstructParam *construct_properties)
{
	GObject          *obj;
	MameOptionsDialog *dialog;

	obj = G_OBJECT_CLASS (mame_options_dialog_parent_class)->constructor (type,
									     n_construct_properties,
									     construct_properties);

	dialog = MAME_OPTIONS_DIALOG (obj);

	return obj;
}

static void
mame_options_dialog_class_init (MameOptionsDialogClass *class)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (class);
	GtkObjectClass *gtkobject_class = GTK_OBJECT_CLASS (class);
	GtkDialogClass *gtkdialog_class = GTK_DIALOG_CLASS (class);
	
	gobject_class->constructor = mame_options_dialog_constructor;
/*	gobject_class->get_property = mame_options_dialog_get_property;
	gobject_class->set_property = mame_options_dialog_set_property;*/

	gtkobject_class->destroy = mame_options_dialog_destroy;
	gtkdialog_class->response = mame_options_dialog_response;

	g_type_class_add_private (class,
				  sizeof (MameOptionsDialogPrivate));

	/* Signals and properties go here */
}

static void
update_property_on_change_int (GtkWidget *widget, gpointer user_data)
{
	gchar *key;
	gint val;
	
	key = (gchar *) user_data;
	
	g_return_if_fail (key != NULL);
	
	GMAMEUI_DEBUG ("Callback received on integer widget for %s", key);
	val = get_property_value_as_int (widget, key);
	GMAMEUI_DEBUG ("Setting integer value of widget for %s to %d", key, val);
	mame_options_set_int (main_gui.options, key, val);
}

static void
update_property_on_change_str (GtkWidget *widget, gpointer user_data)
{
	gchar *key;
	gchar *val;
	
	key = (gchar *) user_data;
	
	g_return_if_fail (key != NULL);
	
	GMAMEUI_DEBUG ("Callback received on widget for %s", key);
	val = get_property_value_as_string (widget, key);
	GMAMEUI_DEBUG ("Setting value of widget for %s to %s", key, val);
	mame_options_set_string (main_gui.options, key, val);
}

static void
update_property_on_change_double (GtkWidget *widget, gpointer user_data)
{
	gchar *key;
	gdouble val;
	
	key = (gchar *) user_data;
	
	g_return_if_fail (key != NULL);
	
	GMAMEUI_DEBUG ("Callback received on widget for %s", key);
	val = get_property_value_as_double (widget, key);

	if (g_ascii_strcasecmp (key, "Sound-volume") == 0) {
		/* Volume is an integer value but we are using a GtkHScale which
		   defaults to using doubles as the value. */
		GMAMEUI_DEBUG ("Setting value of widget for %s to %.2f", key, val);
		mame_options_set_int (main_gui.options, key, val);
	} else {
		GMAMEUI_DEBUG ("Setting value of widget for %s to %.2f", key, val);
		mame_options_set_dbl (main_gui.options, key, val);
	}
}

/* When a widget changes value, check to see whether there are any dependent
   widgets that need to have their state set correspondingly */
static void
parent_widget_clicked (GtkWidget *widget, gpointer user_data)
{
	GtkWidget *child;
	guint i;
	
	MameOptionsDialog *dlg;
	gchar *key;
	
	dlg = MAME_OPTIONS_DIALOG (g_object_get_data (G_OBJECT (widget), "MameOptionsDialog"));
	key = (gchar *) user_data;
		
	/* Process each of the widget relationships for the parent */
	for (i = 0; i < G_N_ELEMENTS (widget_relationships); i++) {
		/* If the widget clicked is a parent */
		if (g_ascii_strcasecmp (key, widget_relationships[i].parent) == 0) {
			GMAMEUI_DEBUG ("Trying to find widget %s for parent %s",
			               widget_relationships[i].dependent,
			               widget_relationships[i].parent);
			child = get_widget_with_suffix (dlg->priv->builder, widget_relationships[i].dependent);
			
			/* Set the state of the children based on the value of the parent */
			gchar *parent_val = get_property_value_as_string (widget, key);

			if (g_ascii_strcasecmp (parent_val, widget_relationships[i].relationship) == 0) {
				gtk_widget_set_sensitive (child, TRUE);
				/* FIXME TODO Also turn off the option */
			} else {
				gtk_widget_set_sensitive (child, FALSE);
				/* FIXME TODO Also turn on the option */
			}
		}
	}
}

static void
register_callbacks (MameOptionsDialog *dlg, GtkWidget *widget, gchar *key)
{
	/* Set the dialog as data against the widget, so the callback can
	   use the dialog's priv struct, e.g. for the keyfile etc */
	g_object_set_data (G_OBJECT (widget), "MameOptionsDialog", dlg);
	
	GMAMEUI_DEBUG ("Registering callback for widget associated with %s", key);

	if (GTK_IS_TOGGLE_BUTTON (widget)) {
		g_signal_connect (G_OBJECT(widget), "toggled",
				  G_CALLBACK (update_property_on_change_int), key);
		
		g_signal_connect (G_OBJECT (widget), "toggled",
				  G_CALLBACK (parent_widget_clicked), key);
	} else if (GTK_IS_SPIN_BUTTON (widget)) {
		g_signal_connect (G_OBJECT(widget), "value-changed",
				  G_CALLBACK (update_property_on_change_int), key);
	} else if (GTK_IS_HSCALE (widget)) {
		g_signal_connect (G_OBJECT(widget), "value-changed",
				  G_CALLBACK (update_property_on_change_double), key);
	} else if (GTK_IS_COMBO_BOX (widget)) {
		g_signal_connect (G_OBJECT (widget), "changed",
				  G_CALLBACK (update_property_on_change_str), key);
		g_signal_connect (G_OBJECT (widget), "changed",
				  G_CALLBACK (parent_widget_clicked), key);
	} else if (GTK_IS_FILE_CHOOSER (widget)) {
		g_signal_connect (G_OBJECT (widget), "file-set",
				  G_CALLBACK (update_property_on_change_str), key);
	}
}

static void
connect_prop_to_object (MameOptionsDialog *dlg, GtkWidget *object, const gchar *key)
{
	guint i;
	int int_value;
	gdouble dbl_value;
	gchar *value;
	
	if (GTK_IS_TOGGLE_BUTTON (object))
	{	
		int_value = mame_options_get_bool (main_gui.options, key);
		value = g_strdup_printf ("%d", int_value);
		set_property_value_as_string (object, value);
		g_free (value);
	} else if (GTK_IS_SPIN_BUTTON (object)) {
		int_value = mame_options_get_int (main_gui.options, key);
		value = g_strdup_printf ("%d", int_value);
		set_property_value_as_string (object, value);
		g_free (value);
	} else if (GTK_IS_HSCALE (object)) {
		dbl_value = mame_options_get_dbl (main_gui.options, key);
		value = g_strdup_printf ("%f", dbl_value);
		set_property_value_as_string (object, value);
		g_free (value);
	} else if (GTK_IS_FILE_CHOOSER (object)) {
		value = mame_options_get (main_gui.options, key);
		set_property_value_as_string (object, value);
		g_free (value);
	} else if (GTK_IS_COMBO_BOX (object)) {
		/* Find the associated internal entries (corresponding with the
		   display entries and link it to the widget */
		for (i = 0; i < G_N_ELEMENTS (combo_links); i++) {
			if (g_ascii_strcasecmp (combo_links[i].prop_name, key) == 0) {
				if (g_ascii_strncasecmp (combo_links[i].recorded_values, "function:", 9) == 0) {
					guint j;
					gchar *func_name;
					gchar *vals;
					gchar **vals_arr;
					
					/* Handle custom function */
					func_name = g_strdup (combo_links[i].recorded_values);
					func_name += 9; /* Skip "function:" to get the function to invoke */

					/* Find and invoke the relevant function */
					for (j = 0; j < G_N_ELEMENTS (custom_function_lookup_table); j++) {
						if (g_ascii_strcasecmp (custom_function_lookup_table[j].key, func_name) == 0) {
							vals = (*(custom_function_lookup_table[ j ].fn))();
							g_object_set_data (G_OBJECT(object), "untranslated",
							                   vals);
						}
					}

					/* Create a model to display the values. Need to do this
					   since we are dynamically generating the contents */
					GtkListStore *model;
					model = gtk_combo_box_get_model (GTK_COMBO_BOX (object));

					if (vals)
						vals_arr = g_strsplit (vals, ":", 0);

					for (j = 0; vals_arr[j] != NULL; j++) {
						GtkTreeIter iter;
						gtk_list_store_append (model, &iter);
						gtk_list_store_set (model, &iter, 0, vals_arr[j], -1);
						GMAMEUI_DEBUG ("  Appending %s to model", vals_arr[j]);
					}
					if (vals_arr)
						g_strfreev (vals_arr);

				} else {
					/* Set data based on what is defined in combo_links,
					   i.e. the majority of combo values */
					g_object_set_data (G_OBJECT(object), "untranslated",
							   combo_links[i].recorded_values);
				}
			}
		}
		
		value = mame_options_get (main_gui.options, key);
		set_property_value_as_string (object, value);
		g_free (value);
	} else
		GMAMEUI_DEBUG ("Connect_prop_to_object: Unhandled widget: %s", key);
}

/**
 * mame_options_register_property_raw:
 * @pr: a #MameOptions object
 * @object: Widget to register
 * @key: Property key
 * @default_value: Default value of the key
 * @flags: Flags
 * @object_type: Object type of widget
 * @data_type: Data type of the property
 *
 * This also registers only one widget, but instead of supplying the property
 * parameters as a single parsable string (as done in previous method), it
 * takes them separately.
 * 
 * Return value: TRUE if sucessful.
 */
static gboolean
mame_options_register_property_raw (MameOptionsDialog *dlg,
					   GtkWidget *object,
					   const gchar *key)
{
	g_return_val_if_fail (MAME_IS_OPTIONS_DIALOG (dlg), FALSE);
	g_return_val_if_fail (GTK_IS_WIDGET (object), FALSE);
	g_return_val_if_fail (key != NULL, FALSE);
	g_return_val_if_fail (strlen(key) > 0, FALSE);
	
	register_callbacks (dlg, object, key);

	connect_prop_to_object (dlg, object, key);
	
	return TRUE;
}

/**
 * mame_options_register_property_from_string:
 * @pr: a #MameOptions object
 * @object: Widget to register
 * @property_desc: Property description (in form #Category.#Key)
 *
 * This registers only one widget. The widget could be shown elsewhere.
 * the property_description should be of the form described before.
 * 
 * Return value: TRUE if sucessful.
 */
static gboolean
mame_options_register_property_from_string (MameOptionsDialog *dlg,
						   GtkWidget *widget,
						   const gchar *property_desc)
{
	gchar *key;
	MameExec *exec;
	
	g_return_val_if_fail (MAME_IS_OPTIONS_DIALOG(dlg), FALSE);
	g_return_val_if_fail ((GTK_IS_WIDGET (widget)), FALSE);
	g_return_val_if_fail (property_desc != NULL, FALSE);
	
	exec = mame_exec_list_get_current_executable (main_gui.exec_list);
	g_return_val_if_fail (exec != NULL, FALSE);
	
	key = g_strdup (property_desc);	

	/* Disable the widget and update the tooltip if this option is not
	   supported by MAME */
	if (!g_str_has_suffix (key, "_toggle")) {       /* Exclude non-options */
		gchar *key_field;
		key_field = get_key_field (key);

		if (!mame_has_option (exec, key_field)) {
			gchar *text;
			
			GMAMEUI_DEBUG ("Option %s not supported by this version of MAME", key_field);
		
			gtk_widget_set_sensitive (widget, FALSE);
			text = g_strdup_printf (_("The option %s is not supported by MAME %s"),
						key_field, mame_exec_get_version (exec));
			gtk_widget_set_tooltip_text (widget, text);
			g_free (text);
		}
		g_free (key_field);
	}
	
	mame_options_register_property_raw (dlg, widget, key);

	//g_free (key); // FIXME TODO Free'ing this segfaults in the callbacks for the various widgets
	
	return TRUE;
}

/**
 * mame_options_register_all_properties_from_builder_xml:
 * @dlg: a #MameOptionsDialog Object
 *
 * This will register all the properties names of the format described above
 * without considering the UI. Useful if you have the widgets shown elsewhere
 * but you want them to be part of preferences system.
 */
static void
mame_options_dialog_register_all_properties_from_builder_xml (MameOptionsDialog *dlg)
{
	GSList *widgets;
	GSList *node;
	
	g_return_if_fail (MAME_IS_OPTIONS_DIALOG (dlg));
	g_return_if_fail (dlg->priv->builder != NULL);
	
	widgets = gtk_builder_get_objects (dlg->priv->builder);
	node = g_slist_nth (widgets, 0);

	while (node)
	{
		const gchar *name;
		GtkWidget *widget;
		
		widget = node->data;

		if (GTK_IS_WIDGET (widget)) {	
			name = gtk_buildable_get_name (GTK_BUILDABLE (widget));

			if (g_ascii_strncasecmp (name, PREFERENCE_PROPERTY_PREFIX,
		             strlen (PREFERENCE_PROPERTY_PREFIX)) == 0)
			{
				const gchar *property = &name[strlen (PREFERENCE_PROPERTY_PREFIX)];
				mame_options_register_property_from_string (dlg, widget, property);
			}
		}
		node = g_slist_next (node);
	}
}

/* When an item in the sidebar is selected, we change the tab
   to the appropriate section */
static void
selection_changed_cb (GtkTreeSelection *selection,
		      MameOptionsDialog *dlg)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (selection, NULL, &iter)) {
		GtkWidget *widget;

		gtk_tree_model_get (GTK_TREE_MODEL (dlg->priv->store), &iter,
				    COL_WIDGET, &widget, -1);
		
		gtk_notebook_set_current_page 
			(GTK_NOTEBOOK (dlg->priv->notebook),
			 gtk_notebook_page_num (GTK_NOTEBOOK (dlg->priv->notebook),
						widget));
		
	}
}

/* Creates a hbox containing a treeview (the sidebar) and a notebook */
static void
mame_options_dialog_init (MameOptionsDialog *dlg)
{
	/* Widget declarations */
	GtkWidget *hbox;
	GtkWidget *scrolled_window;
	GtkWidget *btn_close;
	GtkTreeSelection *selection;
	GtkTreeSortable *sortable;

	MameOptionsDialogPrivate *priv;

	GError *error = NULL;

	const gchar *object_names[] = {
		"PerformanceWindow",
		"SoundWindow",
		"DebuggingWindow",
		"MiscWindow",
		"DisplayWindow",
		"InputWindow",
		"StatePlaybackWindow",
		"OpenGLWindow",
		"model_effect",		
		"model_samplerate",
		"model_videomode",
		"model_yuvmode",
		"model_glslfiltering",
		"adj_frameskip",
		"adj_speed",
		"adj_volume",
		"adj_audiolatency",
		"adj_brightness",
		"adj_contrast",
		"adj_gamma",
		"adj_pausebrightness",
		"adj_beamwidth",
		"adj_flicker",
		"adj_prescale",
	};
	
	priv = G_TYPE_INSTANCE_GET_PRIVATE (dlg,
					    MAME_TYPE_OPTIONS_DIALOG,
					    MameOptionsDialogPrivate);
	
	dlg->priv = priv;

	dlg->priv->builder = gtk_builder_new ();
	gtk_builder_set_translation_domain (dlg->priv->builder, GETTEXT_PACKAGE);
	
	if (!gtk_builder_add_objects_from_file (dlg->priv->builder,
	                                        GLADEDIR "options.builder",
	                                        (gchar **) object_names,
	                                        &error))
	{
		g_warning ("Couldn't load builder file: %s", error->message);
		g_error_free (error);
		return;
	}
	
	/* Now set up the dialog */
	hbox = gtk_hbox_new (FALSE, 0);	
	
	dlg->priv->treeview = gtk_tree_view_new ();
	gtk_widget_show (dlg->priv->treeview);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (dlg->priv->treeview),
					   FALSE);
	dlg->priv->store = gtk_list_store_new (LAST_COL, 
					       G_TYPE_STRING,
					       G_TYPE_STRING,
					       GDK_TYPE_PIXBUF,
					       GTK_TYPE_WIDGET,
					       G_TYPE_INT);
	sortable = GTK_TREE_SORTABLE (dlg->priv->store);
	gtk_tree_sortable_set_sort_column_id (sortable, COL_TITLE,
					      GTK_SORT_ASCENDING);
	gtk_tree_sortable_set_sort_func (sortable, COL_TITLE,
					 compare_pref_page_func,
					 NULL, NULL);
	
	gtk_tree_view_set_model (GTK_TREE_VIEW (dlg->priv->treeview),
				 GTK_TREE_MODEL (dlg->priv->store));

	add_category_columns (GTK_TREE_VIEW (dlg->priv->treeview));

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type 
		(GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	
	gtk_widget_show (scrolled_window);
	gtk_container_add (GTK_CONTAINER (scrolled_window), 
			   dlg->priv->treeview);
	gtk_box_pack_start (GTK_BOX (hbox), scrolled_window,
			    FALSE, FALSE, 0);

	dlg->priv->notebook = gtk_notebook_new ();
	gtk_widget_show (dlg->priv->notebook);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (dlg->priv->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (dlg->priv->notebook), 
				      FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (dlg->priv->notebook),
					8);
	
	gtk_box_pack_start (GTK_BOX (hbox), dlg->priv->notebook,
			    TRUE, TRUE, 0);

	selection = gtk_tree_view_get_selection 
		(GTK_TREE_VIEW (dlg->priv->treeview));

	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);

	g_signal_connect (selection, "changed", 
			  G_CALLBACK (selection_changed_cb), dlg);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), hbox,
			    TRUE, TRUE, 6);
	
	btn_close = gtk_dialog_add_button (GTK_DIALOG (dlg), GTK_STOCK_CLOSE, -7);
	gtk_widget_grab_default (btn_close);
	
	/* Add each of the notebooks */
	mame_options_dialog_add_page (dlg, "DisplayVBox", _("Display"), "gmameui-display-toolbar");
	mame_options_dialog_add_page (dlg, "OpenGLVBox", _("OpenGL"), "gmameui-display-toolbar");
	mame_options_dialog_add_page (dlg, "SoundVBox", _("Sound"), "gmameui-sound-toolbar");
	mame_options_dialog_add_page (dlg, "InputVBox", _("Input"), "gmameui-joystick-toolbar");
	mame_options_dialog_add_page (dlg, "PerformanceVBox", _("Performance"), "gmameui-general-toolbar");
	mame_options_dialog_add_page (dlg, "MiscVBox", _("Miscellaneous"), "gmameui-general-toolbar");
	mame_options_dialog_add_page (dlg, "DebuggingVBox", _("Debugging"), "gmameui-rom");

	mame_options_dialog_register_all_properties_from_builder_xml (dlg);
	
	gtk_widget_show (hbox);
}

GtkWidget *
mame_options_dialog_new (void)
{
	return g_object_new (MAME_TYPE_OPTIONS_DIALOG, 
				 "title", _("MAME Options"),
				 NULL);
}

static void
mame_options_dialog_response (GtkDialog *dialog, gint response)
{
	MameOptionsDialogPrivate *priv;
	
	priv = G_TYPE_INSTANCE_GET_PRIVATE (MAME_OPTIONS_DIALOG (dialog),
					    MAME_TYPE_OPTIONS_DIALOG,
					    MameOptionsDialogPrivate);
	
	switch (response)
	{
		case GTK_RESPONSE_CLOSE:
			/* Close button clicked */
			gtk_widget_destroy (GTK_WIDGET (dialog));

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
mame_options_dialog_destroy (GtkObject *object)
{
	MameOptionsDialog *dlg;
	
GMAMEUI_DEBUG ("Destroying mame options dialog...");	
	dlg = MAME_OPTIONS_DIALOG (object);
	
/* 	if (dlg->priv->builder) */
/* 		g_object_unref (dlg->priv->builder); */
	
	if (dlg->priv->store) {
		g_object_unref (dlg->priv->store);
		dlg->priv->store = NULL;
	}
	
	g_object_unref (dlg->priv);

	GTK_OBJECT_CLASS (mame_options_dialog_parent_class)->destroy (object);
	
GMAMEUI_DEBUG ("Destroying mame options dialog... done");
}

static void
mame_options_dialog_add_page (MameOptionsDialog *dlg, const gchar *widget_name,
			      const gchar *title, const gchar *icon_filename)
{
	GtkTreeIter iter;
	GtkTreeSelection* selection;
	GtkTreeIter first;
	GtkWidget *parent;
	GtkWidget *page;
	GdkPixbuf *icon;
	
	g_return_if_fail (dlg != NULL);
	g_return_if_fail (dlg->priv->builder != NULL);
	g_return_if_fail (dlg->priv->notebook != NULL);
	g_return_if_fail (dlg->priv->store != NULL);
	
	page = GTK_WIDGET (gtk_builder_get_object (dlg->priv->builder, widget_name));
	g_object_ref (page);
	
	g_return_if_fail (GTK_IS_WIDGET (page));
	
	gtk_widget_show (page);
	
	parent = gtk_widget_get_parent (page);
	if (parent && GTK_IS_CONTAINER (parent))
	{
		if (GTK_IS_NOTEBOOK (parent)) {
			gint page_num;
			
			page_num = gtk_notebook_page_num (GTK_NOTEBOOK (parent), page);
			gtk_notebook_remove_page (GTK_NOTEBOOK (parent), page_num);
		} else {
			gtk_container_remove (GTK_CONTAINER (parent), page);
		}
	}
	GMAMEUI_DEBUG ("Appending page to notebook for %s...", widget_name);
	gtk_notebook_append_page (GTK_NOTEBOOK (dlg->priv->notebook), page, NULL);
	GMAMEUI_DEBUG ("Appending page to notebook for %s... done", widget_name);

	icon = gmameui_get_icon_from_stock (icon_filename);
	
	gtk_list_store_append (dlg->priv->store, &iter);
	gtk_list_store_set (dlg->priv->store, &iter,
			    COL_NAME, widget_name,
			    COL_TITLE, _(title),
			    COL_PIXBUF, icon,
			    COL_WIDGET, page,
			    -1);
	
	gtk_tree_model_get_iter_first (GTK_TREE_MODEL (dlg->priv->store), &first);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dlg->priv->treeview));
	gtk_tree_selection_select_iter (selection, &first);
		
	g_object_unref (icon);
}
