#include <config.h>
#include <libgnome/libgnome.h>
#define USE_WNCK
#ifdef USE_WNCK
  #define WNCK_I_KNOW_THIS_IS_UNSTABLE 
  #include <libwnck/libwnck.h>
#endif
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include "prettytable.h"
#include "defaulttable.h"
#include "proctable.h"

void free_entry (gpointer key, gpointer value, gpointer data);
void free_value (gpointer key, gpointer value, gpointer data);
void free_key (gpointer key, gpointer value, gpointer data);
#ifdef USE_WNCK
  WnckScreen *screen = NULL;
#endif

#ifdef USE_WNCK
static void
new_application (WnckScreen *screen, WnckApplication *app, gpointer data)
{
	ProcData *procdata = data;
	ProcInfo *info;
	GHashTable *hash = procdata->pretty_table->app_hash;
	gint pid;
	GdkPixbuf *icon = NULL;
	GList *list = NULL;
	WnckWindow *window;
	
	pid = wnck_application_get_pid (app);
	if (pid == 0)
		return;
	
	/* Hack - wnck_application_get_icon always returns the default icon */
#if 0	
	icon = wnck_application_get_mini_icon (app);	
#else
	list = wnck_application_get_windows (app);
	if (!list)
		return;
	window = list->data;
	icon = wnck_window_get_icon (window);
#endif
	if (!icon)
		return;
	
	/* If process already exists then set the icon. Otherwise put into hash
	** table to be added later */	
	info = proctable_find_process (pid, NULL, procdata);
	
	if (info) {
		/* Don't update the icon if there already is one */
		if (info->pixbuf)
			return;
		info->pixbuf = gdk_pixbuf_scale_simple (icon, 16, 16, 
							GDK_INTERP_HYPER);		
		if (info->visible) {
			GtkTreeModel *model;
			model = gtk_tree_view_get_model (GTK_TREE_VIEW (procdata->tree));
			gtk_tree_store_set (GTK_TREE_STORE (model), &info->node,
                            		    COL_PIXBUF, info->pixbuf, -1);
                }
	}
							
	g_hash_table_insert (hash, g_strdup_printf ("%d", pid), icon);
		
	
}	

static void
application_finished (WnckScreen *screen, WnckApplication *app, gpointer data)
{

	ProcData *procdata = data;
	GHashTable *hash = procdata->pretty_table->app_hash;
	gint pid;
	gpointer p1, p2;
	GdkPixbuf *icon;
	WnckWindow *window;
	gchar *id, *orig_id;

	pid =  wnck_application_get_pid (app);
	if (pid == 0)
		return;
		
	id = g_strdup_printf ("%d", pid);
	if (g_hash_table_lookup_extended (hash, id, &p1, &p2)) {
		orig_id = p1;
		g_hash_table_remove (hash, orig_id);
		if (orig_id) 
			g_free (orig_id);
	}
	
	
	g_free (id);
	
}
#endif

PrettyTable *pretty_table_new (ProcData *procdata) {
	PrettyTable *pretty_table = NULL;
	GList *list = NULL;
	
	pretty_table = g_malloc (sizeof (PrettyTable));
	
	pretty_table->app_hash = g_hash_table_new (g_str_hash, g_str_equal);
	pretty_table->default_hash = g_hash_table_new (g_str_hash, g_str_equal);
	
#ifdef USE_WNCK
	screen = wnck_screen_get_default ();
	wnck_screen_force_update (screen);
	list = wnck_screen_get_windows (screen);
	while (list) {
		WnckWindow *window = list->data;
		gint pid;
		
		pid = wnck_window_get_pid (window);
		if (pid > 0) {
			GdkPixbuf *icon;
			gchar *id;
			icon = wnck_window_get_icon (window);
			if (icon) {
				id = g_strdup_printf ("%d", pid);
				/* Don't put in icons for the same pid */
				if (!g_hash_table_lookup (pretty_table->app_hash, id)) 
				 	g_hash_table_insert (pretty_table->app_hash, 
						             id, icon);
					
				else 
					g_free (id);
			}
		}
		
		list = g_list_next (list);
	}	
#endif	

	
#ifdef USE_WNCK	
	g_signal_connect (G_OBJECT (screen), "application_opened",
			  G_CALLBACK (new_application), procdata);
	g_signal_connect (G_OBJECT (screen), "application_closed",
			  G_CALLBACK (application_finished), procdata);
#endif
	pretty_table_add_table (pretty_table, default_table);
	
	return pretty_table;
}

void pretty_table_add_table (PrettyTable *pretty_table, const gchar *table[]) {
	/* Table format:

	const gchar *table[] = {
			"X", "X Window System", "x.png",
			"bash" "Bourne Again Shell", "bash.png",
			NULL};
	*/
	gint i = 0;
	gchar *command, *prettyicon;
	gchar *text;
	
	while (table[i] && table[i + 1] && table[i + 2]) {
		/* pretty_table_free frees all string in the tables */
		command = g_strdup (table[i]); 
		/* pretty_table_free frees all string in the tables */
		text = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_DATADIR, "pixmaps/",
						  TRUE, NULL);
		if (text) {
			prettyicon = g_malloc (strlen (table[i + 2]) + strlen (text) + 1);
			sprintf (prettyicon, "%s%s", text, table[i + 2]);
			g_hash_table_insert (pretty_table->default_hash, 
					     command, prettyicon);
			g_free (text);
		}
		i += 3;
	}

	return;
}

#if 0
gchar *pretty_table_get_name (PrettyTable *pretty_table, const gchar *command) {
	gchar *pretty_name;

	if (!pretty_table)
		return NULL;
		
	pretty_name = g_hash_table_lookup (pretty_table->cmdline_to_prettyname, command);
	if (pretty_name) 
		return g_strdup (pretty_name); 

	pretty_name = g_hash_table_lookup (pretty_table->name_to_prettyname, command);
	if (pretty_name) 
		return g_strdup (pretty_name);

	return NULL;
}
#endif

GdkPixbuf *pretty_table_get_icon (PrettyTable *pretty_table, gchar *command, gint pid) 
{
	GdkPixbuf *icon = NULL, *tmp_pixbuf = NULL;
	gchar *icon_path = NULL;
	gchar *text;
	
	if (!pretty_table) 
		return NULL;

	icon_path = g_hash_table_lookup (pretty_table->default_hash, command);
	
	if (icon_path) {
		tmp_pixbuf = gdk_pixbuf_new_from_file (icon_path, NULL);		
	}
	/* This looks ugly but we don't want to unref the pixbuf in the 
	** app hash since it's owned by libwnck and not us */	
	if (tmp_pixbuf) {
		icon = gdk_pixbuf_scale_simple (tmp_pixbuf, 16, 16, 
						GDK_INTERP_HYPER);
		g_object_unref (tmp_pixbuf);
		return icon;
	}
		
	text = g_strdup_printf ("%d", pid);
	tmp_pixbuf = g_hash_table_lookup (pretty_table->app_hash, text);
	g_free (text);
	
	if (!tmp_pixbuf) 
		return NULL;
	
	icon = gdk_pixbuf_scale_simple (tmp_pixbuf, 16, 16, GDK_INTERP_HYPER);
		
	return icon;
	
}

void free_entry (gpointer key, gpointer value, gpointer data) {
	if (key)
		g_free (key);
	
	if (value)
		g_free (value);
}

void free_value (gpointer key, gpointer value, gpointer data) {
	if (value)
		g_free (value);
}

void free_key (gpointer key, gpointer value, gpointer data) {
	if (key)
		g_free (key);
}


void pretty_table_free (PrettyTable *pretty_table) {
	if (!pretty_table)
		return;
	return;
#if 0
	g_hash_table_foreach (pretty_table->cmdline_to_prettyname, free_entry, NULL);
	g_hash_table_destroy (pretty_table->cmdline_to_prettyname);
	g_hash_table_foreach (pretty_table->cmdline_to_prettyicon, free_value, NULL);
	g_hash_table_destroy (pretty_table->cmdline_to_prettyicon);
	g_hash_table_foreach (pretty_table->name_to_prettyicon, free_key, NULL);
	g_hash_table_destroy (pretty_table->name_to_prettyicon);
	g_hash_table_destroy (pretty_table->name_to_prettyname);
#endif
	g_free (pretty_table);
}


