/*
 * GNOME panel menu module.
 * (C) 1997, 1998, 1999, 2000 The Free Software Foundation
 * Copyright 2000 Helix Code, Inc.
 * Copyright 2000 Eazel, Inc.
 *
 * Authors: Miguel de Icaza
 *          Federico Mena
 */

#include <config.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <math.h>
#include <gnome.h>

#include "panel-include.h"
#include "panel-widget.h"
#include "tearoffitem.h"
#include "gnome-run.h"
#include "title-item.h"
#include "scroll-menu.h"
#include "icon-entry-hack.h"
#include "multiscreen-stuff.h"
#include "conditional.h"

/*#define PANEL_DEBUG 1*/

typedef enum {
	SMALL_ICON_SIZE  = 20,
	MEDIUM_ICON_SIZE = 32,
	BIG_ICON_SIZE    = 48
} IconSize;

static char *gnome_folder = NULL;

extern GSList *applets;
extern GSList *applets_last;
extern int applet_count;

/*list of all toplevel panel widgets (basep) created*/
extern GSList *panel_list;
/*list of all PanelWidgets created*/
extern GSList *panels;

extern gboolean commie_mode;
extern gboolean no_run_box;
extern GlobalConfig global_config;

extern int config_sync_timeout;
extern int panels_to_sync;
extern int applets_to_sync;
extern int need_complete_save;

extern int base_panels;

extern char *kde_menudir;
extern char *kde_icondir;
extern char *kde_mini_icondir;

extern GtkTooltips *panel_tooltips;

enum {
	HELP_BUTTON = 0,
	REVERT_BUTTON,
	CLOSE_BUTTON
};

typedef struct _TearoffMenu TearoffMenu;
struct _TearoffMenu {
	GtkWidget *menu;
	GSList *mfl;
	char *special;
	char *title;
	char *wmclass;
};

/* list of TearoffMenu s */
static GSList *tearoffs = NULL;

typedef struct _ShowItemMenu ShowItemMenu;
struct _ShowItemMenu {
	int type;
	const char *item_loc;
	MenuFinfo *mf;
	GtkWidget *menu;
	GtkWidget *menuitem;
	int applet;
};

typedef struct _FakeIcon FakeIcon;
struct _FakeIcon {
	GtkWidget *fake;
	char *file;
	int size;
};

static guint load_icons_id = 0;
static GSList *icons_to_load = NULL;

static GtkWidget * create_menu_at_fr (GtkWidget *menu,
				      FileRec *fr,
				      gboolean applets,
				      gboolean launcher_add,
				      gboolean favourites_add,
				      const char *dir_name,
				      const char *pixmap_name,
				      gboolean fake_submenus,
				      gboolean force,
				      gboolean title);
static GtkWidget * create_panel_submenu (GtkWidget *m,
					 gboolean fake_sub,
					 gboolean tearoff,
					 gboolean is_base);
static GtkWidget * create_desktop_menu (GtkWidget *m,
					gboolean fake_sub,
					gboolean tearoff);

static GtkWidget * fake_pixmap_from_fake (FakeIcon *fake);

static void add_kde_submenu (GtkWidget *root_menu,
			     gboolean fake_submenus,
			     gboolean launcher_add,
			     gboolean favourites_add);
static void add_distribution_submenu (GtkWidget *root_menu,
				      gboolean fake_submenus,
				      gboolean launcher_add,
				      gboolean favourites_add);

static GtkWidget * create_add_launcher_menu (GtkWidget *menu,
					     gboolean fake_submenus);
static GtkWidget * create_add_favourites_menu (GtkWidget *menu,
					       gboolean fake_submenus);

static void setup_menuitem_try_pixmap (GtkWidget *menuitem,
				       const char *try_file,
				       const char *title,
				       IconSize icon_size);

/*to be called on startup to load in some of the directories,
  this makes the startup a little bit slower, and take up slightly
  more ram, but it also speeds up later operation*/
void
init_menus (void)
{
	const DistributionInfo *distribution_info = get_distribution_info ();
	char *menu;

	/*just load the menus from disk, don't make the widgets
	  this just reads the .desktops of the top most directory
	  and a level down*/
	menu = gnome_datadir_file ("gnome/apps");
	if (menu != NULL)
		fr_read_dir (NULL, menu, NULL, NULL, 2);
	g_free (menu);

	menu = gnome_datadir_file ("applets");
	if (menu != NULL)
		fr_read_dir (NULL, menu, NULL, NULL, 2);
	g_free (menu);

	menu = gnome_util_home_file ("apps");
	if (menu != NULL)
		fr_read_dir (NULL, menu, NULL, NULL, 2);
	g_free (menu);

	if (distribution_info != NULL &&
	    distribution_info->menu_init_func != NULL)
		distribution_info->menu_init_func ();
}

static gboolean
check_for_screen (GtkWidget *w, GdkEvent *ev, gpointer data)
{
	static int times = 0;
	if (ev->type != GDK_KEY_PRESS)
		return FALSE;
	if (ev->key.keyval == GDK_f ||
	    ev->key.keyval == GDK_F) {
		times++;
		if (times == 3) {
			times = 0;
			start_screen_check ();
		}
	}
	return FALSE;
}

/*the most important dialog in the whole application*/
static void
about_cb (GtkWidget *widget, gpointer data)
{
	static GtkWidget *about;
	GtkWidget *hbox, *l;
	char *authors[] = {
	  "George Lebl (jirka@5z.com)",
	  "Jacob Berkman (jberkman@andrew.cmu.edu)",
	  "Miguel de Icaza (miguel@kernel.org)",
	  "Federico Mena (quartic@gimp.org)",
	  "Tom Tromey (tromey@cygnus.com)",
	  "Ian Main (imain@gtk.org)",
	  "Elliot Lee (sopwith@redhat.com)",
	  "Owen Taylor (otaylor@redhat.com)",
	N_("Many many others ..."),
	/* ... from the Monty Pythons show...  */
	N_("and finally, The Knights Who Say ... NI!"),
	  NULL
	  };

	if (about) {
		gdk_window_show (about->window);
		gdk_window_raise (about->window);
		return;
	}

#ifdef ENABLE_NLS
	{
		int i=0;
		while (authors[i] != NULL) {
		       	authors[i]=_(authors[i]);
			i++;
		}
	}
#endif
	
	about = gnome_about_new ( _("The GNOME Panel"), VERSION,
			_("(C) 1997-2000 the Free Software Foundation"),
			(const gchar **)authors,
			_("This program is responsible for launching "
			"other applications, embedding small applets "
			"within itself, world peace, and random X crashes."),
			"gnome-gegl2.png");
	gtk_window_set_wmclass (GTK_WINDOW (about), "about_dialog", "Panel");
	gtk_signal_connect (GTK_OBJECT (about), "destroy",
			    GTK_SIGNAL_FUNC (gtk_widget_destroyed),
			    &about);
	gtk_signal_connect (GTK_OBJECT (about), "event",
			    GTK_SIGNAL_FUNC (check_for_screen), NULL);

	hbox = gtk_hbox_new (TRUE, 0);
	l = gnome_href_new ("http://www.wfp.org/",
			    _("End world hunger"));
	gtk_box_pack_start (GTK_BOX (hbox), l, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (about)->vbox),
			    hbox, TRUE, FALSE, 0);
	gtk_widget_show_all (hbox);

	if (commie_mode) {
		l = gtk_label_new (_("Running in \"Lockdown\" mode.  This "
				     "means your system administrator has "
				     "prohibited any changes to the panel's "
				     "configuration to take place."));
		gtk_widget_show (l);
		gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (about)->vbox),
				    l, FALSE, FALSE, 0);
	}

	gtk_widget_show (about);
}

static void
about_gnome_cb(GtkObject *object, char *program_path)
{
	if (gnome_execute_async (g_get_home_dir (), 1, &program_path)<0)
		panel_error_dialog (_("Can't execute 'About GNOME'"));
}

static void
activate_app_def (GtkWidget *widget, const char *item_loc)
{
	GnomeDesktopEntry *item = gnome_desktop_entry_load (item_loc);
	if (item != NULL) {
		char *curdir = g_get_current_dir ();
		chdir (g_get_home_dir ());

		gnome_desktop_entry_launch (item);
		gnome_desktop_entry_free (item);

		chdir (curdir);
		g_free (curdir);
	} else {
		panel_error_dialog (_("Can't load entry"));
	}
}

/* Copy a single file */
static void
copy_file_to_dir (const char *item, const char *to, mode_t mode)
{
	int fdin;
	int fdout;
	int chars;
	char buf[1024];
	char *toname;

	fdin = open (item, O_RDONLY);
	if (fdin < 0)
		return;

	toname = g_concat_dir_and_file (to, g_basename (item));
	fdout = open (toname, O_WRONLY | O_CREAT, mode);
	g_free (toname);
	if (fdout < 0) {
		close (fdin);
		return;
	}

	while ((chars = read(fdin, buf, sizeof(buf))) > 0) {
		write (fdout, buf, chars);
	}


	close (fdin);
	close (fdout);
}

static void
copy_fr_dir (DirRec *dr, const char *to)
{
	GSList *li;
	char *file_name;
	FILE *order_file = NULL;

	g_return_if_fail (dr != NULL);
	g_return_if_fail (to != NULL);

	file_name = g_concat_dir_and_file (dr->frec.name, ".directory");
	copy_file_to_dir (file_name, to, 0600);
	g_free (file_name);

	file_name = g_concat_dir_and_file (to, ".order");
	order_file = fopen (file_name, "a");
	g_free (file_name);

	for (li = dr->recs; li != NULL; li = li->next) {
		FileRec *fr = li->data;
		
		if (fr->type == FILE_REC_FILE) {
			copy_file_to_dir (fr->name, to, 0600);
		} else if (fr->type == FILE_REC_DIR) {
			char *newdir = g_concat_dir_and_file (to, g_basename (fr->name));
			if (panel_file_exists (newdir) ||
			    mkdir (newdir, 0700) == 0)
				copy_fr_dir ((DirRec *)fr, newdir);
			g_free (newdir);
		} else if (fr->type == FILE_REC_EXTRA) {
			if (strcmp (g_basename (fr->name),
				    ".order") != 0)
				copy_file_to_dir (fr->name, to, 0600);
		} else {
			continue;
		}

		if (order_file != NULL &&
		    g_basename (fr->name) != NULL)
			fprintf (order_file, "%s\n", g_basename (fr->name));
	}

	if (order_file != NULL)
		fclose (order_file);
}

static void
add_app_to_personal (GtkWidget *widget, const char *item_loc)
{
	char *to;

	to = gnome_util_home_file ("apps");

	if (g_file_test (item_loc, G_FILE_TEST_ISDIR)) {
		FileRec *fr = fr_get_dir (item_loc);
		if (fr != NULL) {
			char *newdir = g_concat_dir_and_file (to, g_basename (fr->name));
			if (panel_file_exists (newdir) ||
			    mkdir (newdir, 0700) == 0)
				copy_fr_dir ((DirRec *)fr, newdir);
			g_free(newdir);
		}
	} else {
		copy_file_to_dir (item_loc, to, 0600);
	}

	g_free (to);
}

void
panel_add_favourite (const char *source_dentry)
{
	add_app_to_personal (NULL, source_dentry);
}

static PanelWidget *
get_panel_from_menu_data(GtkWidget *menu, gboolean must_have)
{
	g_return_val_if_fail (menu != NULL, NULL);
	g_return_val_if_fail (GTK_IS_MENU(menu) || GTK_IS_MENU_ITEM(menu),
			      NULL);

	if(GTK_IS_MENU_ITEM(menu))
		menu = menu->parent;

	while(menu) {
		PanelWidget *panel = gtk_object_get_data(GTK_OBJECT(menu),
							 "menu_panel");
		if(panel) {
			if(IS_PANEL_WIDGET(panel))
				return panel;
			else
				g_warning("Menu is on crack");
		}
		menu = GTK_MENU_SHELL(menu)->parent_menu_shell;
	}
	if (must_have) {
		g_warning("Something went quite terribly wrong and we can't "
			  "find where this menu belongs");
		return panels->data;
	} else {
		return NULL;
	}
}

static void
setup_menu_panel(GtkWidget *menu)
{
	PanelWidget *menu_panel = gtk_object_get_data(GTK_OBJECT(menu),
						      "menu_panel");
	if(!menu_panel) {
		menu_panel = get_panel_from_menu_data(menu, TRUE);
		gtk_object_set_data(GTK_OBJECT(menu), "menu_panel", menu_panel);
	}
}

static GtkWidget *
menu_new(void)
{
	GtkWidget *ret;
	ret = hack_scroll_menu_new ();
	gtk_signal_connect(GTK_OBJECT(ret), "show",
			   GTK_SIGNAL_FUNC(setup_menu_panel), NULL);

	return ret;
}


/* the following is taken from gnome_stock, and beaten with a stick
 * until it worked with ArtPixBufs and alpha channel, it scales down
 * and makes a bi-level alpha channel at threshold of 0xff/2,
 * dest is the destination and src is the source, it will be scaled according
 * to their sizes */

/* Then further hacked by Mark Crichton to use GdkPixBufs */

static void
scale_down (GtkWidget *window, GtkStateType state,
	    GdkPixbuf *dest, GdkPixbuf *src)
{
	unsigned char *p, *p2, *p3;
	long x, y, w2, h2, xo, yo, x2, y2, i, x3, y3;
	long yw, xw, ww, hw, r, g, b, r2, g2, b2;
	int trans;
	int wo, ho, w, h;
	gboolean do_alpha, d_alpha;
	int d_channels, do_channels, d_rowstride, do_rowstride;
	guchar *d_pixels, *do_pixels;

	guchar baser = 0xd6;
	guchar baseg = 0xd6;
	guchar baseb = 0xd6;

	wo = gdk_pixbuf_get_width(src);
	ho = gdk_pixbuf_get_height(src);
	do_channels = gdk_pixbuf_get_n_channels(src);
	do_pixels = gdk_pixbuf_get_pixels(src);
	do_rowstride = gdk_pixbuf_get_rowstride(src);
	do_alpha = gdk_pixbuf_get_has_alpha(src);

	w = gdk_pixbuf_get_width(dest);
	h = gdk_pixbuf_get_height(dest);
	d_channels = gdk_pixbuf_get_n_channels(dest);
	d_pixels = gdk_pixbuf_get_pixels(dest);
	d_rowstride = gdk_pixbuf_get_rowstride(dest);
	d_alpha = gdk_pixbuf_get_has_alpha(dest);

	if (window) {
		GtkStyle *style = gtk_widget_get_style(window);
		baser = style->bg[state].red >> 8;
		baseg = style->bg[state].green >> 8;
		baseb = style->bg[state].blue >> 8;
	}

	ww = (wo << 8) / w;
	hw = (ho << 8) / h;
	h2 = h << 8;
	w2 = w << 8;
	for (y = 0; y < h2; y += 0x100) {
		yo = (y * ho) / h;
		y2 = yo & 0xff;
		yo >>= 8;
		p = d_pixels + (y>>8) * d_rowstride;
		for (x = 0; x < w2; x += 0x100) {
			xo = (x * wo) / w;
			x2 = xo & 0xff;
			xo >>= 8;
			p2 = do_pixels + xo*do_channels +
				yo * do_rowstride;

			r2 = g2 = b2 = 0;
			yw = hw;
			y3 = y2;
			trans = 1;
			while (yw) {
				xw = ww;
				x3 = x2;
				p3 = p2;
				r = g = b = 0;
				while (xw) {
					if ((0x100 - x3) < xw)
						i = 0x100 - x3;
					else
						i = xw;
					if(do_alpha &&
					   p3[3]<(0xff/2)) {
						r += baser * i;
						g += baseg * i;
						b += baseb * i;
					} else {
						trans = 0;
						r += p3[0] * i;
						g += p3[1] * i;
						b += p3[2] * i;
					}
					if(do_alpha)
						p3 += 4;
					else
						p3 += 3;
					xw -= i;
					x3 = 0;
				}
				if ((0x100 - y3) < yw) {
					r2 += r * (0x100 - y3);
					g2 += g * (0x100 - y3);
					b2 += b * (0x100 - y3);
					yw -= 0x100 - y3;
				} else {
					r2 += r * yw;
					g2 += g * yw;
					b2 += b * yw;
					yw = 0;
				}
				y3 = 0;
				p2 += do_rowstride;
			}
			if (trans) {
				/* we leave garbage there but the
				 * alpha is 0 so we don't care */
				p+=3;
				if(d_alpha)
					*(p++) = 0x00;
			} else {
				r2 /= ww * hw;
				g2 /= ww * hw;
				b2 /= ww * hw;
				*(p++) = r2 & 0xff;
				*(p++) = g2 & 0xff;
				*(p++) = b2 & 0xff;
				if(d_alpha)
					*(p++) = 0xff;
			}
		}
	}
}


static void
pixmap_unmap(GtkWidget *w, FakeIcon *fake)
{
	GtkWidget *parent = fake->fake->parent;

	if(global_config.hungry_menus)
		return;

	/* don't kill the fake now, we'll kill it with the fake pixmap */
	gtk_signal_disconnect_by_data(GTK_OBJECT(fake->fake), fake);
	/* this must be destroy, it's owned by a parent and this will remove
	 * and unref it */
	gtk_widget_destroy(fake->fake);

	fake_pixmap_from_fake(fake);
	gtk_container_add(GTK_CONTAINER(parent), fake->fake);
}

static void
fake_destroyed(GtkWidget *w, FakeIcon *fake)
{
	/* XXX: we need to work right, so even though this should not be
	 * necessary it's here for correctness sake.  It seems we might not
	 * handle everything correctly and this may be needed */
	icons_to_load = g_slist_remove (icons_to_load, fake);

	g_free (fake->file);
	fake->file = NULL;

	g_free (fake);
}

static gboolean
load_icons_handler (gpointer data)
{
	GtkWidget *parent;
	GtkWidget *pixmap = NULL;
	GtkWidget *toplevel;
	GdkPixbuf *pb, *pb2;
	GdkPixmap *gp;
	GdkBitmap *gm;

	FakeIcon *fake;

	if (icons_to_load == NULL) {
		load_icons_id = 0;
		return FALSE;
	}

	fake = icons_to_load->data;
	icons_to_load = g_slist_remove (icons_to_load, fake);

	parent = fake->fake->parent;
	
	/* don't kill the fake now, we'll kill it with the pixmap */
	gtk_signal_disconnect_by_data(GTK_OBJECT(fake->fake), fake);

	/* destroy and not unref, as it's already inside a parent */
	gtk_widget_destroy(fake->fake);
	fake->fake = NULL;

	pb = gdk_pixbuf_new_from_file (fake->file);
	if (pb == NULL) {
		g_free (fake->file);
		fake->file = NULL;
		g_free (fake);
		return TRUE;
	}
	
	pb2 = gdk_pixbuf_new(GDK_COLORSPACE_RGB, gdk_pixbuf_get_has_alpha(pb),
			     8, fake->size, fake->size);
	
	scale_down(parent, parent->state, pb2, pb);
	
	gdk_pixbuf_unref (pb);

	gdk_pixbuf_render_pixmap_and_mask (pb2, &gp, &gm, 128);
	
	gdk_pixbuf_unref(pb2);
	pixmap = gtk_pixmap_new(gp, gm);
	gdk_pixmap_unref(gp);

	if (gm != NULL)   /* Is this right?  Some icons dont seem to have alpha. */
		gdk_bitmap_unref(gm);
	
	fake->fake = pixmap;
	toplevel = gtk_widget_get_toplevel(parent);
	gtk_signal_connect_while_alive(GTK_OBJECT(toplevel), "unmap",
				       GTK_SIGNAL_FUNC(pixmap_unmap),
				       fake,
				       GTK_OBJECT(pixmap));
	gtk_signal_connect(GTK_OBJECT(pixmap), "destroy",
			   GTK_SIGNAL_FUNC(fake_destroyed), fake);
	
	gtk_widget_show(pixmap);
	gtk_container_add(GTK_CONTAINER(parent), pixmap);
	
	/* if still more we'll come back */
	return TRUE;
}

static void
fake_unmapped (GtkWidget *w, FakeIcon *fake)
{
	icons_to_load = g_slist_remove (icons_to_load, fake);
}

static void
fake_mapped(GtkWidget *w, FakeIcon *fake)
{
	if(!g_slist_find(icons_to_load, fake))
		icons_to_load = g_slist_append(icons_to_load, fake);
	if(!load_icons_id)
		load_icons_id = g_idle_add(load_icons_handler, NULL);
}


static void
fake_mapped_fake(GtkWidget *w, FakeIcon *fake)
{
	GtkWidget *toplevel;
	if(!g_slist_find(icons_to_load, fake))
		icons_to_load = g_slist_append(icons_to_load, fake);
	if(!load_icons_id)
		load_icons_id = g_idle_add(load_icons_handler, NULL);

	gtk_signal_disconnect_by_func(GTK_OBJECT(w),
				      GTK_SIGNAL_FUNC(fake_mapped_fake),
				      fake);

	toplevel = gtk_widget_get_toplevel(w);
	gtk_signal_connect_while_alive(GTK_OBJECT(toplevel), "map",
				       GTK_SIGNAL_FUNC(fake_mapped),
				       fake,
				       GTK_OBJECT(fake->fake));
	gtk_signal_connect_while_alive(GTK_OBJECT(toplevel), "unmap",
				       GTK_SIGNAL_FUNC(fake_unmapped),
				       fake,
				       GTK_OBJECT(fake->fake));
}

static GtkWidget *
fake_pixmap_from_fake(FakeIcon *fake)
{
	fake->fake = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	gtk_widget_show(fake->fake);
	gtk_signal_connect(GTK_OBJECT(fake->fake), "map",
			   GTK_SIGNAL_FUNC(fake_mapped_fake),
			   fake);
	gtk_signal_connect(GTK_OBJECT(fake->fake), "destroy",
			   GTK_SIGNAL_FUNC(fake_destroyed),
			   fake);

	return fake->fake;
}

static GtkWidget *
fake_pixmap_at_size (const char *file, int size)
{
	FakeIcon *fake;

	if ( ! panel_file_exists(file))
		return NULL;

	fake = g_new0 (FakeIcon, 1);
	fake->file = g_strdup (file);
	fake->size = size;

	return fake_pixmap_from_fake (fake);
}

/* replaces '/' with returns _'s, originally from gmenu */
static void
validate_for_filename(char *file)
{
	char *ptr;

	g_return_if_fail (file != NULL);
	
	ptr = file;
	while (*ptr != '\0') {
		if (*ptr == '/')
			*ptr = '_';
		ptr++;
	}
}

static void
really_add_new_menu_item (GtkWidget *d, int button, gpointer data)
{
	GnomeDEntryEdit *dedit = GNOME_DENTRY_EDIT(data);
	char *file, *dir = gtk_object_get_data(GTK_OBJECT(d), "dir");
	GnomeDesktopEntry *dentry;
	FILE *fp;

	if (button != 0) {
		gtk_widget_destroy (d);
		return;
	}

	g_return_if_fail (dir != NULL);

	dentry = gnome_dentry_get_dentry (dedit);

	if(dentry->exec == NULL ||
	   dentry->exec_length <= 0) {
		gnome_desktop_entry_free (dentry);
		panel_error_dialog (_("Cannot create an item with an empty "
				      "command"));
		return;
	}

	if(string_empty (dentry->name)) {
		g_free (dentry->name);
		dentry->name = g_strdup (_("untitled"));
	}

	/* assume we are making a new file */
	if (dentry->location == NULL) {
		int i = 2;
		char *name = g_strdup (dentry->name);

		if (string_empty (name)) {
			g_free (name);
			name = g_strdup ("huh-no-name");
		}

		validate_for_filename (name);

		dentry->location = g_strdup_printf ("%s/%s.desktop",
						    dir, name);

		while (panel_file_exists (dentry->location)) {
			g_free (dentry->location);
			dentry->location = g_strdup_printf ("%s/%s%d.desktop",
							    dir, name,
							    i ++);
		}
		g_free (name);
	}

	g_assert (dentry->location != NULL);

	file = g_concat_dir_and_file (dir, ".order");
	fp = fopen (file, "a");
	if (fp != NULL) {
		char *file2 = g_basename (dentry->location);
		if (file2 != NULL)
			fprintf(fp, "%s\n", file2);
		else
			g_warning (_("Could not get file from path: %s"), 
				   dentry->location);
		fclose (fp);
	} else {
		g_warning (_("Could not open .order file: %s"), file);
	}
	g_free (file);

	/* open for append, which will not harm any file and we will see if
	 * we have write privilages */
	fp = fopen (dentry->location, "a");
	if(fp == NULL) {
		panel_error_dialog (_("Could not open file '%s' for writing"),
				    dentry->location);
		return;
	}
	fclose(fp);

	gnome_desktop_entry_save(dentry);
	gnome_desktop_entry_free(dentry);

	gtk_widget_destroy(d);
}

static void
add_new_app_to_menu (GtkWidget *widget, const char *item_loc)
{
	GtkWidget *dialog, *notebook;
	GnomeDEntryEdit *dee;
	GList *types;

	dialog = gnome_dialog_new (_("Create menu item"),
				   GNOME_STOCK_BUTTON_OK,
				   GNOME_STOCK_BUTTON_CANCEL,
				   NULL);
	gtk_window_set_wmclass (GTK_WINDOW (dialog),
			       "create_menu_item", "Panel");
	gtk_window_set_policy (GTK_WINDOW (dialog), FALSE, FALSE, TRUE);
	
	notebook = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), notebook,
			    TRUE, TRUE, GNOME_PAD_SMALL);
	dee = GNOME_DENTRY_EDIT (gnome_dentry_edit_new_notebook (GTK_NOTEBOOK (notebook)));
	hack_dentry_edit (dee);

	types = NULL;
	types = g_list_append(types, "Application");
	types = g_list_append(types, "URL");
	types = g_list_append(types, "PanelApplet");
	gtk_combo_set_popdown_strings (GTK_COMBO (dee->type_combo), types);
	g_list_free(types);
	types = NULL;

#define SETUP_EDITABLE(entry_name)					\
	gnome_dialog_editable_enters					\
		(GNOME_DIALOG (dialog),					\
		 GTK_EDITABLE (gnome_dentry_get_##entry_name##_entry (dee)));

	SETUP_EDITABLE (name);
	SETUP_EDITABLE (comment);
	SETUP_EDITABLE (exec);
	SETUP_EDITABLE (tryexec);
	SETUP_EDITABLE (doc);

#undef SETUP_EDITABLE
	
	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (dee->type_combo)->entry),
			    "Application");

	gtk_object_set_data_full (GTK_OBJECT (dialog), "dir",
				  g_strdup (item_loc),
				  (GtkDestroyNotify)g_free);
	
	gtk_signal_connect (GTK_OBJECT (dialog), "clicked",
			    GTK_SIGNAL_FUNC (really_add_new_menu_item),
			    dee);

	/* YAIKES, the problem here is that the notebook will attempt
	 * to destroy the dedit, so if we unref it in the close handler,
	 * it will be finalized by the time the notebook will destroy it,
	 * dedit is just a horrible thing */
	gtk_signal_connect (GTK_OBJECT (dee), "destroy",
			    GTK_SIGNAL_FUNC (gtk_object_unref),
			    NULL);

	gnome_dialog_close_hides (GNOME_DIALOG(dialog), FALSE);

	gnome_dialog_set_default (GNOME_DIALOG(dialog), 0);

	gtk_widget_show_all (dialog);
	panel_set_dialog_layer (dialog);

	gtk_widget_grab_focus (gnome_dentry_get_name_entry (dee));
}

static void
remove_menuitem (GtkWidget *widget, ShowItemMenu *sim)
{
	const char *file;
	char *dir, buf[256], *order_in_name, *order_out_name;
	FILE *order_in_file, *order_out_file;

	g_return_if_fail (sim->item_loc != NULL);
	g_return_if_fail (sim->menuitem != NULL);

	gtk_widget_hide (sim->menuitem);

	if (unlink (sim->item_loc) < 0) {
		panel_error_dialog(_("Could not remove the menu item %s: %s\n"), 
				    sim->item_loc, g_strerror(errno));
		return;
	}

	file = g_basename (sim->item_loc);
	if (file == NULL) {
		g_warning (_("Could not get file name from path: %s"),
			  sim->item_loc);
		return;
	}

	dir = g_dirname (sim->item_loc);
	if (dir == NULL) {
		g_warning (_("Could not get directory name from path: %s"),
			  sim->item_loc);
		return;
	}
	
	order_in_name = g_concat_dir_and_file(dir, ".order");
	order_in_file = fopen(order_in_name, "r");

	if (order_in_file == NULL) {
		/*no .order file so we can just leave*/
		g_free (order_in_name);
		g_free (dir);
		return;
	}

	order_out_name = g_concat_dir_and_file(dir, ".order.tmp");
	order_out_file = fopen(order_out_name, "w");

	g_free (dir);

	if (order_out_file == NULL) {
		panel_error_dialog(_("Could not open .order file: %s\n%s"),
				   order_out_name,
				   g_unix_error_string(errno));

		g_free (order_in_name);
		g_free (order_out_name);
		fclose (order_in_file);
		return;
	}

	while (fgets (buf, sizeof(buf)-1, order_in_file) != NULL) {
		g_strchomp (buf);  /* remove trailing '\n' */
		if (strcmp (buf, file) != 0)
			fprintf (order_out_file, "%s\n", buf);
	}

	fclose (order_out_file);
	fclose (order_in_file);

	if (unlink (order_in_name) < 0) {
		panel_error_dialog(_("Could not remove old order file %s: %s\n"), 
				    order_in_name, g_strerror(errno));
		g_free (order_out_name);
		g_free (order_in_name);
		return;
	}

	if (rename (order_out_name, order_in_name) == -1) {
		panel_error_dialog(_("Could not rename tmp file: %s to %s\n%s"),
				   order_out_name, order_in_name,
				   g_unix_error_string(errno));
	}

	g_free (order_out_name);
	g_free (order_in_name);
}

static void
add_to_run_dialog (GtkWidget *widget, const char *item_loc)
{
	GnomeDesktopEntry *item =
		gnome_desktop_entry_load_unconditional (item_loc);
	if (item != NULL) {
		if (item->exec != NULL &&
		    item->exec[0] != NULL) {
			char *s;
			char **cmd = item->exec;
			if (strcmp (cmd[0], "NO_XALF") == 0) {
				cmd++;
			}
			s = g_strjoinv (" ", cmd);
			show_run_dialog_with_text (s);
			g_free (s);
		} else {
			panel_error_dialog (_("No 'Exec' field in entry"));
		}
		gnome_desktop_entry_free (item);
	} else {
		panel_error_dialog (_("Can't load entry"));
	}
}

static void
show_help_on (GtkWidget *widget, const char *item_loc)
{
	GnomeDesktopEntry *item =
		gnome_desktop_entry_load_unconditional (item_loc);
	if (item != NULL) {
		char *path = panel_gnome_kde_help_path (item->docpath);
		if (path != NULL) {
			gnome_url_show (path);
			g_free (path);
		}
		gnome_desktop_entry_free (item);
	} else {
		panel_error_dialog (_("Can't load entry"));
	}
}

static void
add_app_to_panel (GtkWidget *widget, const char *item_loc)
{
	PanelWidget *panel = get_panel_from_menu_data (widget, TRUE);
	Launcher *launcher;

	launcher = load_launcher_applet (item_loc, panel, 0, FALSE);

	if (launcher != NULL)
		launcher_hoard (launcher);
}


static void
add_drawers_from_dir (const char *dirname, const char *name,
		      int pos, PanelWidget *panel)
{
	AppletInfo *info;
	Drawer *drawer;
	PanelWidget *newpanel;
	GnomeDesktopEntry *item_info;
	char *dentry_name;
	const char *subdir_name;
	char *pixmap_name;
	char *p;
	char *filename = NULL;
	char *mergedir;
	GSList *list, *li;

	if(!panel_file_exists(dirname))
		return;

	dentry_name = g_concat_dir_and_file (dirname,
					     ".directory");
	item_info = gnome_desktop_entry_load (dentry_name);
	g_free (dentry_name);

	if(!name)
		subdir_name = item_info ? item_info->name : NULL;
	else
		subdir_name = name;
	pixmap_name = item_info?item_info->icon:NULL;

	if( ! load_drawer_applet (-1, pixmap_name, subdir_name,
				  panel, pos, FALSE) ||
	    applets_last == NULL) {
		g_warning("Can't load a drawer");
		return;
	}
	info = applets_last->data;
	g_assert(info!=NULL);
	if(info->type != APPLET_DRAWER) {
		g_warning("Something weird happened and we didn't get a drawer");
		return;
	}
	
	drawer = info->data;
	g_assert(drawer);
	newpanel = PANEL_WIDGET(BASEP_WIDGET(drawer->drawer)->panel);

	mergedir = fr_get_mergedir(dirname);
	
	list = get_mfiles_from_menudir(dirname);
	for(li = list; li!= NULL; li = li->next) {
		MFile *mfile = li->data;
		struct stat s;
		GnomeDesktopEntry *dentry;

		g_free (filename);
		if ( ! mfile->merged) {
			filename = g_concat_dir_and_file(dirname, mfile->name);
		} else if (mergedir != NULL) {
			filename = g_concat_dir_and_file(mergedir, mfile->name);
		} else {
			filename = NULL;
			continue;
		}

		if (stat (filename, &s) != 0) {
			continue;
		}

		if (S_ISDIR (s.st_mode)) {
			add_drawers_from_dir (filename, NULL, G_MAXINT/2,
					      newpanel);
			continue;
		}
			
		p = strrchr(filename,'.');
		if (p && (strcmp(p,".desktop")==0 || 
			  strcmp(p,".kdelnk")==0)) {
			/*we load the applet at the right
			  side, that is end of the drawer*/
			dentry = gnome_desktop_entry_load (filename);
			if (dentry) {
				Launcher *launcher;

				launcher =
					load_launcher_applet_full (filename,
								   dentry,
								   newpanel,
								   G_MAXINT/2,
								   FALSE);

				if (launcher != NULL)
					launcher_hoard (launcher);
			}
		}
	}
	g_free (filename);
	g_free (mergedir);

	free_mfile_list (list);
}

/*add a drawer with the contents of a menu to the panel*/
static void
add_menudrawer_to_panel(GtkWidget *w, gpointer data)
{
	MenuFinfo *mf = data;
	PanelWidget *panel = get_panel_from_menu_data (w, TRUE);
	g_return_if_fail(mf);
	
	add_drawers_from_dir (mf->menudir, mf->dir_name, 0, panel);
}

static void
add_menu_to_panel (GtkWidget *widget, gpointer data)
{
	char *menudir = data;
	PanelWidget *panel;
	DistributionType distribution = get_distribution_type ();
	int flags = MAIN_MENU_SYSTEM_SUB | MAIN_MENU_USER_SUB |
		MAIN_MENU_APPLETS_SUB | MAIN_MENU_PANEL_SUB |
		MAIN_MENU_DESKTOP_SUB;
	
	/*guess distribution menus*/
	if (distribution != DISTRIBUTION_UNKNOWN)
		flags |= MAIN_MENU_DISTRIBUTION_SUB;

	/*guess KDE menus*/
	if(panel_file_exists(kde_menudir))
		flags |= MAIN_MENU_KDE_SUB;

	panel = get_panel_from_menu_data (widget, TRUE);

	load_menu_applet (menudir, flags, TRUE, FALSE, NULL, panel, 0, FALSE);
}

/*most of this function stolen from the real gtk_menu_popup*/
static void
restore_grabs(GtkWidget *w, gpointer data)
{
	GtkWidget *menu_item = data;
	GtkMenu *menu = GTK_MENU(menu_item->parent); 
	GtkWidget *xgrab_shell;
	GtkWidget *parent;
	/* Find the last viewable ancestor, and make an X grab on it
	 */
	parent = GTK_WIDGET (menu);
	xgrab_shell = NULL;
	while (parent) {
		gboolean viewable = TRUE;
		GtkWidget *tmp = parent;

		while (tmp) {
			if (!GTK_WIDGET_MAPPED (tmp)) {
				viewable = FALSE;
				break;
			}
			tmp = tmp->parent;
		}

		if (viewable)
			xgrab_shell = parent;

		parent = GTK_MENU_SHELL (parent)->parent_menu_shell;
	}

	/*only grab if this HAD a grab before*/
	if (xgrab_shell && (GTK_MENU_SHELL (xgrab_shell)->have_xgrab)) {
		GdkCursor *cursor = gdk_cursor_new (GDK_ARROW);

		GTK_MENU_SHELL (xgrab_shell)->have_xgrab = 
			(gdk_pointer_grab (xgrab_shell->window, TRUE,
					   GDK_BUTTON_PRESS_MASK |
					   GDK_BUTTON_RELEASE_MASK |
					   GDK_ENTER_NOTIFY_MASK |
					   GDK_LEAVE_NOTIFY_MASK,
					   NULL, cursor, 0) == 0);
		gdk_cursor_destroy (cursor);
	}
	
	gtk_grab_add (GTK_WIDGET (menu));
}

static void
ditem_properties_clicked (GtkWidget *w, int button, gpointer data)
{
	GnomeDEntryEdit *dee = gtk_object_get_user_data (GTK_OBJECT (w));
	GnomeDesktopEntry *dentry = data;

	if (button == HELP_BUTTON) {
		panel_show_help ("launchers.html");
	} else if (button == REVERT_BUTTON) {
		if (dentry != NULL)
			gnome_dentry_edit_set_dentry (dee, dentry);
		else
			gnome_dentry_edit_clear (dee);
	} else {
		gnome_dialog_close (GNOME_DIALOG (w));
	}
}

static gboolean
ditem_properties_apply_timeout (gpointer data)
{
	GtkObject *dedit = data;
	GnomeDesktopEntry *dentry;

	gtk_object_remove_data (dedit, "apply_timeout");

	dentry = gnome_dentry_get_dentry (GNOME_DENTRY_EDIT (dedit));
	dentry->location = g_strdup (gtk_object_get_data (dedit, "location"));
	gnome_desktop_entry_save (dentry);
	gnome_desktop_entry_free (dentry);

	return FALSE;
}

/* 
 * Will save after 5 seconds of no changes.  If something is changed, the save
 * is postponed to another 5 seconds.  This seems to be a saner behaviour,
 * then just saving every N seconds.
 */
static void
ditem_properties_changed (GtkObject *dedit, gpointer data)
{
	gpointer timeout_data = gtk_object_get_data (dedit, "apply_timeout");
	guint timeout = GPOINTER_TO_UINT (timeout_data);

	gtk_object_remove_data (dedit, "apply_timeout");

	if (timeout != 0)
		gtk_timeout_remove (timeout);

	/* Will save after 5 seconds */
	timeout = gtk_timeout_add (5 * 1000,
				   ditem_properties_apply_timeout,
				   dedit);

	gtk_object_set_data (dedit, "apply_timeout",
			     GUINT_TO_POINTER (timeout));
}


static void
ditem_properties_close (GtkWidget *widget, gpointer data)
{
	GtkObject *dedit = data;
	gpointer timeout_data = gtk_object_get_data (dedit, "apply_timeout");
	guint timeout = GPOINTER_TO_UINT (timeout_data);

	gtk_object_remove_data (dedit, "apply_timeout");

	if (timeout != 0) {
		gtk_timeout_remove (timeout);

		ditem_properties_apply_timeout (dedit);
	}
}

static gboolean
is_item_writable (const ShowItemMenu *sim)
{
	errno = 0;
	if (sim->item_loc != NULL &&
	    /*A HACK: but it works, don't have it edittable if it's redhat
	      menus as they are auto generated!*/
	    strstr (sim->item_loc,"/.gnome/apps-redhat/") == NULL &&
	    /*if it's a kdelnk file, don't let it be editted*/
	    ! is_ext (sim->item_loc, ".kdelnk") &&
	    access (sim->item_loc, W_OK) == 0) {
#ifdef PANEL_DEBUG
		puts (sim->item_loc);
#endif
		/*file exists and is writable, we're in bussines*/
		return TRUE;
	} else if ((sim->item_loc == NULL ||
		    errno == ENOENT) &&
		   sim->mf != NULL) {
		/*the dentry isn't there, check if we can write the
		  directory*/
		if (access (sim->mf->menudir, W_OK) == 0 &&
		   /*A HACK: but it works, don't have it edittable if it's redhat
		     menus as they are auto generated!*/
		   strstr (sim->mf->menudir, ".gnome/apps-redhat/") == NULL)
			return TRUE;
	}
	return FALSE;
}

static void
set_ditem_sensitive (GnomeDialog *dialog,
		     GnomeDEntryEdit *dedit,
		     ShowItemMenu *sim)
{
	gboolean sensitive;

	sensitive = is_item_writable (sim);

	gtk_widget_set_sensitive (gnome_dentry_edit_child1 (dedit),
				  sensitive);
	gtk_widget_set_sensitive (gnome_dentry_edit_child2 (dedit),
				  sensitive);

	gnome_dialog_set_sensitive (dialog, REVERT_BUTTON, sensitive);
}

static void
edit_dentry (GtkWidget *widget, ShowItemMenu *sim)
{
	GtkWidget *dialog, *notebook;
	GtkObject *dedit;
	GnomeDesktopEntry *dentry;
	GList *types = NULL;
	
	g_return_if_fail (sim != NULL);
	g_return_if_fail (sim->item_loc != NULL);

	dentry = gnome_desktop_entry_load_unconditional (sim->item_loc);
	/* We'll screw up a KDE menu entry if we edit it */
	if (dentry != NULL &&
	    dentry->is_kde) {
		gnome_desktop_entry_free (dentry);
		return;
	}

	/* watch the enum at the top of the file */
	dialog = gnome_dialog_new (_("Desktop entry properties"),
				   GNOME_STOCK_BUTTON_HELP,
				   _("Revert"),
				   GNOME_STOCK_BUTTON_CLOSE,
				   NULL);
	gnome_dialog_set_close (GNOME_DIALOG (dialog),
				FALSE /* click_closes */);

	notebook = gtk_notebook_new ();
	gtk_widget_show (notebook);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    notebook, TRUE, TRUE, 0);

	gtk_window_set_wmclass(GTK_WINDOW(dialog),
			       "desktop_entry_properties","Panel");
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	
	dedit = gnome_dentry_edit_new_notebook (GTK_NOTEBOOK (notebook));
	hack_dentry_edit (GNOME_DENTRY_EDIT (dedit));

	types = NULL;
	types = g_list_append (types, "Application");
	types = g_list_append (types, "URL");
	types = g_list_append (types, "PanelApplet");
	gtk_combo_set_popdown_strings(GTK_COMBO(GNOME_DENTRY_EDIT(dedit)->type_combo), types);
	g_list_free(types);
	types = NULL;

	/* This sucks, but there is no other way to do this with the current
	   GnomeDEntry API.  */

#define SETUP_EDITABLE(entry_name)					\
	gnome_dialog_editable_enters					\
		(GNOME_DIALOG (dialog),					\
		 GTK_EDITABLE (gnome_dentry_get_##entry_name##_entry  	\
			       (GNOME_DENTRY_EDIT (dedit))));

	SETUP_EDITABLE (name);
	SETUP_EDITABLE (comment);
	SETUP_EDITABLE (exec);
	SETUP_EDITABLE (tryexec);
	SETUP_EDITABLE (doc);

#undef SETUP_EDITABLE

	gtk_object_set_data_full (dedit, "location",
				  g_strdup (sim->item_loc),
				  (GtkDestroyNotify)g_free);

	if (dentry != NULL)
		gnome_dentry_edit_set_dentry (GNOME_DENTRY_EDIT (dedit), dentry);

	set_ditem_sensitive (GNOME_DIALOG (dialog),
			     GNOME_DENTRY_EDIT (dedit), sim);

	gtk_signal_connect (GTK_OBJECT (dedit), "changed",
			    GTK_SIGNAL_FUNC (ditem_properties_changed),
			    NULL);

	gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
			    GTK_SIGNAL_FUNC (ditem_properties_close),
			    dedit);

	/* YAIKES, the problem here is that the notebook will attempt
	 * to destroy the dedit, so if we unref it in the close handler,
	 * it will be finalized by the time the notebook will destroy it,
	 * dedit is just a horrible thing */
	gtk_signal_connect (GTK_OBJECT (dedit), "destroy",
			    GTK_SIGNAL_FUNC (gtk_object_unref),
			    NULL);

	gtk_object_set_user_data (GTK_OBJECT (dialog), dedit);

	if (dentry != NULL) {
		/* pass the dentry as the data to clicked */
		gtk_signal_connect_full (GTK_OBJECT (dialog), "clicked",
					 GTK_SIGNAL_FUNC (ditem_properties_clicked),
					 NULL,
					 dentry,
					 (GtkDestroyNotify) gnome_desktop_entry_free,
					 FALSE, FALSE);
	} else {
		gtk_signal_connect (GTK_OBJECT (dialog), "clicked",
				    GTK_SIGNAL_FUNC (ditem_properties_clicked),
				    NULL);
	}

	gtk_widget_show (dialog);

	gtk_widget_grab_focus
		(gnome_dentry_get_name_entry (GNOME_DENTRY_EDIT (dedit)));
}

static void
edit_direntry (GtkWidget *widget, ShowItemMenu *sim)
{
	GtkWidget *dialog, *notebook;
	GtkObject *dedit;
	char *dirfile = g_concat_dir_and_file (sim->mf->menudir, ".directory");
	GnomeDesktopEntry *dentry;
	GList *types = NULL;

	dentry = gnome_desktop_entry_load_unconditional(dirfile);
	/* We'll screw up a KDE menu entry if we edit it */
	if (dentry != NULL &&
	    dentry->is_kde) {
		gnome_desktop_entry_free (dentry);
		return;
	}

	/* watch the enum at the top of the file */
	dialog = gnome_dialog_new (_("Desktop entry properties"),
				   GNOME_STOCK_BUTTON_HELP,
				   _("Revert"),
				   GNOME_STOCK_BUTTON_CLOSE,
				   NULL);
	gnome_dialog_set_close (GNOME_DIALOG (dialog),
				FALSE /* click_closes */);

	notebook = gtk_notebook_new ();
	gtk_widget_show (notebook);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    notebook, TRUE, TRUE, 0);

	gtk_window_set_wmclass (GTK_WINDOW (dialog),
				"desktop_entry_properties", "Panel");
	gtk_window_set_policy (GTK_WINDOW(dialog), FALSE, FALSE, TRUE);
	
	dedit = gnome_dentry_edit_new_notebook (GTK_NOTEBOOK (notebook));
	hack_dentry_edit (GNOME_DENTRY_EDIT (dedit));

	types = NULL;
	types = g_list_append (types, "Directory");
	gtk_combo_set_popdown_strings (GTK_COMBO (GNOME_DENTRY_EDIT (dedit)->type_combo), types);
	g_list_free (types);
	types = NULL;

	if (dentry != NULL) {
		gnome_dentry_edit_set_dentry (GNOME_DENTRY_EDIT (dedit), dentry);
		gtk_object_set_data_full (dedit, "location",
					  g_strdup (dentry->location),
					  (GtkDestroyNotify)g_free);
		g_free (dirfile);
		dirfile = NULL;
	} else {
		dentry = g_new0 (GnomeDesktopEntry, 1);
		if (sim->mf->dir_name == NULL)
			dentry->name = g_strdup (_("Menu"));
		else
			dentry->name = g_strdup (sim->mf->dir_name);
		dentry->type = g_strdup ("Directory");
		/*we don't have to free dirfile here it will be freed as if
		  we had strduped it here*/
		gtk_object_set_data_full (dedit, "location", dirfile,
					  (GtkDestroyNotify)g_free);
		dirfile = NULL;
		gnome_dentry_edit_set_dentry(GNOME_DENTRY_EDIT(dedit), dentry);
	}

	/* This sucks, but there is no other way to do this with the current
	   GnomeDEntry API.  */

#define SETUP_EDITABLE(entry_name)					\
	gnome_dialog_editable_enters					\
		(GNOME_DIALOG (dialog),					\
		 GTK_EDITABLE (gnome_dentry_get_##entry_name##_entry  	\
			       (GNOME_DENTRY_EDIT (dedit))));

	SETUP_EDITABLE (name);
	SETUP_EDITABLE (comment);
	SETUP_EDITABLE (exec);
	SETUP_EDITABLE (tryexec);
	SETUP_EDITABLE (doc);

#undef SETUP_EDITABLE

	gtk_widget_set_sensitive (GNOME_DENTRY_EDIT(dedit)->exec_entry, FALSE);
	gtk_widget_set_sensitive (GNOME_DENTRY_EDIT(dedit)->tryexec_entry, FALSE);
	gtk_widget_set_sensitive (GNOME_DENTRY_EDIT(dedit)->doc_entry, FALSE);
	gtk_widget_set_sensitive (GNOME_DENTRY_EDIT(dedit)->type_combo, FALSE);
	gtk_widget_set_sensitive (GNOME_DENTRY_EDIT(dedit)->terminal_button, FALSE);

	set_ditem_sensitive (GNOME_DIALOG (dialog),
			     GNOME_DENTRY_EDIT (dedit), sim);

	gtk_signal_connect (GTK_OBJECT (dedit), "changed",
			    GTK_SIGNAL_FUNC (ditem_properties_changed),
			    NULL);

	gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
			    GTK_SIGNAL_FUNC (ditem_properties_close),
			    dedit);

	/* YAIKES, the problem here is that the notebook will attempt
	 * to destroy the dedit, so if we unref it in the close handler,
	 * it will be finalized by the time the notebook will destroy it,
	 * dedit is just a horrible thing */
	gtk_signal_connect (GTK_OBJECT (dedit), "destroy",
			    GTK_SIGNAL_FUNC (gtk_object_unref),
			    NULL);

	gtk_object_set_user_data (GTK_OBJECT (dialog), dedit);

	if (dentry != NULL) {
		/* pass the dentry as the data to clicked */
		gtk_signal_connect_full (GTK_OBJECT (dialog), "clicked",
					 GTK_SIGNAL_FUNC (ditem_properties_clicked),
					 NULL,
					 dentry,
					 (GtkDestroyNotify) gnome_desktop_entry_free,
					 FALSE, FALSE);
	} else {
		gtk_signal_connect (GTK_OBJECT (dialog), "clicked",
				    GTK_SIGNAL_FUNC (ditem_properties_clicked),
				    NULL);
	}

	gtk_widget_show(dialog);

	gtk_widget_grab_focus
		(gnome_dentry_get_name_entry (GNOME_DENTRY_EDIT (dedit)));
}

static void
show_item_menu (GtkWidget *item, GdkEventButton *bevent, ShowItemMenu *sim)
{
	GtkWidget *menuitem;
	char *tmp;
	GnomeDesktopEntry *ii;

	if (sim->menu == NULL) {
		sim->menu = menu_new ();

		gtk_object_set_data (GTK_OBJECT(sim->menu), "menu_panel",
				     get_panel_from_menu_data (sim->menuitem,
							       TRUE));
		
		gtk_signal_connect(GTK_OBJECT(sim->menu),"deactivate",
				   GTK_SIGNAL_FUNC(restore_grabs),
				   item);

		if (sim->type == 1) {
			ii = gnome_desktop_entry_load_unconditional (sim->item_loc);

			menuitem = gtk_menu_item_new ();
			gtk_widget_lock_accelerators (menuitem);
			if ( ! sim->applet)
				setup_menuitem (menuitem, 0,
						_("Add this launcher to panel"));
			else
				setup_menuitem (menuitem, 0,
						_("Add this applet as a launcher to panel"));
			gtk_menu_append (GTK_MENU (sim->menu), menuitem);
			gtk_signal_connect (GTK_OBJECT(menuitem), "activate",
					    GTK_SIGNAL_FUNC(add_app_to_panel),
					    (gpointer)sim->item_loc);

			if ( ! sim->applet) {
				menuitem = gtk_menu_item_new ();
				gtk_widget_lock_accelerators (menuitem);
				setup_menuitem (menuitem, 0,
						_("Add this to Favorites menu"));
				gtk_menu_append (GTK_MENU (sim->menu), menuitem);
				gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
						   GTK_SIGNAL_FUNC(add_app_to_personal),
						   (gpointer)sim->item_loc);
				/*ummmm slightly ugly but should work 99% of time*/
				if (strstr(sim->item_loc,"/.gnome/apps/") != NULL)
					gtk_widget_set_sensitive(menuitem,FALSE);
			}

			menuitem = gtk_menu_item_new ();
			gtk_widget_lock_accelerators (menuitem);
			setup_menuitem (menuitem, 0,
					_("Remove this item"));
			gtk_menu_append (GTK_MENU (sim->menu), menuitem);
			gtk_signal_connect (GTK_OBJECT(menuitem), "activate",
					    GTK_SIGNAL_FUNC (remove_menuitem),
					    sim);
			tmp = g_dirname(sim->item_loc);
			if (access (tmp, W_OK) != 0)
				gtk_widget_set_sensitive(menuitem,FALSE);
			g_free (tmp);
			gtk_signal_connect_object (GTK_OBJECT (menuitem),
						   "activate",
						   GTK_SIGNAL_FUNC (gtk_menu_shell_deactivate),
						   GTK_OBJECT (item->parent));

			if ( ! sim->applet) {
				menuitem = gtk_menu_item_new ();
				gtk_widget_lock_accelerators (menuitem);
				setup_menuitem (menuitem, 0,
						_("Put into run dialog"));
				gtk_menu_append (GTK_MENU (sim->menu),
						 menuitem);
				gtk_signal_connect
					(GTK_OBJECT(menuitem), "activate",
					 GTK_SIGNAL_FUNC(add_to_run_dialog),
					 (gpointer)sim->item_loc);
				gtk_signal_connect_object
					(GTK_OBJECT(menuitem),
					 "activate",
					 GTK_SIGNAL_FUNC(gtk_menu_shell_deactivate),
					 GTK_OBJECT(item->parent));
			}

			if (ii != NULL)
				tmp = panel_gnome_kde_help_path (ii->docpath);
			else
				tmp = NULL;

			if (tmp != NULL) {
				char *title;

				g_free (tmp);

				menuitem = gtk_menu_item_new ();
				gtk_widget_lock_accelerators (menuitem);
				title = g_strdup_printf (_("Help on %s"),
							 ii->name != NULL ?
							 ii->name :
							 _("Application"));
				setup_menuitem (menuitem, 0, title);
				g_free (title);
				gtk_menu_append (GTK_MENU (sim->menu),
						 menuitem);
				gtk_signal_connect
					(GTK_OBJECT(menuitem), "activate",
					 GTK_SIGNAL_FUNC(show_help_on),
					 (gpointer)sim->item_loc);
				gtk_signal_connect_object
					(GTK_OBJECT(menuitem),
					 "activate",
					 GTK_SIGNAL_FUNC(gtk_menu_shell_deactivate),
					 GTK_OBJECT(item->parent));
			}

			menuitem = gtk_menu_item_new ();
			gtk_widget_lock_accelerators (menuitem);
			/*when activated we must pop down the first menu*/
			gtk_signal_connect_object(GTK_OBJECT(menuitem),
						  "activate",
						  GTK_SIGNAL_FUNC(gtk_menu_shell_deactivate),
						  GTK_OBJECT(item->parent));
			gtk_signal_connect(GTK_OBJECT(menuitem),
					   "activate",
					   GTK_SIGNAL_FUNC(edit_dentry),
					   sim);
			setup_menuitem (menuitem, 0, _("Properties..."));
			gtk_menu_append (GTK_MENU (sim->menu), menuitem);


			gnome_desktop_entry_free (ii);
		}
		
		if (sim->mf != NULL) {
			GtkWidget *submenu;

			if (sim->type == 0) {
				submenu = sim->menu;
			} else {
				submenu = menu_new ();

				gtk_object_set_data
					(GTK_OBJECT(submenu), "menu_panel",
					 get_panel_from_menu_data (sim->menuitem,
								   TRUE));

				menuitem = gtk_menu_item_new ();
				gtk_widget_lock_accelerators (menuitem);
				setup_menuitem (menuitem, 0,
						_("Entire menu"));
				gtk_menu_append (GTK_MENU (sim->menu), menuitem);
				gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
							   submenu);
			}


			menuitem = gtk_menu_item_new ();
			gtk_widget_lock_accelerators (menuitem);
			setup_menuitem (menuitem, 0,
					_("Add this as drawer to panel"));
			gtk_menu_append (GTK_MENU (submenu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
				   GTK_SIGNAL_FUNC(add_menudrawer_to_panel),
				   sim->mf);

			menuitem = gtk_menu_item_new ();
			gtk_widget_lock_accelerators (menuitem);
			setup_menuitem (menuitem, 0,
					_("Add this as menu to panel"));
			gtk_menu_append (GTK_MENU (submenu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(add_menu_to_panel),
					   sim->mf->menudir);
			menuitem = gtk_menu_item_new ();
			gtk_widget_lock_accelerators (menuitem);
			setup_menuitem (menuitem, 0,
					_("Add this to Favorites menu"));
			gtk_menu_append (GTK_MENU (submenu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(add_app_to_personal),
					   sim->mf->menudir);
			/*ummmm slightly ugly but should work 99% of time*/
			if(strstr(sim->mf->menudir, "/.gnome/apps") != NULL)
				gtk_widget_set_sensitive(menuitem, FALSE);

			menuitem = gtk_menu_item_new ();
			gtk_widget_lock_accelerators (menuitem);
			setup_menuitem (menuitem, 0,
					_("Add new item to this menu"));
			gtk_menu_append (GTK_MENU (submenu), menuitem);
			/*when activated we must pop down the first menu*/
			gtk_signal_connect_object (GTK_OBJECT(menuitem),
						  "activate",
						  GTK_SIGNAL_FUNC(gtk_menu_shell_deactivate),
						  GTK_OBJECT(item->parent));
			gtk_signal_connect (GTK_OBJECT(menuitem), "activate",
					    GTK_SIGNAL_FUNC(add_new_app_to_menu),
					    sim->mf->menudir);
			if (access (sim->mf->menudir, W_OK) != 0)
				gtk_widget_set_sensitive (menuitem, FALSE);


			menuitem = gtk_menu_item_new ();
			gtk_widget_lock_accelerators (menuitem);
			/*when activated we must pop down the first menu*/
			gtk_signal_connect_object(GTK_OBJECT(menuitem),
						  "activate",
						  GTK_SIGNAL_FUNC(gtk_menu_shell_deactivate),
						  GTK_OBJECT(item->parent));
			gtk_signal_connect (GTK_OBJECT (menuitem),
					    "activate",
					    GTK_SIGNAL_FUNC (edit_direntry),
					    sim);
			setup_menuitem (menuitem, 0, _("Properties..."));
			gtk_menu_append (GTK_MENU (submenu), menuitem);
		}
	}
	
	gtk_menu_popup (GTK_MENU (sim->menu),
			NULL,
			NULL,
			NULL,
			NULL,
			bevent->button,
			bevent->time);
}

static gboolean
show_item_menu_b_cb(GtkWidget *w, GdkEvent *event, ShowItemMenu *sim)
{
	GdkEventButton *bevent = (GdkEventButton *)event;
	GtkWidget *item;
	
	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	/* no item menu in commie mode */
	if (commie_mode)
		return FALSE;
	
	item = w->parent->parent;
	show_item_menu (item, bevent, sim);
	
	return TRUE;
}

static gboolean
show_item_menu_mi_cb (GtkWidget *w, GdkEvent *event, ShowItemMenu *sim)
{
	GdkEventButton *bevent = (GdkEventButton *)event;

	/* no item menu in commie mode */
	if (commie_mode)
		return FALSE;
	
	if (event->type == GDK_BUTTON_PRESS &&
	    bevent->button == 3)
		show_item_menu (w, bevent, sim);
	
	return FALSE;
}

static void
destroy_item_menu(GtkWidget *w, ShowItemMenu *sim)
{
	/*NOTE: don't free item_loc or mf.. it's not ours and will be free'd
	  elsewhere*/
	if (sim->menu != NULL)
		gtk_widget_unref (sim->menu);
	sim->menu = NULL;
	g_free (sim);
}

/* This is a _horrible_ hack to have this here. This needs to be added to the
 * GTK+ menuing code in some manner.
 */
static void  
drag_end_menu_cb (GtkWidget *widget, GdkDragContext     *context)
{
  GtkWidget *xgrab_shell;
  GtkWidget *parent;

  /* Find the last viewable ancestor, and make an X grab on it
   */
  parent = widget->parent;
  xgrab_shell = NULL;
  while (parent)
    {
      gboolean viewable = TRUE;
      GtkWidget *tmp = parent;
      
      while (tmp)
	{
	  if (!GTK_WIDGET_MAPPED (tmp))
	    {
	      viewable = FALSE;
	      break;
	    }
	  tmp = tmp->parent;
	}
      
      if (viewable)
	xgrab_shell = parent;
      
      parent = GTK_MENU_SHELL (parent)->parent_menu_shell;
    }
  
  if (xgrab_shell && !GTK_MENU(xgrab_shell)->torn_off)
    {
      GdkCursor *cursor = gdk_cursor_new (GDK_ARROW);

      if ((gdk_pointer_grab (xgrab_shell->window, TRUE,
			     GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
			     GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
			     GDK_POINTER_MOTION_MASK,
			     NULL, cursor, GDK_CURRENT_TIME) == 0))
	{
	  if (gdk_keyboard_grab (xgrab_shell->window, TRUE,
				 GDK_CURRENT_TIME) == 0)
	    GTK_MENU_SHELL (xgrab_shell)->have_xgrab = TRUE;
	  else
	    {
	      gdk_pointer_ungrab (GDK_CURRENT_TIME);
	    }
	}

      gdk_cursor_destroy (cursor);
    }
}

static void  
drag_data_get_menu_cb (GtkWidget *widget, GdkDragContext     *context,
		       GtkSelectionData   *selection_data, guint info,
		       guint time, char *item_loc)
{
	gchar *uri_list = g_strconcat ("file:", item_loc, "\r\n", NULL);
	gtk_selection_data_set (selection_data,
				selection_data->target, 8, (guchar *)uri_list,
				strlen(uri_list));
	g_free(uri_list);
}

static void  
drag_data_get_string_cb (GtkWidget *widget, GdkDragContext     *context,
			 GtkSelectionData   *selection_data, guint info,
			 guint time, const char *string)
{
	gtk_selection_data_set (selection_data,
				selection_data->target, 8, (guchar *)string,
				strlen(string));
}
 
static void
setup_title_menuitem (GtkWidget *menuitem, GtkWidget *pixmap,
		      const char *title, MenuFinfo *mf)
{
	GtkWidget *label, *hbox=NULL, *align;
	IconSize size = global_config.use_large_icons 
		? MEDIUM_ICON_SIZE : SMALL_ICON_SIZE;
	label = gtk_label_new (title);
	gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show (label);

	if (gnome_preferences_get_menus_have_icons () ||
	    global_config.show_dot_buttons) {
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (hbox);
		gtk_container_add (GTK_CONTAINER (menuitem), hbox);
	} else
		gtk_container_add (GTK_CONTAINER (menuitem), label);
	
	if (gnome_preferences_get_menus_have_icons ()) {
		align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
		gtk_widget_show (align);
		gtk_container_set_border_width (GTK_CONTAINER (align), 1);

		if (pixmap) {
			gtk_container_add (GTK_CONTAINER (align), pixmap);
			gtk_widget_set_usize (align, size + 2, size - 4);
			gtk_widget_show (pixmap);
		} else
			gtk_widget_set_usize (align, size + 2, size - 4);

		gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);
	}

	if (gnome_preferences_get_menus_have_icons () ||
	    global_config.show_dot_buttons)
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
	if(mf) {
		ShowItemMenu *sim = g_new0(ShowItemMenu,1);
		sim->mf = mf;/*make sure you don't free this,
			       it's not ours!*/
		sim->type = 0;
		sim->menuitem = menuitem;
		gtk_signal_connect(GTK_OBJECT(menuitem), "event",
				   GTK_SIGNAL_FUNC(show_item_menu_mi_cb),
				   sim);
		gtk_signal_connect(GTK_OBJECT(menuitem), "destroy",
				   GTK_SIGNAL_FUNC(destroy_item_menu),
				   sim);
		if(global_config.show_dot_buttons) {
			GtkWidget *w = gtk_button_new_with_label(_("..."));
			gtk_signal_connect(GTK_OBJECT(w), "event",
					   GTK_SIGNAL_FUNC(show_item_menu_b_cb),
					   sim);
			gtk_widget_show(w);
			gtk_box_pack_end (GTK_BOX (hbox), w, FALSE, FALSE, 0);

			/*this is not really a problem for large fonts but it
			  makes the button smaller*/
			gtk_widget_set_usize (w, 0, 16);
		}
	}

	gtk_widget_show (menuitem);

}

static void
setup_full_menuitem_with_size (GtkWidget *menuitem, GtkWidget *pixmap, 
			       const char *title, const char *item_loc,
			       gboolean applet, 
			       IconSize icon_size,
			       MenuFinfo *mf)
			       
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "text/uri-list", 0, 0 }
	};

	GtkWidget *label, *hbox=NULL, *align;

	label = gtk_label_new (title);
	gtk_misc_set_alignment (GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show (label);
	
	if (gnome_preferences_get_menus_have_icons () ||
	    global_config.show_dot_buttons) {
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_widget_show (hbox);
		gtk_container_add (GTK_CONTAINER (menuitem), hbox);
	} else
		gtk_container_add (GTK_CONTAINER (menuitem), label);
	
	if (gnome_preferences_get_menus_have_icons ()) {
		align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
		gtk_widget_show (align);
		gtk_container_set_border_width (GTK_CONTAINER (align), 1);

		if (pixmap) {
			gtk_container_add (GTK_CONTAINER (align), pixmap);
			gtk_widget_set_usize (align, icon_size + 2,
					      icon_size - 4);
			gtk_widget_show (pixmap);
		} else
			gtk_widget_set_usize (align, icon_size + 2,
					      icon_size - 4);

		gtk_box_pack_start (GTK_BOX (hbox), align, FALSE, FALSE, 0);
	} else if(pixmap)
		gtk_widget_unref(pixmap);

	if (gnome_preferences_get_menus_have_icons () ||
	    global_config.show_dot_buttons)
		gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);

	if (item_loc != NULL) {
		ShowItemMenu *sim = g_new0 (ShowItemMenu, 1);
		sim->type = 1;
		sim->item_loc = item_loc; /*make sure you don't free this,
					    it's not ours!*/
		sim->applet = applet;
		sim->mf = mf;
		sim->menuitem = menuitem;
		gtk_signal_connect(GTK_OBJECT(menuitem), "event",
				   GTK_SIGNAL_FUNC(show_item_menu_mi_cb),
				   sim);
		gtk_signal_connect(GTK_OBJECT(menuitem), "destroy",
				   GTK_SIGNAL_FUNC(destroy_item_menu),
				   sim);
		if(global_config.show_dot_buttons) {
			GtkWidget *w = gtk_button_new_with_label(_("..."));
			gtk_signal_connect(GTK_OBJECT(w),"event",
					   GTK_SIGNAL_FUNC(show_item_menu_b_cb),
					   sim);
			gtk_widget_show(w);
			gtk_box_pack_end (GTK_BOX (hbox), w, FALSE, FALSE, 0);
			/*this is not really a problem for large fonts but it
			  makes the button smaller*/
			gtk_widget_set_usize (w, 0, 16);
		}

		/*applets have their own drag'n'drop*/
		if ( ! applet && ! commie_mode) {
			gtk_drag_source_set(menuitem,
					    GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
					    menu_item_targets, 1,
					    GDK_ACTION_COPY);

			gtk_signal_connect(GTK_OBJECT(menuitem), "drag_data_get",
					   drag_data_get_menu_cb,
					   (gpointer)item_loc);
			gtk_signal_connect(GTK_OBJECT(menuitem), "drag_end",
					   drag_end_menu_cb, NULL);
		}
	}

	gtk_widget_show (menuitem);
}

static void
setup_full_menuitem (GtkWidget *menuitem, GtkWidget *pixmap,
		     const char *title, const char *item_loc,
		     gboolean applet)
{
	setup_full_menuitem_with_size (menuitem, pixmap, title, 
				       item_loc, applet, SMALL_ICON_SIZE,
				       NULL);
}

void
setup_menuitem (GtkWidget *menuitem, GtkWidget *pixmap, const char *title)
{
	setup_full_menuitem(menuitem, pixmap, title, NULL, FALSE);
}

static void
setup_menuitem_with_size (GtkWidget *menuitem, GtkWidget *pixmap,
			  const char *title, int icon_size)
{
	setup_full_menuitem_with_size (menuitem, pixmap, title, NULL,
				       FALSE, icon_size, NULL);
}

static void
setup_directory_drag (GtkWidget *menuitem, const char *directory)
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "application/x-panel-directory", 0, 0 }
	};

	gtk_drag_source_set(menuitem,
			    GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
			    menu_item_targets, 1,
			    GDK_ACTION_COPY);
	
	gtk_signal_connect_full(GTK_OBJECT(menuitem), "drag_data_get",
			   GTK_SIGNAL_FUNC (drag_data_get_string_cb), NULL,
			   g_strdup (directory), (GtkDestroyNotify)g_free,
			   FALSE, FALSE);
	gtk_signal_connect(GTK_OBJECT(menuitem), "drag_end",
			   GTK_SIGNAL_FUNC (drag_end_menu_cb), NULL);
}

void
setup_internal_applet_drag (GtkWidget *menuitem, const char *applet_type)
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "application/x-panel-applet-internal", 0, 0 }
	};
	
	if (applet_type == NULL ||
	    commie_mode)
		return;
	
	gtk_drag_source_set (menuitem,
			     GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
			     menu_item_targets, 1,
			     GDK_ACTION_COPY);
	
	gtk_signal_connect_full (GTK_OBJECT (menuitem), "drag_data_get",
			   GTK_SIGNAL_FUNC (drag_data_get_string_cb), NULL,
			   g_strdup (applet_type), (GtkDestroyNotify)g_free,
			   FALSE, FALSE);
	gtk_signal_connect (GTK_OBJECT (menuitem), "drag_end",
			    GTK_SIGNAL_FUNC (drag_end_menu_cb), NULL);

}

static void
setup_applet_drag (GtkWidget *menuitem, const char *goad_id)
{
        static GtkTargetEntry menu_item_targets[] = {
		{ "application/x-panel-applet", 0, 0 }
	};
	
	if (goad_id == NULL)
		return;
	
	gtk_drag_source_set (menuitem,
			     GDK_BUTTON1_MASK|GDK_BUTTON2_MASK,
			     menu_item_targets, 1,
			     GDK_ACTION_COPY);

	/*note: goad_id should be alive long enough!!*/
	gtk_signal_connect (GTK_OBJECT (menuitem), "drag_data_get",
			    GTK_SIGNAL_FUNC (drag_data_get_string_cb),
			    (gpointer)goad_id);
	gtk_signal_connect (GTK_OBJECT (menuitem), "drag_end",
			    GTK_SIGNAL_FUNC (drag_end_menu_cb), NULL);

}

static void
add_drawer_to_panel (GtkWidget *widget, gpointer data)
{
	load_drawer_applet (-1, NULL, NULL,
			    get_panel_from_menu_data(widget, TRUE), 0, FALSE);
}

static void
add_logout_to_panel (GtkWidget *widget, gpointer data)
{
	load_logout_applet (get_panel_from_menu_data(widget, TRUE), 0, FALSE);
}

static void
add_lock_to_panel (GtkWidget *widget, gpointer data)
{
	load_lock_applet (get_panel_from_menu_data(widget, TRUE), 0, FALSE);
}

static void
add_run_to_panel (GtkWidget *widget, gpointer data)
{
	load_run_applet (get_panel_from_menu_data(widget, TRUE), 0, FALSE);
}

static void
try_add_status_to_panel (GtkWidget *widget, gpointer data)
{
	if(!load_status_applet(get_panel_from_menu_data(widget, TRUE),
			       0, FALSE)) {
		GtkWidget *mbox;
		mbox = gnome_message_box_new(_("You already have a status "
					       "dock on the panel. You can "
					       "only have one"),
					     GNOME_MESSAGE_BOX_INFO,
					     GNOME_STOCK_BUTTON_CLOSE,
					     NULL);
		gtk_window_set_wmclass(GTK_WINDOW(mbox),
				       "no_more_status_dialog","Panel");
		gtk_widget_show_all (mbox);
		panel_set_dialog_layer (mbox);
	}
}

static void
add_applet (GtkWidget *w, const char *item_loc)
{
	GnomeDesktopEntry *ii;
	char *goad_id;

	ii = gnome_desktop_entry_load(item_loc);
	if (ii == NULL) {
		panel_error_dialog (_("Can't load entry"));
		return;
	}

	goad_id = get_applet_goad_id_from_dentry(ii);
	gnome_desktop_entry_free(ii);
	
	if(!goad_id) {
		panel_error_dialog(_("Can't get goad_id from desktop entry!"));
		return;
	}
	load_extern_applet(goad_id, NULL,
			   get_panel_from_menu_data(w, TRUE),
			   -1, FALSE, FALSE);

	g_free(goad_id);
}

static void
add_launcher (GtkWidget *w, const char *item_loc)
{
	Launcher *launcher;

	launcher = load_launcher_applet
		(item_loc, get_panel_from_menu_data (w, TRUE), 0, FALSE);

	if (launcher != NULL)
		launcher_hoard (launcher);
}

static void
add_favourites (GtkWidget *w, const char *item_loc)
{
	panel_add_favourite (item_loc);
}

static void
destroy_mf(MenuFinfo *mf)
{
	if (mf->fr != NULL) {
		DirRec *dr = (DirRec *)mf->fr;
		mf->fr = NULL;
		dr->mfl = g_slist_remove(dr->mfl, mf);
	}
	g_free(mf->menudir);
	mf->menudir = NULL;
	g_free(mf->dir_name);
	mf->dir_name = NULL;
	g_free(mf->pixmap_name);
	mf->pixmap_name = NULL;
	g_free(mf);
}


static void
menu_destroy(GtkWidget *menu, gpointer data)
{
	GSList *mfl = gtk_object_get_data (GTK_OBJECT (menu), "mf");
	GSList *li;
	for (li = mfl; li != NULL; li = li->next) {
		MenuFinfo *mf = li->data;
		destroy_mf (mf);
	}
	g_slist_free (mfl);
	gtk_object_set_data (GTK_OBJECT (menu), "mf", NULL);
}

/*reread the applet menu, not a submenu*/
static void
check_and_reread_applet(Menu *menu, gboolean main_menu)
{
	GSList *mfl, *list;

	if (menu_need_reread (menu->menu)) {
		mfl = gtk_object_get_data (GTK_OBJECT (menu->menu), "mf");

		/*that will be destroyed in add_menu_widget*/
		if(main_menu) {
			add_menu_widget (menu, NULL, NULL, main_menu, TRUE);
		} else {
			GSList *dirlist = NULL;
			for(list = mfl; list != NULL;
			    list = list->next) {
				MenuFinfo *mf = list->data;
				dirlist = g_slist_append (dirlist,
							  g_strdup (mf->menudir));
			}
			add_menu_widget (menu, NULL, dirlist, main_menu, TRUE);

			g_slist_foreach (dirlist, (GFunc)g_free, NULL);
			g_slist_free (dirlist);
		}
	}
}

/* XXX: hmmm stolen GTK code, the gtk_menu_reposition only calls
   gtk_menu_position if the widget is drawable, but that's not the
   case when we want to do it*/
void
our_gtk_menu_position (GtkMenu *menu)
{
  GtkWidget *widget;
  GtkRequisition requisition;
  gint x, y;
 
  g_return_if_fail (menu != NULL);
  g_return_if_fail (GTK_IS_MENU (menu));

  widget = GTK_WIDGET (menu);

  gdk_window_get_pointer (NULL, &x, &y, NULL);

  /* We need the requisition to figure out the right place to
   * popup the menu. In fact, we always need to ask here, since
   * if one a size_request was queued while we weren't popped up,
   * the requisition won't have been recomputed yet.
   */
  gtk_widget_size_request (widget, &requisition);
      
  if (menu->position_func)
    (* menu->position_func) (menu, &x, &y, menu->position_func_data);
  else
    {
      gint screen_width;
      gint screen_height;
      int screen_basex, screen_basey;
      int screen;

      screen = multiscreen_screen_from_pos (x, y);
      
      if (screen < 0) {
	      screen_width = gdk_screen_width ();
	      screen_height = gdk_screen_height ();
	      screen_basex = 0;
	      screen_basey = 0;
      } else {
	      screen_width = multiscreen_width (screen);
	      screen_height = multiscreen_height (screen);
	      screen_basex = multiscreen_x (screen);
	      screen_basey = multiscreen_y (screen);
      }

      x -= screen_basex;
      y -= screen_basey;

      x -= 2;
      y -= 2;
      
      if ((x + requisition.width) > screen_width)
	x -= ((x + requisition.width) - screen_width);
      if (x < 0)
	x = 0;
      if ((y + requisition.height) > screen_height)
	y -= ((y + requisition.height) - screen_height);
      if (y < 0)
	y = 0;

      x += screen_basex;
      y += screen_basey;
    }
  
  gtk_widget_set_uposition (GTK_MENU_SHELL (menu)->active ?
			        menu->toplevel : menu->tearoff_window, 
			    x, y);
}

/* Stolen from GTK+
 * Reparent the menu, taking care of the refcounting
 */
static void 
my_gtk_menu_reparent (GtkMenu      *menu, 
		      GtkWidget    *new_parent, 
		      gboolean      unrealize)
{
  GtkObject *object = GTK_OBJECT (menu);
  GtkWidget *widget = GTK_WIDGET (menu);
  gboolean was_floating = GTK_OBJECT_FLOATING (object);

  gtk_object_ref (object);
  gtk_object_sink (object);

  if (unrealize)
    {
      gtk_object_ref (object);
      gtk_container_remove (GTK_CONTAINER (widget->parent), widget);
      gtk_container_add (GTK_CONTAINER (new_parent), widget);
      gtk_object_unref (object);
    }
  else
    gtk_widget_reparent (GTK_WIDGET (menu), new_parent);
  gtk_widget_set_usize (new_parent, -1, -1);
  
  if (was_floating)
    GTK_OBJECT_SET_FLAGS (object, GTK_FLOATING);
  else
    gtk_object_unref (object);
}

static gboolean
move_window_handler (gpointer data)
{
	int x, y, sx, sy, wx, wy, foox, fooy;
	GtkWidget *win = data;

	data = gtk_object_get_data (GTK_OBJECT (win), "move_speed_x");
	sx = GPOINTER_TO_INT (data);
	data = gtk_object_get_data (GTK_OBJECT (win), "move_speed_y");
	sy = GPOINTER_TO_INT (data);

	gdk_window_get_pointer (NULL, &x, &y, NULL);
	wx = win->allocation.x;
	wy = win->allocation.y;

	foox = wx + (win->allocation.width / 2);
	fooy = wy + (win->allocation.height / 2);

	if (sqrt ((foox - x)*(foox - x) + (fooy - y)*(fooy - y)) <
	    MAX (win->allocation.width, win->allocation.height)) {
		if (foox < x) sx -= 5;
		else sx += 5;
		if (fooy < y) sy -= 5;
		else sy += 5;
	} else {
		sx /= 2;
		sy /= 2;
	}
	
	if (sx > 50) sx = 50;
	else if (sx < -50) sx = -50;
	if (sy > 50) sy = 50;
	else if (sy < -50) sy = -50;

	wx += sx;
	wy += sy;

	if (wx < 0) wx = 0;
	if (wy < 0) wy = 0;
	if (wx + win->allocation.width > gdk_screen_width ())
		wx = gdk_screen_width () - win->allocation.width;
	if (wy + win->allocation.height > gdk_screen_height ())
		wy = gdk_screen_height () - win->allocation.height;

	gtk_widget_set_uposition (win, wx, wy);
	win->allocation.x = wx;
	win->allocation.y = wy;

	data = GINT_TO_POINTER (sx);
	gtk_object_set_data (GTK_OBJECT (win), "move_speed_x", data);
	data = GINT_TO_POINTER (sy);
	gtk_object_set_data (GTK_OBJECT (win), "move_speed_y", data);

	return TRUE;
}

static void
move_window_destroyed (GtkWidget *win)
{
	gpointer data = gtk_object_get_data (GTK_OBJECT (win), "move_window_handler");
	int handler = GPOINTER_TO_INT (data);

	if (handler != 0)
		gtk_timeout_remove (handler);
	gtk_object_remove_data (GTK_OBJECT (win), "move_window_handler");
}

static void
doblah (GtkWidget *window)
{
	gpointer data = gtk_object_get_data (GTK_OBJECT (window), "move_window_handler");
	int handler = GPOINTER_TO_INT (data);

	if (handler == 0) {
		handler = gtk_timeout_add (30, move_window_handler, window);
		data = GINT_TO_POINTER (handler);
		gtk_object_set_data (GTK_OBJECT (window), "move_window_handler", data);
		gtk_signal_connect (GTK_OBJECT (window), "destroy",
				    GTK_SIGNAL_FUNC (move_window_destroyed),
				    NULL);
	}
}

/*mostly stolen from GTK+ */
static gboolean
my_gtk_menu_window_event (GtkWidget *window,
			  GdkEvent  *event,
			  GtkWidget *menu)
{
	gboolean handled = FALSE;

	gtk_widget_ref (window);
	gtk_widget_ref (menu);

	switch (event->type) {
		static int foo = 0;
	case GDK_KEY_PRESS:
		if((event->key.state & GDK_CONTROL_MASK) && foo < 4) {
			switch (event->key.keyval) {
			case GDK_r:
			case GDK_R:
				if(foo == 3) { doblah (window); } foo = 0; break;
			case GDK_a:
			case GDK_A:
				if(foo == 2) { foo++; } else { foo = 0; } break;
			case GDK_e:
			case GDK_E:
				if(foo == 1) { foo++; } else { foo = 0; } break;
			case GDK_f:
			case GDK_F:
				if(foo == 0) { foo++; } else { foo = 0; } break;
			default:
				foo = 0;
			}
		}
		/* fall thru */
	case GDK_KEY_RELEASE:
		gtk_widget_event (menu, event);
		handled = TRUE;
		break;
	default:
		break;
	}

	gtk_widget_unref (window);
	gtk_widget_unref (menu);

	return handled;
}

static gulong wmclass_number = 0;

static char *
get_unique_tearoff_wmclass (void)
{
	static char buf[256];

	g_snprintf (buf, sizeof (buf), "panel_tearoff_%lu", wmclass_number++);

	return buf;
}

static void
show_tearoff_menu (GtkWidget *menu, const char *title, gboolean cursor_position,
		   int x, int y, const char *wmclass)
{
	GtkWidget *win;
	win = GTK_MENU(menu)->tearoff_window =
		gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass (GTK_WINDOW (win),
				wmclass, "Panel");
	gtk_widget_set_app_paintable (win, TRUE);
	gtk_signal_connect (GTK_OBJECT (win), "event",
			    GTK_SIGNAL_FUNC (my_gtk_menu_window_event), 
			    GTK_OBJECT (menu));
	gtk_widget_realize (win);
	      
	gdk_window_set_title (win->window, title);
	
	gdk_window_set_decorations (win->window, 
				    GDK_DECOR_ALL |
				    GDK_DECOR_RESIZEH |
				    GDK_DECOR_MINIMIZE |
				    GDK_DECOR_MAXIMIZE);
	gtk_window_set_policy (GTK_WINDOW (win), FALSE, FALSE, TRUE);
	my_gtk_menu_reparent (GTK_MENU (menu), win, FALSE);
	/* set sticky so that we mask the fact that we have no clue
	   how to restore non sticky windows */
	gnome_win_hints_set_state (win, gnome_win_hints_get_state (win) |
				   WIN_STATE_STICKY);
	
	GTK_MENU (menu)->torn_off = TRUE;

	if (cursor_position)
		our_gtk_menu_position (GTK_MENU (menu));
	else
		gtk_widget_set_uposition (win, x, y);

	gtk_widget_show (GTK_WIDGET (menu));
	gtk_widget_show (win);
}

static void
tearoff_destroyed (GtkWidget *tearoff, TearoffMenu *tm)
{
	tearoffs = g_slist_remove(tearoffs, tm);
	g_free(tm->title);
	tm->title = NULL;
	g_free(tm->wmclass);
	tm->wmclass = NULL;
	g_free(tm->special);
	tm->special = NULL;
	g_free(tm);
}

static void
tearoff_new_menu(GtkWidget *item, GtkWidget *menuw)
{
	GSList *mfl = gtk_object_get_data(GTK_OBJECT(menuw), "mf");
	GSList *list;
	GtkWidget *menu;
	GString *title;
	TearoffMenu *tm;
	char *wmclass;
	PanelWidget *menu_panel;
	
	if (mfl == NULL)
		return;

	menu = menu_new();

	menu_panel = get_panel_from_menu_data(menuw, TRUE);

	/*set the panel to use as the data*/
	gtk_object_set_data(GTK_OBJECT(menu), "menu_panel", menu_panel);

	gtk_signal_connect_object_while_alive(GTK_OBJECT(menu_panel),
		      "destroy", GTK_SIGNAL_FUNC(gtk_widget_unref),
		      GTK_OBJECT(menu));
	
	title = g_string_new(NULL);

	for(list = mfl; list != NULL; list = list->next) {
		MenuFinfo *mf = list->data;

		menu = create_menu_at_fr (menu,
					  mf->fr,
					  mf->applets,
					  mf->launcher_add,
					  mf->favourites_add,
					  mf->dir_name,
					  mf->pixmap_name,
					  TRUE /*fake_submenus*/,
					  FALSE /*force*/,
					  TRUE /*title*/);
		
		if (list != mfl)
			g_string_append_c (title, ' ');
		g_string_append (title, mf->dir_name);
	}

	wmclass = get_unique_tearoff_wmclass ();
	show_tearoff_menu(menu, title->str, TRUE, 0, 0, wmclass);

	tm = g_new0 (TearoffMenu, 1);
	tm->menu = menu;
	tm->mfl = gtk_object_get_data (GTK_OBJECT (menu), "mf");
	tm->special = NULL;
	tm->title = title->str;
	tm->wmclass = g_strdup (wmclass);
	gtk_signal_connect (GTK_OBJECT (menu), "destroy",
			    GTK_SIGNAL_FUNC (tearoff_destroyed), tm);

	tearoffs = g_slist_prepend (tearoffs, tm);

	g_string_free (title, FALSE);

	need_complete_save = TRUE;
}

static void
add_tearoff (GtkMenu *menu)
{
	GtkWidget *w;

	if (!gnome_preferences_get_menus_have_tearoff ()){
		g_warning (_("Adding tearoff when tearoffs are disabled"));
		return;
	}

	w = tearoff_item_new();
	gtk_widget_show(w);
	gtk_menu_prepend(menu, w);
	
	gtk_signal_connect(GTK_OBJECT(w), "activate",
			   GTK_SIGNAL_FUNC(tearoff_new_menu),
			   menu);
}

/*BTW, this also updates the fr entires */
gboolean
menu_need_reread(GtkWidget *menuw)
{
	GSList *mfl = gtk_object_get_data(GTK_OBJECT(menuw), "mf");
	GSList *list;
	gboolean need_reread = FALSE;

	/*if(!mfl)
	  g_warning("Weird menu doesn't have mf entry");*/

	if (GTK_MENU(menuw)->torn_off)
		return FALSE;

	if (gtk_object_get_data(GTK_OBJECT(menuw), "need_reread")) {
		need_reread = TRUE;
		gtk_object_remove_data(GTK_OBJECT(menuw), "need_reread");
	}
	
	/*check if we need to reread this*/
	for(list = mfl; list != NULL; list = list->next) {
		MenuFinfo *mf = list->data;
		if(mf->fake_menu ||
		   mf->fr == NULL) {
			if(mf->fr != NULL)
				mf->fr = fr_replace(mf->fr);
			else
				mf->fr = fr_get_dir(mf->menudir);
			need_reread = TRUE;
		} else {
			FileRec *fr;
			fr = fr_check_and_reread(mf->fr);
			if(fr != mf->fr ||
			   fr == NULL) {
				need_reread = TRUE;
				mf->fr = fr;
			}
		}
	}

	return need_reread;
}

void
submenu_to_display (GtkWidget *menuw, gpointer data)
{
	GSList *mfl, *list;
	gboolean add_launcher_hack;
	gboolean add_favourites_hack;
	gboolean favourites_hack;

	if (GTK_MENU(menuw)->torn_off)
		return;

	/*this no longer constitutes a bad hack, now it's purely cool :)*/
	if( ! menu_need_reread(menuw))
		return;

	/* EEEEEK! hacks */
	add_launcher_hack = 
		GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (menuw),
						      "_add_launcher_menu_hack_"));
	add_favourites_hack = 
		GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (menuw),
						      "_add_favourites_menu_hack_"));
	favourites_hack = 
		GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (menuw),
						      "_favourites_menu_hack_"));

	/* Note this MUST be destroy and not unref, unref would fuck
	 * up here, we don't hold a reference to them, so we must
	 * destroy them, menu shell will unref these */
	while(GTK_MENU_SHELL(menuw)->children)
		gtk_widget_destroy(GTK_MENU_SHELL(menuw)->children->data);

	if (add_launcher_hack) {
		create_add_launcher_menu (menuw, TRUE /* fake_submenus */);
	} else if (add_favourites_hack) {
		create_add_favourites_menu (menuw, TRUE /* fake_submenus */);
	} else {

		if (gnome_preferences_get_menus_have_tearoff ())
			add_tearoff(GTK_MENU(menuw));

		if (favourites_hack) {
			start_favourites_menu (menuw, TRUE /* fake_submenus */);
		}

		mfl = gtk_object_get_data(GTK_OBJECT(menuw), "mf");

		gtk_object_set_data(GTK_OBJECT(menuw), "mf", NULL);
		for(list = mfl;
		    list != NULL;
		    list = list->next) {
			MenuFinfo *mf = list->data;

			menuw = create_menu_at_fr (menuw,
						   mf->fr,
						   mf->applets,
						   mf->launcher_add,
						   mf->favourites_add,
						   mf->dir_name,
						   mf->pixmap_name,
						   TRUE /*fake_submenus*/,
						   FALSE /*force*/,
						   mf->title /*title*/);
			destroy_mf(mf);
		}
		g_slist_free(mfl);
	}

	our_gtk_menu_position(GTK_MENU(menuw));
}

GtkWidget *
start_favourites_menu (GtkWidget *menu,
		       gboolean fake_submenus)
{
	GtkWidget *menuitem;
	GtkWidget *m;

	if (menu == NULL) {
		menu = menu_new ();

		if (gnome_preferences_get_menus_have_tearoff ())
			add_tearoff(GTK_MENU(menu));
	}

	/* Add the favourites stuff here */
	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem_try_pixmap (menuitem,
				   "launcher-program.png",
				   /* Add to favourites */
				   _("Add from menu"),
				   SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	m = create_add_favourites_menu (NULL,
					TRUE /*fake_submenus*/);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), m);
	gtk_signal_connect (GTK_OBJECT (m),"show",
			    GTK_SIGNAL_FUNC (submenu_to_display), NULL);

	/* Eeeek, a hack, if this is set then the reloading
	 * function will use start_favourites_menu */
	gtk_object_set_data (GTK_OBJECT (menu),
			     "_favourites_menu_hack_",
			     GINT_TO_POINTER (1));

	return menu;
}


GtkWidget *
create_fake_menu_at (const char *menudir,
		     gboolean applets,
		     gboolean launcher_add,
		     gboolean favourites_add,
		     const char *dir_name,
		     const char *pixmap_name,
		     gboolean title)
{	
	MenuFinfo *mf;
	GtkWidget *menu;
	GSList *list;
	
	menu = menu_new ();

	mf = g_new0 (MenuFinfo, 1);
	mf->menudir = g_strdup (menudir);
	mf->applets = applets;
	mf->launcher_add = launcher_add;
	mf->favourites_add = favourites_add;
	mf->dir_name = g_strdup (dir_name);
	mf->pixmap_name = g_strdup (pixmap_name);
	mf->fake_menu = TRUE;
	mf->title = title;
	mf->fr = NULL;
	
	list = g_slist_prepend(NULL, mf);
	gtk_object_set_data(GTK_OBJECT(menu), "mf", list);
	
	gtk_signal_connect(GTK_OBJECT(menu), "destroy",
			   GTK_SIGNAL_FUNC(menu_destroy), NULL);
	
	return menu;
}

static gboolean
create_menuitem (GtkWidget *menu,
		 FileRec *fr,
		 gboolean applets,
		 gboolean launcher_add,
		 gboolean favourites_add,
		 gboolean fake_submenus,
		 gboolean *add_separator,
		 int *first_item,
		 MenuFinfo *mf)
{
	GtkWidget *menuitem, *sub, *pixmap;
	IconSize size = global_config.use_large_icons
		? MEDIUM_ICON_SIZE : SMALL_ICON_SIZE;
	char *itemname;
	
	g_return_val_if_fail (fr != NULL, FALSE);

	if(fr->type == FILE_REC_EXTRA)
		return FALSE;


	if(fr->type == FILE_REC_FILE && applets &&
	   !fr->goad_id) {
		g_warning(_("Can't get goad_id for applet, ignoring it"));
		return FALSE;
	}

	sub = NULL;
	if(fr->fullname) {
		itemname = g_strdup(fr->fullname);
	} else {
		char *p;
		itemname = g_strdup(g_basename(fr->name));
		p = strrchr(itemname, '.');
		if(p) *p = '\0';
	}

	if(fr->type == FILE_REC_DIR) {
		if(fake_submenus)
			sub = create_fake_menu_at (fr->name,
						   applets,
						   launcher_add,
						   favourites_add,
						   itemname,
						   fr->icon,
						   TRUE);
		else
			sub = create_menu_at_fr (NULL, fr,
						 applets,
						 launcher_add,
						 favourites_add,
						 itemname,
						 fr->icon,
						 fake_submenus,
						 FALSE /*force*/,
						 TRUE /*title*/);

		if (!sub) {
			g_free(itemname);
			return FALSE;
		}
	}

	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	if (sub) {
		gtk_menu_item_set_submenu (GTK_MENU_ITEM(menuitem), sub);
		gtk_signal_connect(GTK_OBJECT(sub), "show",
				   GTK_SIGNAL_FUNC(submenu_to_display), NULL);
	}

	pixmap = NULL;
	if (gnome_preferences_get_menus_have_icons ()) {
		if (fr->icon && panel_file_exists (fr->icon)) {
			pixmap = fake_pixmap_at_size (fr->icon, size);
			if (pixmap)
				gtk_widget_show (pixmap);
		}
	}

	if (sub == NULL &&
		   strstr(fr->name,"/applets/") &&
		   fr->goad_id != NULL) {
		setup_applet_drag (menuitem, fr->goad_id);
		setup_full_menuitem_with_size (menuitem, pixmap, itemname,
					       fr->name, TRUE, size, mf);
	} else {
		/*setup the menuitem, pass item_loc if this is not
		  a submenu, so that the item can be added,
		  we can be sure that the FileRec will live that long,
		  (when it dies, the menu will not be used again, it will
		  be recreated at the next available opportunity)*/
		setup_full_menuitem_with_size (menuitem, pixmap, itemname,
					       sub != NULL ? NULL : fr->name,
					       FALSE, size, mf);
	}

	if(*add_separator) {
		add_menu_separator(menu);
		(*first_item)++;
		*add_separator = FALSE;
	}
	
	if(fr->comment)
		gtk_tooltips_set_tip (panel_tooltips, menuitem,
				      fr->comment, NULL);
	gtk_menu_append (GTK_MENU (menu), menuitem);

	if(!sub) {
		if (launcher_add)
			gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
					    GTK_SIGNAL_FUNC (add_launcher),
					    fr->name);
		else if (favourites_add)
			gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
					    GTK_SIGNAL_FUNC (add_favourites),
					    fr->name);
		else if (applets)
			gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
					    GTK_SIGNAL_FUNC (add_applet),
					    fr->name);
		else
			gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
					    GTK_SIGNAL_FUNC (activate_app_def),
					    fr->name);
	}

	g_free (itemname);

	return TRUE;
}

GtkWidget *
create_menu_at (GtkWidget *menu,
		const char *menudir,
		gboolean applets,
		gboolean launcher_add,
		gboolean favourites_add,
		const char *dir_name,
		const char *pixmap_name,
		gboolean fake_submenus,
		gboolean force,
		gboolean title)
{
	return create_menu_at_fr (menu,
				  fr_get_dir (menudir),
				  applets,
				  launcher_add,
				  favourites_add,
				  dir_name,
				  pixmap_name,
				  fake_submenus,
				  force,
				  title);
}

static GtkWidget *
create_menu_at_fr (GtkWidget *menu,
		   FileRec *fr,
		   gboolean applets,
		   gboolean launcher_add,
		   gboolean favourites_add,
		   const char *dir_name,
		   const char *pixmap_name,
		   gboolean fake_submenus,
		   gboolean force,
		   gboolean title)
{	
	GSList *li;
	GSList *mfl = NULL;
	gboolean add_separator = FALSE;
	int first_item = 0;
	GtkWidget *menuitem;
	MenuFinfo *mf = NULL;
	DirRec *dr = (DirRec *)fr;
	GtkWidget *pixmap;
	IconSize size = global_config.use_large_icons
		? MEDIUM_ICON_SIZE : SMALL_ICON_SIZE;

	g_return_val_if_fail(!(fr&&fr->type!=FILE_REC_DIR),menu);
	
	if( ! force &&
	   fr == NULL)
		return menu;

	/* unfilled out, but the pointer will be correct */
	mf = g_new0 (MenuFinfo, 1);
	
	/*get this info ONLY if we haven't gotten it already*/
	if(dir_name == NULL)
		dir_name = (fr&&fr->fullname)?fr->fullname:_("Menu");
	if(pixmap_name == NULL)
		pixmap_name = (fr&&fr->icon)?fr->icon:gnome_folder;
	
	if(!menu) {
		menu = menu_new ();
		if (gnome_preferences_get_menus_have_tearoff ()) {
			add_tearoff(GTK_MENU(menu));
			first_item++;
		}
		gtk_signal_connect(GTK_OBJECT(menu), "destroy",
				   GTK_SIGNAL_FUNC(menu_destroy), NULL);
	} else {
		first_item = g_list_length(GTK_MENU_SHELL(menu)->children);
		mfl = gtk_object_get_data(GTK_OBJECT(menu), "mf");
		if(GTK_MENU_SHELL(menu)->children &&
		   !(GTK_MENU_SHELL(menu)->children->next == NULL &&
		     IS_TEAROFF_ITEM(GTK_MENU_SHELL(menu)->children->data)))
			add_separator = TRUE;
	}
	
	if(fr) {
		/* Last added points to the last fr list item that was successfuly
		 * added as a menu item */
		GSList *last_added = NULL;
		for(li = dr->recs; li != NULL; li = li->next) {
			FileRec *tfr = li->data;
			FileRec *pfr = last_added ? last_added->data : NULL;

			/* Add a separator between merged and non-merged menuitems */
			if (tfr->merged &&
			    pfr != NULL &&
			    ! pfr->merged) {
				add_menu_separator(menu);
			}

			if (tfr->type == FILE_REC_SEP)
				add_menu_separator (menu);				
			else if (create_menuitem (menu, tfr,
						  applets,
						  launcher_add,
						  favourites_add,
						  fake_submenus,
						  &add_separator,
						  &first_item,
						  mf))
				last_added = li;
		}
	}

	mf->menudir = g_strdup (fr->name);
	mf->applets = applets;
	mf->launcher_add = launcher_add;
	mf->favourites_add = favourites_add;
	mf->dir_name = g_strdup (dir_name);
	mf->pixmap_name = g_strdup (pixmap_name);
	mf->fake_menu = FALSE;
	mf->title = title;
	mf->fr = fr;
	if (fr != NULL) {
		DirRec *dr = (DirRec *)fr;
		dr->mfl = g_slist_prepend (dr->mfl, mf);
	}

	if (title && global_config.show_menu_titles) {
		char *menu_name;

		/*if we actually added anything*/
		if (first_item < g_list_length(GTK_MENU_SHELL(menu)->children)) {
			menuitem = gtk_menu_item_new();
			gtk_widget_lock_accelerators (menuitem);
			gtk_menu_insert(GTK_MENU(menu),menuitem,first_item);
			gtk_widget_show(menuitem);
			gtk_widget_set_sensitive(menuitem,FALSE);
			if (dir_name == NULL)
				menu_name = g_strdup (_("Menu"));
			else
				menu_name = g_strdup (dir_name);
		} else {
			if (dir_name == NULL)
				menu_name = g_strconcat (_("Menu"), _(" (empty)"), NULL);
			else
				menu_name = g_strconcat (dir_name, _(" (empty)"), NULL);
		}

		pixmap = NULL;
		if (gnome_preferences_get_menus_have_icons ()) {
			if (pixmap_name) {
				pixmap = fake_pixmap_at_size (pixmap_name, size);
			}
			if (!pixmap && gnome_folder && panel_file_exists (gnome_folder)) {
				pixmap = fake_pixmap_at_size (gnome_folder, size);
			}
		}

		if (pixmap != NULL)
			gtk_widget_show (pixmap);

		menuitem = title_item_new();
		gtk_widget_lock_accelerators (menuitem);
		setup_title_menuitem (menuitem, pixmap, menu_name, mf);
		gtk_menu_insert (GTK_MENU (menu), menuitem, first_item);

		g_free (menu_name);

		if ( ! commie_mode)
			setup_directory_drag (menuitem, mf->menudir);
	}

	/*add separator*/
	if (add_separator) {
		menuitem = gtk_menu_item_new();
		gtk_widget_lock_accelerators (menuitem);
		gtk_menu_insert(GTK_MENU(menu), menuitem, first_item);
		gtk_widget_show(menuitem);
		gtk_widget_set_sensitive(menuitem, FALSE);
		add_separator = FALSE;
	}

	mfl = g_slist_append (mfl, mf);

	gtk_object_set_data (GTK_OBJECT (menu), "mf", mfl);

	return menu;
}

static void
destroy_menu (GtkWidget *widget, gpointer data)
{
	Menu *menu = data;
	GtkWidget *prop_dialog = menu->prop_dialog;

	menu->prop_dialog = NULL;

	if (prop_dialog != NULL)
		gtk_widget_unref (prop_dialog);

	menu->button = NULL;

	if (menu->menu != NULL)
		gtk_widget_unref(menu->menu);
	menu->menu = NULL;
}

static void
free_menu (gpointer data)
{
	Menu *menu = data;

	g_free (menu->path);
	menu->path = NULL;

	g_free (menu->custom_icon_file);
	menu->custom_icon_file = NULL;

	g_free(menu);
}

static void
menu_deactivate (GtkWidget *w, gpointer data)
{
	Menu *menu = data;
	GtkWidget *panel = get_panel_parent (menu->button);

	/* allow the panel to hide again */
	if (IS_BASEP_WIDGET (panel))
		BASEP_WIDGET (panel)->autohide_inhibit = FALSE;
	BUTTON_WIDGET (menu->button)->in_button = FALSE;
	BUTTON_WIDGET (menu->button)->ignore_leave = FALSE;
	button_widget_up (BUTTON_WIDGET (menu->button));
	menu->age = 0;
}

static GtkWidget *
create_applets_menu (GtkWidget *menu, gboolean fake_submenus, gboolean title)
{
	GtkWidget *applet_menu;
	char *menudir = gnome_datadir_file ("applets");

	if (menudir == NULL ||
	    ! g_file_test (menudir, G_FILE_TEST_ISDIR)) {
		g_free (menudir);
		return NULL;
	}
	
	applet_menu = create_menu_at (menu, menudir,
				      TRUE /* applets */,
				      FALSE /* launcher_add */,
				      FALSE /* favourites_add */,
				      _("Applets"),
				      "gnome-applets.png",
				      fake_submenus, FALSE, title);
	g_free (menudir);
	return applet_menu;
}

static void
find_empty_pos_array (int screen, int posscore[3][3])
{
	GSList *li;
	int i,j;
	PanelData *pd;
	BasePWidget *basep;
	
	int tx, ty;
	int w, h;
	gfloat sw, sw2, sh, sh2;

	sw2 = 2 * (sw = multiscreen_width (screen) / 3);
	sh2 = 2 * (sh = multiscreen_height (screen) / 3);
	
	for (li = panel_list; li != NULL; li = li->next) {
		pd = li->data;

		if (IS_DRAWER_WIDGET(pd->panel) ||
		    IS_FOOBAR_WIDGET (pd->panel))
			continue;

		basep = BASEP_WIDGET (pd->panel);
		
		if (basep->screen != screen)
			continue;

		basep_widget_get_pos (basep, &tx, &ty);
		tx -= multiscreen_x (screen);
		ty -= multiscreen_y (screen);
		basep_widget_get_size (basep, &w, &h);

		if (PANEL_WIDGET (basep->panel)->orient == PANEL_HORIZONTAL) {
			j = MIN (ty / sh, 2);
			ty = tx + w;
			if (tx < sw) posscore[0][j]++;
			if (tx < sw2 && ty > sw) posscore[1][j]++;
			if (ty > sw2) posscore[2][j]++;
		} else {
			i = MIN (tx / sw, 2);
			tx = ty + h;
			if (ty < sh) posscore[i][0]++;
			if (ty < sh2 && tx > sh) posscore[i][1]++;
			if (tx > sh2) posscore[i][2]++;
		}
	}
}

static void
find_empty_pos (int screen, gint16 *x, gint16 *y)
{
	int posscore[3][3] = { {0,0,0}, {0,512,0}, {0,0,0}};
	int i, j, lowi= 0, lowj = 0;

	find_empty_pos_array (screen, posscore);

	for (j = 2; j >= 0; j--) {
		for (i = 0; i < 3; i++) {
			if (posscore[i][j] < posscore[lowi][lowj]) {
				lowi = i;
				lowj = j;
			}
		}
	}

	*x = ((float)lowi * multiscreen_width (screen)) / 2.0;
	*y = ((float)lowj * multiscreen_height (screen)) / 2.0;

	*x += multiscreen_x (screen);
	*y += multiscreen_y (screen);
}

static BorderEdge
find_empty_edge (int screen)
{
	int posscore[3][3] = { {0,0,0}, {0,512,0}, {0,0,0}};
	int escore [4] = { 0, 0, 0, 0};
	BorderEdge edge = BORDER_BOTTOM;
	int low=512, i;

	find_empty_pos_array (screen, posscore);

	escore[BORDER_TOP] = posscore[0][0] + posscore[1][0] + posscore[2][0];
	escore[BORDER_RIGHT] = posscore[2][0] + posscore[2][1] + posscore[2][2];
	escore[BORDER_BOTTOM] = posscore[0][2] + posscore[1][2] + posscore[2][2];
	escore[BORDER_LEFT] = posscore[0][0] + posscore[0][1] + posscore[0][2];
	
	for (i = 0; i < 4; i++) {
		if (escore[i] < low) {
			edge = i;
			low = escore[i];
		}
	}
	return edge;
}

static void
create_new_panel (GtkWidget *w, gpointer data)
{
	PanelType type = GPOINTER_TO_INT (data);
	GdkColor bcolor = {0, 0, 0, 1};
	gint16 x, y;
	GtkWidget *panel = NULL;
	PanelWidget *menu_panel;
	int screen;

	g_return_if_fail (type != DRAWER_PANEL);

	menu_panel = get_panel_from_menu_data (w, TRUE);
	if (menu_panel != NULL)
		screen = multiscreen_screen_from_panel
			(menu_panel->panel_parent);
	else
		screen = 0;

	switch (type) {
	case ALIGNED_PANEL: 
		find_empty_pos (screen, &x, &y);
		panel = aligned_widget_new (screen,
					    ALIGNED_LEFT,
					    BORDER_TOP,
					    BASEP_EXPLICIT_HIDE,
					    BASEP_SHOWN,
					    BASEP_LEVEL_DEFAULT,
					    TRUE,
					    SIZE_STANDARD,
					    TRUE,
					    TRUE,
					    PANEL_BACK_NONE,
					    NULL,
					    TRUE, FALSE, TRUE,
					    &bcolor);
		panel_setup (panel);
		gtk_widget_show (panel);
		basep_widget_set_pos (BASEP_WIDGET (panel), x, y);
		break;
	case EDGE_PANEL: 
		panel = edge_widget_new (screen,
					 find_empty_edge (screen),
					 BASEP_EXPLICIT_HIDE,
					 BASEP_SHOWN,
					 BASEP_LEVEL_DEFAULT,
					 TRUE,
					 SIZE_STANDARD,
					 TRUE,
					 TRUE,
					 PANEL_BACK_NONE,
					 NULL,
					 TRUE, FALSE, TRUE,
					 &bcolor);
		panel_setup (panel);
		gtk_widget_show (panel);	
		break;
	case SLIDING_PANEL:
		find_empty_pos (screen, &x, &y);
		panel = sliding_widget_new (screen,
					    SLIDING_ANCHOR_LEFT, 0,
					    BORDER_TOP,
					    BASEP_EXPLICIT_HIDE,
					    BASEP_SHOWN,
					    BASEP_LEVEL_DEFAULT,
					    TRUE,
					    SIZE_STANDARD,
					    TRUE, TRUE,
					    PANEL_BACK_NONE,
					    NULL, TRUE, FALSE, TRUE,
					    &bcolor);
		panel_setup (panel);
		gtk_widget_show (panel);	
		basep_widget_set_pos (BASEP_WIDGET (panel), x, y);
		break;
	case FLOATING_PANEL:
		find_empty_pos (screen, &x, &y);
		panel = floating_widget_new (screen,
					     x, y,
					     PANEL_VERTICAL,
					     BASEP_EXPLICIT_HIDE,
					     BASEP_SHOWN,
					     BASEP_LEVEL_DEFAULT,
					     FALSE,
					     SIZE_STANDARD,
					     TRUE, TRUE,
					     PANEL_BACK_NONE,
					     NULL, TRUE, FALSE, TRUE,
					     &bcolor);
		panel_setup (panel);
		gtk_widget_show (panel);
		basep_widget_set_pos (BASEP_WIDGET (panel), x, y);
		break;
	case FOOBAR_PANEL: {
		GtkWidget *dialog;
		char *s;
		if (!foobar_widget_exists (screen)) {
			panel = foobar_widget_new (screen);

			/* Don't translate the first part of this string */
			s = conditional_get_string
				("/panel/Config/clock_format",
				 _("%I:%M:%S %p"), NULL);
			if (s != NULL)
				foobar_widget_set_clock_format (FOOBAR_WIDGET (panel), s);
			g_free (s);

			panel_setup (panel);
			gtk_widget_show (panel);
			break;
		}
		s = _("You can only have one menu panel at a time.");
		
		dialog = gnome_message_box_new (s, 
						GNOME_MESSAGE_BOX_ERROR,
						GNOME_STOCK_BUTTON_OK,
						NULL);
		gtk_widget_show_all (dialog);
		panel_set_dialog_layer (dialog);
		break;
	}
	default: break;
	}

	if (panel == NULL)
		return;
		
	panels_to_sync = TRUE;
}

static GtkWidget * create_add_panel_submenu (gboolean tearoff);

static void
add_panel_tearoff_new_menu(GtkWidget *w, gpointer data)
{
	TearoffMenu *tm;
	char *wmclass = get_unique_tearoff_wmclass ();
	GtkWidget *menu = create_add_panel_submenu (FALSE);
	PanelWidget *menu_panel;

	menu_panel = get_panel_from_menu_data (w, TRUE);

	/*set the panel to use as the data*/
	gtk_object_set_data (GTK_OBJECT (menu), "menu_panel", menu_panel);
	gtk_signal_connect_object_while_alive
		(GTK_OBJECT (menu_panel), "destroy",
		 GTK_SIGNAL_FUNC (gtk_widget_unref),
		 GTK_OBJECT(menu));

	show_tearoff_menu (menu, _("Create panel"), TRUE, 0, 0, wmclass);

	tm = g_new0 (TearoffMenu, 1);
	tm->menu = menu;
	tm->mfl = NULL;
	tm->title = g_strdup (_("Create panel"));
	tm->special = g_strdup ("ADD_PANEL");
	tm->wmclass = g_strdup (wmclass);
	gtk_signal_connect(GTK_OBJECT(menu), "destroy",
			   GTK_SIGNAL_FUNC(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend (tearoffs, tm);
}

static GtkWidget *
create_add_panel_submenu (gboolean tearoff)
{
	GtkWidget *menu, *menuitem;

	menu = menu_new ();
	
	if (tearoff &&
	    gnome_preferences_get_menus_have_tearoff ()) {
		menuitem = tearoff_item_new ();
		gtk_widget_show (menuitem);
		gtk_menu_prepend (GTK_MENU (menu), menuitem);
	
		gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
				    GTK_SIGNAL_FUNC (add_panel_tearoff_new_menu),
				    NULL);
	}

	menuitem = gtk_menu_item_new ();
	setup_menuitem (menuitem, 0, _("Menu panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(create_new_panel),
			   GINT_TO_POINTER(FOOBAR_PANEL));
 	
	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem (menuitem, 0, _("Edge panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(create_new_panel),
			   GINT_TO_POINTER(EDGE_PANEL));

	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem (menuitem, 0, _("Aligned panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(create_new_panel),
			   GINT_TO_POINTER(ALIGNED_PANEL));

	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem (menuitem, 0, _("Sliding panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(create_new_panel),
			   GINT_TO_POINTER(SLIDING_PANEL));
	
	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem (menuitem, 0, _("Floating panel"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(create_new_panel),
			   GINT_TO_POINTER(FLOATING_PANEL));

	return menu;
}

static void
setup_menuitem_try_pixmap (GtkWidget *menuitem, const char *try_file,
			   const char *title, IconSize icon_size)
{
	char *file = NULL;

	if (!gnome_preferences_get_menus_have_icons ()) {
		setup_menuitem_with_size (menuitem,
					  NULL /*pixmap */,
					  title,
					  icon_size);
		return;
	}

	if (try_file) {
		file = gnome_pixmap_file (try_file);
		if (!file)
			g_warning (_("Cannot find pixmap file %s"), try_file);
	}
	
	if (!file)
		setup_menuitem_with_size (menuitem,
					  NULL /*pixmap */,
					  title,
					  icon_size);
	else
		setup_menuitem_with_size (menuitem,
					  fake_pixmap_at_size(file, icon_size),
					  title, icon_size);
	g_free (file);
}
	  

static GtkWidget *
create_system_menu (GtkWidget *menu, gboolean fake_submenus,
		    gboolean fake,
		    gboolean title,
		    gboolean launcher_add,
		    gboolean favourites_add)
{
	char *menudir = gnome_datadir_file ("gnome/apps");

	if (menudir &&
	    g_file_test (menudir, G_FILE_TEST_ISDIR)) {
		if(!fake || menu) {
			menu = create_menu_at (menu, menudir,
					       FALSE /* applets */,
					       launcher_add,
					       favourites_add,
					       _("Programs"),
					       "gnome-logo-icon-transparent.png",
					       fake_submenus, FALSE, title);
		} else {
			menu = create_fake_menu_at (menudir,
						    FALSE /* applets */,
						    launcher_add,
						    favourites_add,
						    _("Programs"),
						    "gnome-logo-icon-transparent.png",
						    title);
		}
	} else {
		/* show an error dialog for this only once, then just
		   use g_warning */
		static gboolean done_dialog = FALSE;
		if(!done_dialog) {
			panel_error_dialog(_("No system menus found!"));
			done_dialog = TRUE;
		} else
			g_warning(_("No system menus found!"));
	}
	g_free (menudir); 	
	return menu;
}

static GtkWidget *
create_user_menu (const char *title, const char *dir, GtkWidget *menu,
		  const char *pixmap, gboolean fake_submenus,
		  gboolean force, gboolean fake, gboolean gottitle,
		  gboolean launcher_add,
		  gboolean favourites_add)
{
	char *menudir = gnome_util_home_file (dir);
	if (!panel_file_exists (menudir))
		mkdir (menudir, 0755);
	if (!g_file_test (menudir, G_FILE_TEST_ISDIR)) {
		g_warning(_("Can't create the user menu directory"));
		g_free (menudir); 
		return menu;
	}
	
	if(!fake || menu) {
		menu = create_menu_at (menu, menudir,
				       FALSE /* applets */,
				       launcher_add,
				       favourites_add,
				       title, pixmap,
				       fake_submenus,
				       force, gottitle);
	} else {
		menu = create_fake_menu_at (menudir,
					    FALSE /* applets */,
					    launcher_add,
					    favourites_add,
					    title, pixmap, gottitle);
	}
	g_free (menudir); 
	return menu;
}

static GtkWidget *
create_distribution_menu (GtkWidget *menu,
			  gboolean fake_submenus,
			  gboolean fake,
			  gboolean title,
			  gboolean launcher_add,
			  gboolean favourites_add)
{
	const DistributionInfo *info = get_distribution_info ();
	gchar *pixmap_file, *menu_path;

	if (!info)
		return NULL;

	if (info->menu_icon != NULL)
		pixmap_file = gnome_pixmap_file (info->menu_icon);
	else
		pixmap_file = NULL;

	if (info->menu_path [0] != '/')
		menu_path = gnome_util_home_file (info->menu_path);
	else
		menu_path = g_strdup (info->menu_path);

	if (!fake || menu) {
		menu = create_menu_at (menu, menu_path,
				       FALSE /* applets */,
				       launcher_add,
				       favourites_add,
				       info->menu_name, pixmap_file,
				       fake_submenus, FALSE, title);
	} else {
		menu = create_fake_menu_at (menu_path,
					    FALSE /* applets */,
					    launcher_add,
					    favourites_add,
					    info->menu_name, pixmap_file,
					    title);
	}

	g_free (pixmap_file);
	g_free (menu_path);

	return menu;
}

static GtkWidget *
create_kde_menu (GtkWidget *menu, gboolean fake_submenus,
		 gboolean force, gboolean fake, gboolean title,
		 gboolean launcher_add,
		 gboolean favourites_add)
{
	char *pixmap_name;

	pixmap_name = g_concat_dir_and_file (kde_icondir, "exec.xpm");

	if(!fake || menu) {
		menu = create_menu_at (menu, 
				       kde_menudir,
				       FALSE /* applets */,
				       launcher_add,
				       favourites_add,
				       _("KDE menus"), 
				       pixmap_name,
				       fake_submenus,
				       force, title);
	} else {
		menu = create_fake_menu_at (kde_menudir,
					    FALSE /* applets */,
					    launcher_add,
					    favourites_add,
					    _("KDE menus"),
					    pixmap_name, title);
	}
	g_free (pixmap_name);
	return menu;
}

static GtkWidget *
create_add_launcher_menu (GtkWidget *menu, gboolean fake_submenus)
{
	if (menu == NULL)
		menu = menu_new ();

	/* Eeeek, a hack, if this is set then the reloading
	 * function will use create_add_launcher_menu, rather then
	 * the nomral way of reloading, as that would dump the
	 * submenus */
	gtk_object_set_data (GTK_OBJECT (menu),
			     "_add_launcher_menu_hack_",
			     GINT_TO_POINTER (1));

	create_system_menu (menu, fake_submenus, FALSE, TRUE, TRUE, FALSE);
	create_user_menu (_("Favorites"), "apps",
			  menu, "gnome-favorites.png",
			  fake_submenus, FALSE, FALSE, TRUE, TRUE, FALSE);

	add_menu_separator (menu);

	add_distribution_submenu (menu, fake_submenus,
				  TRUE /*launcher_add */,
				  FALSE /*favourites_add */);
	if (g_file_test (kde_menudir, G_FILE_TEST_ISDIR)) {
		add_kde_submenu (menu, fake_submenus,
				 TRUE /*launcher_add */,
				 FALSE /*favourites_add */);
	}

	return menu;
}

static GtkWidget *
create_add_favourites_menu (GtkWidget *menu, gboolean fake_submenus)
{
	if (menu == NULL)
		menu = menu_new ();

	/* Eeeek, a hack, if this is set then the reloading
	 * function will use create_add_favourites_menu, rather then
	 * the nomral way of reloading, as that would dump the
	 * submenus */
	gtk_object_set_data (GTK_OBJECT (menu),
			     "_add_favourites_menu_hack_",
			     GINT_TO_POINTER (1));

	create_system_menu (menu, fake_submenus, FALSE, TRUE,
			    FALSE /*launcher_add */,
			    TRUE /*favourites_add */);

	add_menu_separator (menu);

	add_distribution_submenu (menu, fake_submenus,
				  FALSE /*launcher_add */,
				  TRUE /*favourites_add */);
	if (g_file_test (kde_menudir, G_FILE_TEST_ISDIR)) {
		add_kde_submenu (menu, fake_submenus,
				 FALSE /*launcher_add */,
				 TRUE /*favourites_add */);
	}

	return menu;
}

static void
remove_panel (GtkWidget *widget)
{
	status_unparent (widget);
	gtk_widget_destroy (widget);
}

static void
remove_panel_accept (GtkWidget *w, GtkWidget *panelw)
{
	remove_panel (panelw);
}

static void
remove_panel_query (GtkWidget *w, gpointer data)
{
	GtkWidget *dialog;
	GtkWidget *panelw;
	PanelWidget *panel;

	panel = get_panel_from_menu_data(w, TRUE);
	panelw = panel->panel_parent;

	if (!IS_DRAWER_WIDGET (panelw) && base_panels == 1) {
		panel_error_dialog (_("You cannot remove your last panel."));
		return;
	}

	/* if there are no applets just remove the panel */
	if(!global_config.confirm_panel_remove || !panel->applet_list) {
		remove_panel (panelw);
		return;
	}

	dialog = gnome_message_box_new (_("When a panel is removed, the panel "
					  "and its\napplet settings are lost. "
					  "Remove this panel?"),
					GNOME_MESSAGE_BOX_QUESTION,
					GNOME_STOCK_BUTTON_YES,
					GNOME_STOCK_BUTTON_NO,
					NULL);
	
	gnome_dialog_button_connect (GNOME_DIALOG(dialog), 0,
				     GTK_SIGNAL_FUNC (remove_panel_accept),
				     panelw);
	gtk_signal_connect_object_while_alive (GTK_OBJECT(panelw), "destroy",
					       GTK_SIGNAL_FUNC(gtk_widget_destroy),
					       GTK_OBJECT(dialog));
	gtk_widget_show_all (dialog);
	panel_set_dialog_layer (dialog);
}

static void
panel_tearoff_new_menu(GtkWidget *w, gpointer data)
{
	TearoffMenu *tm;
	char *wmclass = get_unique_tearoff_wmclass();
	GtkWidget *menu = NULL;
	PanelWidget *menu_panel;

	int flags = GPOINTER_TO_INT (data);

	menu_panel = get_panel_from_menu_data (w, TRUE);

	menu = create_root_menu (NULL, TRUE, flags, FALSE,
				 IS_BASEP_WIDGET (menu_panel->panel_parent),
				 TRUE);

	gtk_object_set_data (GTK_OBJECT(menu), "menu_panel", menu_panel);
	gtk_signal_connect_object_while_alive(GTK_OBJECT(menu_panel),
		      "destroy", GTK_SIGNAL_FUNC(gtk_widget_unref),
		      GTK_OBJECT(menu));

	show_tearoff_menu(menu, _("Main Menu"),TRUE,0,0,wmclass);

	tm = g_new0(TearoffMenu,1);
	tm->menu = menu;
	tm->mfl = NULL;
	tm->title = g_strdup(_("Main Menu"));
	tm->special = g_strdup_printf("PANEL:%d", flags);
	tm->wmclass = g_strdup(wmclass);
	gtk_signal_connect(GTK_OBJECT(menu), "destroy",
			   GTK_SIGNAL_FUNC(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend(tearoffs,tm);
}

GtkWidget *
create_panel_root_menu(PanelWidget *panel, gboolean tearoff)
{
	GtkWidget *menu;

	menu = create_root_menu (NULL, TRUE, global_config.menu_flags, tearoff,
				 IS_BASEP_WIDGET (panel->panel_parent),
				 TRUE);

	gtk_object_set_data (GTK_OBJECT(menu), "menu_panel", panel);

	return menu;
}

static void
current_panel_config(GtkWidget *w, gpointer data)
{
	PanelWidget *panel = get_panel_from_menu_data(w, TRUE);
	GtkWidget *parent = panel->panel_parent;
	panel_config(parent);
}

static void
ask_about_launcher_cb(GtkWidget *w, gpointer data)
{
	ask_about_launcher(NULL, get_panel_from_menu_data(w, TRUE), 0, FALSE);
}

static void
ask_about_swallowing_cb(GtkWidget *w, gpointer data)
{
	ask_about_swallowing(get_panel_from_menu_data(w, TRUE), 0, FALSE);
}

static void
convert_setup (BasePWidget *basep, GtkType type)
{
	basep->pos = gtk_type_new (type);
	basep->pos->basep = basep;
	basep_widget_pre_convert_hook (basep);
	basep_pos_connect_signals (basep);
	update_config_type (basep);
}

static void
convert_to_panel(GtkWidget *widget, gpointer data)
{
	PanelType type = GPOINTER_TO_INT(data);
	PanelData *pd;
	int x, y;
	int w, h;
	BasePWidget *basep;
	BasePPos *old_pos;
	PanelWidget *cur_panel = get_panel_from_menu_data(widget, TRUE);

	g_return_if_fail(cur_panel != NULL);
	g_return_if_fail(IS_PANEL_WIDGET(cur_panel));

	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	g_assert (cur_panel->panel_parent);
	g_return_if_fail (IS_BASEP_WIDGET (cur_panel->panel_parent));

	basep = BASEP_WIDGET(cur_panel->panel_parent);

	pd = gtk_object_get_user_data (GTK_OBJECT (basep));
	if (pd->type == type)
		return;

	basep_widget_get_pos (basep, &x, &y);
	basep_widget_get_size (basep, &w, &h);

	old_pos = basep->pos;
	old_pos->basep = NULL;
	pd->type = type;

	/* for now, just ignore non-border types */
	switch (type) {
	case EDGE_PANEL: 
	{
		BorderEdge edge = BORDER_BOTTOM;
		convert_setup (basep, TYPE_EDGE_POS);

		if (IS_BORDER_POS (old_pos))
			edge = BORDER_POS (old_pos)->edge;
		else if (PANEL_WIDGET (cur_panel)->orient == PANEL_HORIZONTAL)
			edge = (y - multiscreen_y (basep->screen) >
				(multiscreen_height (basep->screen) / 2))
				? BORDER_BOTTOM : BORDER_TOP;
		else
			edge = (x - multiscreen_x (basep->screen) >
				(multiscreen_width (basep->screen) / 2))
				? BORDER_RIGHT : BORDER_LEFT;

		border_widget_change_edge (BORDER_WIDGET (basep), edge);
		break;
	}
	case ALIGNED_PANEL: 
	{
		gint mid, max;
		BorderEdge edge = BORDER_BOTTOM;
		AlignedAlignment align;

		convert_setup (basep, TYPE_ALIGNED_POS);

		if (IS_BORDER_POS (old_pos))
			edge = BORDER_POS (old_pos)->edge;
		else if (PANEL_WIDGET (cur_panel)->orient == PANEL_HORIZONTAL)
			edge = (y - multiscreen_y (basep->screen) >
				(multiscreen_height (basep->screen) / 2))
				? BORDER_BOTTOM : BORDER_TOP;
		else
			edge = (x - multiscreen_x (basep->screen) >
				(multiscreen_width (basep->screen) / 2))
				? BORDER_RIGHT : BORDER_LEFT;

		if (PANEL_WIDGET (cur_panel)->orient == PANEL_HORIZONTAL) {
			mid = x + w / 2 - multiscreen_x (basep->screen);
			max = multiscreen_width (basep->screen);
		} else {
			mid = y + h / 2 - multiscreen_y (basep->screen);
			max = multiscreen_height (basep->screen);
		}
	
		if (mid < max / 3)
			align = ALIGNED_LEFT;
		else if (mid < 2 * (max / 3))
			align = ALIGNED_CENTER;
		else
			align = ALIGNED_RIGHT;
		aligned_widget_change_align_edge (
			ALIGNED_WIDGET (basep), align, edge);
		break;
	}
	case SLIDING_PANEL:
	{
		gint val, max;
		BorderEdge edge = BORDER_BOTTOM;
		SlidingAnchor anchor;
		gint16 offset;
		
		convert_setup (basep, TYPE_SLIDING_POS);
		
		if (IS_BORDER_POS (old_pos))
			edge = BORDER_POS (old_pos)->edge;
		else if (PANEL_WIDGET (cur_panel)->orient == PANEL_HORIZONTAL)
			edge = (y - multiscreen_y (basep->screen) >
				(multiscreen_height (basep->screen) / 2))
				? BORDER_BOTTOM : BORDER_TOP;
		else
			edge = (x - multiscreen_x (basep->screen) >
				(multiscreen_width (basep->screen) / 2))
				? BORDER_RIGHT : BORDER_LEFT;
		
		if (PANEL_WIDGET (cur_panel)->orient == PANEL_HORIZONTAL) {
			val = x - multiscreen_x (basep->screen);
			max = multiscreen_width (basep->screen);
		} else {
			val = y - multiscreen_y (basep->screen);
			max = multiscreen_height (basep->screen);
		}
		
		if (val > 0.9 * max) {
			offset = max - val;
			anchor = SLIDING_ANCHOR_RIGHT;
		} else {
			offset = val;
			anchor = SLIDING_ANCHOR_LEFT;
		}

		sliding_widget_change_anchor_offset_edge (
			SLIDING_WIDGET (basep), anchor, offset, edge);
		
		break;
	}
	case FLOATING_PANEL:
	{
		convert_setup (basep, TYPE_FLOATING_POS);
		floating_widget_change_coords (FLOATING_WIDGET (basep),
					       x, y);
		break;
	}
	default:
		g_assert_not_reached ();
		break;
	}

	gtk_object_unref (GTK_OBJECT (old_pos));
	gtk_widget_queue_resize (GTK_WIDGET (basep));
}

static void
change_hiding_mode (GtkWidget *widget, gpointer data)
{
	BasePWidget *basep;
	PanelWidget *cur_panel = get_panel_from_menu_data(widget, TRUE);

	g_return_if_fail(cur_panel != NULL);
	g_return_if_fail(IS_PANEL_WIDGET(cur_panel));

	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	g_assert (cur_panel->panel_parent);
	g_return_if_fail (IS_BASEP_WIDGET (cur_panel->panel_parent));

	basep = BASEP_WIDGET(cur_panel->panel_parent);
	
	basep_widget_change_params (basep,
				    basep->screen,
				    cur_panel->orient,
				    cur_panel->sz,
				    GPOINTER_TO_INT (data),
				    basep->state,
				    basep->level,
				    basep->avoid_on_maximize,
				    basep->hidebuttons_enabled,
				    basep->hidebutton_pixmaps_enabled,
				    cur_panel->back_type,
				    cur_panel->back_pixmap,
				    cur_panel->fit_pixmap_bg,
				    cur_panel->strech_pixmap_bg,
				    cur_panel->rotate_pixmap_bg,
				    &cur_panel->back_color);

	update_config_mode (basep);
}

static void
change_level (GtkWidget *widget, gpointer data)
{
	BasePWidget *basep;
	PanelWidget *cur_panel = get_panel_from_menu_data(widget, TRUE);

	g_return_if_fail(cur_panel != NULL);
	g_return_if_fail(IS_PANEL_WIDGET(cur_panel));

	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	g_assert (cur_panel->panel_parent);
	g_return_if_fail (IS_BASEP_WIDGET (cur_panel->panel_parent));

	basep = BASEP_WIDGET(cur_panel->panel_parent);
	
	basep_widget_change_params (basep,
				    basep->screen,
				    cur_panel->orient,
				    cur_panel->sz,
				    basep->mode,
				    basep->state,
				    GPOINTER_TO_INT (data),
				    basep->avoid_on_maximize,
				    basep->hidebuttons_enabled,
				    basep->hidebutton_pixmaps_enabled,
				    cur_panel->back_type,
				    cur_panel->back_pixmap,
				    cur_panel->fit_pixmap_bg,
				    cur_panel->strech_pixmap_bg,
				    cur_panel->rotate_pixmap_bg,
				    &cur_panel->back_color);
	
	update_config_level (basep);
}

static void
change_avoid_on_maximize (GtkWidget *widget, gpointer data)
{
	BasePWidget *basep;
	PanelWidget *cur_panel = get_panel_from_menu_data(widget, TRUE);

	g_return_if_fail(cur_panel != NULL);
	g_return_if_fail(IS_PANEL_WIDGET(cur_panel));

	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	g_assert (cur_panel->panel_parent);
	g_return_if_fail (IS_BASEP_WIDGET (cur_panel->panel_parent));

	basep = BASEP_WIDGET(cur_panel->panel_parent);
	
	basep_widget_change_params (basep,
				    basep->screen,
				    cur_panel->orient,
				    cur_panel->sz,
				    basep->mode,
				    basep->state,
				    basep->level,
				    GPOINTER_TO_INT (data),
				    basep->hidebuttons_enabled,
				    basep->hidebutton_pixmaps_enabled,
				    cur_panel->back_type,
				    cur_panel->back_pixmap,
				    cur_panel->fit_pixmap_bg,
				    cur_panel->strech_pixmap_bg,
				    cur_panel->rotate_pixmap_bg,
				    &cur_panel->back_color);

	update_config_avoid_on_maximize (basep);
}

static void
change_size (GtkWidget *widget, gpointer data)
{
	PanelWidget *cur_panel = get_panel_from_menu_data(widget, TRUE);
	g_return_if_fail(cur_panel != NULL);
	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	panel_widget_change_params (cur_panel,
				    cur_panel->orient,
				    GPOINTER_TO_INT (data),
				    cur_panel->back_type,
				    cur_panel->back_pixmap,
				    cur_panel->fit_pixmap_bg,
				    cur_panel->strech_pixmap_bg,
				    cur_panel->rotate_pixmap_bg,
				    cur_panel->no_padding_on_ends,
				    &cur_panel->back_color);
}

static void
change_orient (GtkWidget *widget, gpointer data)
{

	BasePWidget *basep;
	PanelWidget *cur_panel = get_panel_from_menu_data(widget, TRUE);
	
	g_return_if_fail(cur_panel != NULL);
	g_return_if_fail(IS_PANEL_WIDGET(cur_panel));

	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	g_assert (cur_panel->panel_parent);
	g_return_if_fail (IS_BASEP_WIDGET (cur_panel->panel_parent));

	basep = BASEP_WIDGET(cur_panel->panel_parent);
	
	basep_widget_change_params (basep,
				    basep->screen,
				    GPOINTER_TO_INT (data),
				    cur_panel->sz,
				    basep->mode,
				    basep->state,
				    basep->level,
				    basep->avoid_on_maximize,
				    basep->hidebuttons_enabled,
				    basep->hidebutton_pixmaps_enabled,
				    cur_panel->back_type,
				    cur_panel->back_pixmap,
				    cur_panel->fit_pixmap_bg,
				    cur_panel->strech_pixmap_bg,
				    cur_panel->rotate_pixmap_bg,
				    &cur_panel->back_color);
}

static void
change_background (GtkWidget *widget, gpointer data)
{
	PanelWidget *cur_panel = get_panel_from_menu_data(widget, TRUE);
	g_return_if_fail(cur_panel != NULL);

	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	panel_widget_change_params (cur_panel,
				    cur_panel->orient,
				    cur_panel->sz,
				    GPOINTER_TO_INT (data),
				    cur_panel->back_pixmap,
				    cur_panel->fit_pixmap_bg,
				    cur_panel->strech_pixmap_bg,
				    cur_panel->rotate_pixmap_bg,
				    cur_panel->no_padding_on_ends,
				    &cur_panel->back_color);
}

static void
change_hidebuttons (GtkWidget *widget, gpointer data)
{
	BasePWidget *basep;
	gboolean hidebutton_pixmaps_enabled, hidebuttons_enabled;
	PanelWidget *cur_panel = get_panel_from_menu_data(widget, TRUE);

	g_return_if_fail(cur_panel != NULL);
	g_return_if_fail(IS_PANEL_WIDGET(cur_panel));

	if (!GTK_CHECK_MENU_ITEM (widget)->active)
		return;

	g_assert (cur_panel->panel_parent);
	g_return_if_fail (IS_BASEP_WIDGET (cur_panel->panel_parent));

	basep = BASEP_WIDGET(cur_panel->panel_parent);

	hidebuttons_enabled = basep->hidebuttons_enabled;
	hidebutton_pixmaps_enabled = basep->hidebutton_pixmaps_enabled;

	switch (GPOINTER_TO_INT (data)) {
	case HIDEBUTTONS_NONE:
		hidebuttons_enabled = FALSE;
		break;
	case HIDEBUTTONS_PLAIN:
		hidebutton_pixmaps_enabled = FALSE;
		hidebuttons_enabled = TRUE;
		break;
	case HIDEBUTTONS_PIXMAP:
		hidebutton_pixmaps_enabled = TRUE;
		hidebuttons_enabled = TRUE;
		break;
	}

	basep_widget_change_params (basep,
				    basep->screen,
				    cur_panel->orient,
				    cur_panel->sz,
				    basep->mode,
				    basep->state,
				    basep->level,
				    basep->avoid_on_maximize,
				    hidebuttons_enabled,
				    hidebutton_pixmaps_enabled,
				    cur_panel->back_type,
				    cur_panel->back_pixmap,
				    cur_panel->fit_pixmap_bg,
				    cur_panel->strech_pixmap_bg,
				    cur_panel->rotate_pixmap_bg,
				    &cur_panel->back_color);

	update_config_hidebuttons (basep);
}

static void
show_x_on_panels(GtkWidget *menu, gpointer data)
{
	GtkWidget *pw;
	GtkWidget *types = gtk_object_get_data(GTK_OBJECT(menu), MENU_TYPES);
	GtkWidget *modes = gtk_object_get_data(GTK_OBJECT(menu), MENU_MODES);
	GtkWidget *orient = gtk_object_get_data (GTK_OBJECT (menu), MENU_ORIENTS);
	PanelWidget *cur_panel = get_panel_from_menu_data(menu, TRUE);
	g_return_if_fail(cur_panel != NULL);
	g_return_if_fail(IS_PANEL_WIDGET(cur_panel));
	g_return_if_fail(types != NULL);
	g_return_if_fail(modes != NULL);
	
	pw = cur_panel->panel_parent;
	g_return_if_fail(pw != NULL);
	
	if(IS_DRAWER_WIDGET(pw)) {
		gtk_widget_hide(modes);
		gtk_widget_hide(types);
	} else {
		gtk_widget_show(modes);
		gtk_widget_show(types);
	}

	if (IS_FLOATING_WIDGET (pw))
		gtk_widget_show (orient);
	else
		gtk_widget_hide (orient);
}

static void
update_type_menu (GtkWidget *menu, gpointer data)
{
	char *s = NULL;
	GtkWidget *menuitem = NULL;
	PanelWidget *cur_panel = get_panel_from_menu_data(menu, TRUE);
	GtkWidget *basep = cur_panel->panel_parent;
	if (IS_EDGE_WIDGET (basep))
		s = MENU_TYPE_EDGE;
	else if (IS_ALIGNED_WIDGET (basep))
		s = MENU_TYPE_ALIGNED;
	else if (IS_SLIDING_WIDGET (basep))
		s = MENU_TYPE_SLIDING;
	else if (IS_FLOATING_WIDGET (basep))
		s = MENU_TYPE_FLOATING;
	else
		return;
	
	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);				 
	
	if (menuitem)
		gtk_check_menu_item_set_active (
			GTK_CHECK_MENU_ITEM (menuitem), TRUE);
}

static void
update_level_menu (GtkWidget *menu, gpointer data)
{
	char *s = NULL;
	GtkWidget *menuitem = NULL;
	PanelWidget *cur_panel = get_panel_from_menu_data(menu, TRUE);
	GtkWidget *basep = cur_panel->panel_parent;

	/* sanity */
	if ( ! BASEP_WIDGET (basep))
		return;

	switch (BASEP_WIDGET (basep)->level) {
	case BASEP_LEVEL_DEFAULT:
		s = MENU_LEVEL_DEFAULT;
		break;
	case BASEP_LEVEL_ABOVE:
		s = MENU_LEVEL_ABOVE;
		break;
	case BASEP_LEVEL_NORMAL:
		s = MENU_LEVEL_NORMAL;
		break;
	case BASEP_LEVEL_BELOW:
		s = MENU_LEVEL_BELOW;
		break;
	default:
		return;
	}
	
	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);				 
	
	if (menuitem != NULL)
		gtk_check_menu_item_set_active (
			GTK_CHECK_MENU_ITEM (menuitem), TRUE);
}

static void
update_avoid_on_maximize_menu (GtkWidget *menu, gpointer data)
{
	char *s = NULL;
	GtkWidget *menuitem = NULL;
	PanelWidget *cur_panel = get_panel_from_menu_data(menu, TRUE);
	GtkWidget *basep = cur_panel->panel_parent;

	/* sanity */
	if ( ! BASEP_WIDGET (basep))
		return;

	if (BASEP_WIDGET (basep)->avoid_on_maximize) {
		s = MENU_AVOID_ON_MAX;
	} else {
		s = MENU_NO_AVOID_ON_MAX;
	}
	
	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);				 
	
	if (menuitem != NULL)
		gtk_check_menu_item_set_active (
			GTK_CHECK_MENU_ITEM (menuitem), TRUE);
}

static void
update_size_menu (GtkWidget *menu, gpointer data)
{
	GtkWidget *menuitem = NULL;
	char *s = NULL;
	PanelWidget *cur_panel = get_panel_from_menu_data(menu, TRUE);
	switch (cur_panel->sz) {
	case SIZE_ULTRA_TINY:
		s = MENU_SIZE_ULTRA_TINY;
		break;
	case SIZE_TINY:
		s = MENU_SIZE_TINY;
		break;
	case SIZE_SMALL:
		s = MENU_SIZE_SMALL;
		break;
	case SIZE_STANDARD:
		s = MENU_SIZE_STANDARD;
		break;
	case SIZE_LARGE:
		s = MENU_SIZE_LARGE;
		break;
	case SIZE_HUGE:
		s = MENU_SIZE_HUGE;
		break;
	case SIZE_RIDICULOUS:
		s = MENU_SIZE_RIDICULOUS;
		break;
	default:
		return;
	}

	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					TRUE);
}


static void
update_back_menu (GtkWidget *menu, gpointer data)
{
	GtkWidget *menuitem = NULL;
	char *s = NULL;
	PanelWidget *cur_panel = get_panel_from_menu_data(menu, TRUE);
	switch (cur_panel->back_type) {
	case PANEL_BACK_NONE:
		s = MENU_BACK_NONE;
		break;
	case PANEL_BACK_COLOR:
		s = MENU_BACK_COLOR;
		break;
	case PANEL_BACK_PIXMAP:
		s = MENU_BACK_PIXMAP;
		break;
	case PANEL_BACK_TRANSLUCENT:
		s = MENU_BACK_TRANSLUCENT;
		break;
	default:
		return;
	}

	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					TRUE);
	menuitem = gtk_object_get_data (GTK_OBJECT (menu), MENU_BACK_PIXMAP);
	gtk_widget_set_sensitive (menuitem, cur_panel->back_pixmap != NULL);
}

static void
update_hidebutton_menu (GtkWidget *menu, gpointer data)
{
	char *s = NULL;
	GtkWidget *menuitem = NULL;
	PanelWidget *cur_panel = get_panel_from_menu_data(menu, TRUE);
	BasePWidget *basep = BASEP_WIDGET(cur_panel->panel_parent);

	if (!basep->hidebuttons_enabled)
		s = MENU_HIDEBUTTONS_NONE;
	else if (basep->hidebutton_pixmaps_enabled)
		s = MENU_HIDEBUTTONS_PIXMAP;
	else 
		s = MENU_HIDEBUTTONS_PLAIN;
	
	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					TRUE);
}

static void
update_hiding_menu (GtkWidget *menu, gpointer data)
{
	char *s = NULL;
	GtkWidget *menuitem = NULL;
	PanelWidget *cur_panel = get_panel_from_menu_data(menu, TRUE);
	BasePWidget *basep = BASEP_WIDGET(cur_panel->panel_parent);
	s =  (basep->mode == BASEP_EXPLICIT_HIDE)
		? MENU_MODE_EXPLICIT_HIDE
		: MENU_MODE_AUTO_HIDE;

	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					TRUE);
}

static void
update_orient_menu (GtkWidget *menu, gpointer data)
{
	char *s = NULL;
	GtkWidget *menuitem = NULL;
	PanelWidget *cur_panel = get_panel_from_menu_data (menu, TRUE);
	BasePWidget *basep = BASEP_WIDGET (cur_panel->panel_parent);
	s = (PANEL_WIDGET (basep->panel)->orient == PANEL_HORIZONTAL)
		? MENU_ORIENT_HORIZONTAL
		: MENU_ORIENT_VERTICAL;

	menuitem = gtk_object_get_data (GTK_OBJECT (menu), s);
	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menuitem),
					TRUE);
}

typedef struct {
	char *name;
	char *id;
	int i;
} NameIdEnum;

static void
add_radios_to_menu (GtkWidget *menu, NameIdEnum *items,
		    GtkSignalFunc func)
{
	int i;
	GSList *radio_group = NULL;
	GtkWidget *menuitem;

	for (i=0; items[i].name; i++) {
		menuitem = gtk_radio_menu_item_new (radio_group);
		gtk_widget_lock_accelerators (menuitem);
		radio_group = gtk_radio_menu_item_group (
			GTK_RADIO_MENU_ITEM (menuitem));
		setup_menuitem (menuitem, NULL, _(items[i].name));
		gtk_menu_append (GTK_MENU (menu), 
				 menuitem);
		gtk_object_set_data (GTK_OBJECT (menu),
				     items[i].id, menuitem);
		gtk_check_menu_item_set_show_toggle (
			GTK_CHECK_MENU_ITEM (menuitem), TRUE);
		gtk_check_menu_item_set_active (
			GTK_CHECK_MENU_ITEM (menuitem), FALSE);
		gtk_signal_connect (GTK_OBJECT (menuitem), "toggled",
				    GTK_SIGNAL_FUNC (func),
				    GINT_TO_POINTER (items[i].i));
	}
}

static void
add_radio_menu (GtkWidget *menu, const char *menutext, 
		NameIdEnum *items, const char *menu_key,
		GtkSignalFunc change_func,
		GtkSignalFunc update_func)
{
	GtkWidget *menuitem;
	GtkWidget *submenu;

	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem (menuitem, NULL, menutext);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_object_set_data (GTK_OBJECT (menu), menu_key, menuitem);

	submenu = menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);
	add_radios_to_menu (submenu, items, change_func);
	gtk_signal_connect (GTK_OBJECT (submenu), "show",
			    GTK_SIGNAL_FUNC (update_func),
			    NULL);
	
}

static void
make_properties_submenu (GtkWidget *menu)
{
	NameIdEnum types[] = {
		{ N_("Edge panel"), MENU_TYPE_EDGE, EDGE_PANEL },
		{ N_("Aligned panel"), MENU_TYPE_ALIGNED, ALIGNED_PANEL },
		{ N_("Sliding panel"), MENU_TYPE_SLIDING, SLIDING_PANEL },
		{ N_("Floating panel"), MENU_TYPE_FLOATING, FLOATING_PANEL },
		{ NULL, NULL, -1 }
	};
	
	NameIdEnum modes[] = {
		{ N_("Explicit hide"), MENU_MODE_EXPLICIT_HIDE, BASEP_EXPLICIT_HIDE },
		{ N_("Auto hide"), MENU_MODE_AUTO_HIDE, BASEP_AUTO_HIDE },
		{ NULL, NULL, -1 }
	};

	NameIdEnum hidebuttons[] = {
		{ N_("With pixmap arrow"), MENU_HIDEBUTTONS_PIXMAP, HIDEBUTTONS_PIXMAP },
		{ N_("Without pixmap"), MENU_HIDEBUTTONS_PLAIN, HIDEBUTTONS_PLAIN },
		{ N_("None"), MENU_HIDEBUTTONS_NONE, HIDEBUTTONS_NONE },
		{ NULL, NULL, -1 }
	};

	NameIdEnum orients[] = {
		{ N_("Horizontal"), MENU_ORIENT_HORIZONTAL, PANEL_HORIZONTAL },
		{ N_("Vertical"), MENU_ORIENT_VERTICAL, PANEL_VERTICAL },
		{ NULL, NULL, -1 }
	};

	NameIdEnum sizes[] = {
		{ N_("Ultra Tiny (12 pixels)"), MENU_SIZE_ULTRA_TINY, SIZE_ULTRA_TINY },
		{ N_("Tiny (24 pixels)"), MENU_SIZE_TINY, SIZE_TINY },
		{ N_("Small (36 pixels)"), MENU_SIZE_SMALL, SIZE_SMALL },
		{ N_("Standard (48 pixels)"), MENU_SIZE_STANDARD, SIZE_STANDARD },
		{ N_("Large (64 pixels)"), MENU_SIZE_LARGE, SIZE_LARGE },
		{ N_("Huge (80 pixels)"), MENU_SIZE_HUGE, SIZE_HUGE },
		{ N_("Ridiculous (128 pixels)"), MENU_SIZE_RIDICULOUS, SIZE_RIDICULOUS },
		{ NULL, NULL, -1 }
	};

	NameIdEnum backgrounds[] = {
		{ N_("Standard"), MENU_BACK_NONE, PANEL_BACK_NONE },
		{ N_("Color"), MENU_BACK_COLOR, PANEL_BACK_COLOR },
		{ N_("Pixmap"), MENU_BACK_PIXMAP, PANEL_BACK_PIXMAP },
		{ N_("Translucent"), MENU_BACK_TRANSLUCENT, PANEL_BACK_TRANSLUCENT },
		{ NULL, NULL, -1 }
	};

	NameIdEnum levels[] = {
		{ N_("Default"), MENU_LEVEL_DEFAULT, BASEP_LEVEL_DEFAULT },
		{ N_("Below"), MENU_LEVEL_BELOW, BASEP_LEVEL_BELOW },
		{ N_("Normal"), MENU_LEVEL_NORMAL, BASEP_LEVEL_NORMAL },
		{ N_("Above"), MENU_LEVEL_ABOVE, BASEP_LEVEL_ABOVE },
		{ NULL, NULL, -1 }
	};

	NameIdEnum avoid_on_maximize[] = {
		{ N_("Avoid on maximize"), MENU_AVOID_ON_MAX, TRUE },
		{ N_("Don't avoid on maximize"), MENU_NO_AVOID_ON_MAX, FALSE },
		{ NULL, NULL, -1 }
	};

	add_radio_menu (menu, _("Type"), types, MENU_TYPES,
			convert_to_panel, update_type_menu);

	add_radio_menu (menu, _("Hiding policy"), modes, MENU_MODES,
			change_hiding_mode, update_hiding_menu);

	add_radio_menu (menu, _("Hide buttons"), hidebuttons, MENU_HIDEBUTTONS,
			change_hidebuttons, update_hidebutton_menu);

	add_radio_menu (menu, _("Size"), sizes, MENU_SIZES,
			change_size, update_size_menu);

	add_radio_menu (menu, _("Orientation"), orients, MENU_ORIENTS,
			change_orient, update_orient_menu);

	add_radio_menu (menu, _("Background type"), backgrounds, MENU_BACKS,
			change_background, update_back_menu);

	add_radio_menu (menu, _("Level"), levels, MENU_LEVELS,
			change_level, update_level_menu);

	add_radio_menu (menu, _("Maximize mode"), avoid_on_maximize, MENU_MAXIMIZE_MODE,
			change_avoid_on_maximize, update_avoid_on_maximize_menu);
	
	gtk_signal_connect (GTK_OBJECT (menu), "show",
			    GTK_SIGNAL_FUNC (show_x_on_panels),
			    NULL);
}

static void
make_add_submenu (GtkWidget *menu, gboolean fake_submenus)
{
	GtkWidget *menuitem, *submenu, *submenuitem, *m;

	/* Add Menu */

	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem_try_pixmap (menuitem, "gnome-applets.png",
				   _("Applet"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	m = create_applets_menu(NULL, fake_submenus, TRUE);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),m);
	gtk_signal_connect(GTK_OBJECT(m),"show",
			   GTK_SIGNAL_FUNC(submenu_to_display), NULL);

	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem_try_pixmap (menuitem, 
				   "gnome-gmenu.png",
				   _("Menu"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (menu), menuitem);

	submenu = menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);

	submenuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem_try_pixmap (submenuitem,
				   "gnome-logo-icon-transparent.png",
				   _("Main menu"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (submenu), submenuitem);
	gtk_signal_connect(GTK_OBJECT(submenuitem), "activate",
			   GTK_SIGNAL_FUNC(add_menu_to_panel),
			   NULL);
	setup_internal_applet_drag(submenuitem, "MENU:MAIN");

	submenuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem_try_pixmap (submenuitem, 
				   "gnome-logo-icon-transparent.png",
				   _("Programs menu"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (submenu), submenuitem);
	gtk_signal_connect(GTK_OBJECT(submenuitem), "activate",
			   GTK_SIGNAL_FUNC(add_menu_to_panel),
			   "gnome/apps");
	setup_internal_applet_drag(submenuitem, "MENU:gnome/apps");

	submenuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem_try_pixmap (submenuitem, "gnome-favorites.png",
				   _("Favorites menu"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (submenu), submenuitem);
	gtk_signal_connect(GTK_OBJECT(submenuitem), "activate",
			   GTK_SIGNAL_FUNC(add_menu_to_panel),
			   "~/.gnome/apps");
	setup_internal_applet_drag(submenuitem, "MENU:~/.gnome/apps");

	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem_try_pixmap (menuitem, 
				   "launcher-program.png",
				   _("Launcher..."), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(ask_about_launcher_cb),NULL);
	setup_internal_applet_drag(menuitem, "LAUNCHER:ASK");

	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem_try_pixmap (menuitem, "launcher-program.png",
				   _("Launcher from menu"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	m = create_add_launcher_menu (NULL, fake_submenus);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), m);
	gtk_signal_connect (GTK_OBJECT (m),"show",
			   GTK_SIGNAL_FUNC (submenu_to_display), NULL);

	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem_try_pixmap (menuitem, 
				   "panel-drawer.png",
				   _("Drawer"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   (GtkSignalFunc) add_drawer_to_panel,
			   NULL);
	setup_internal_applet_drag(menuitem, "DRAWER:NEW");

	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem_try_pixmap (menuitem,
				   "gnome-term-night.png",
				   _("Log out button"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(add_logout_to_panel),
			   NULL);
	setup_internal_applet_drag(menuitem, "LOGOUT:NEW");
	
	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem_try_pixmap (menuitem, 
				   "gnome-lockscreen.png",
				   _("Lock button"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(add_lock_to_panel),
			   NULL);
	setup_internal_applet_drag(menuitem, "LOCK:NEW");

	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem_try_pixmap (menuitem,
				   "gnome-run.png",
				   _("Run button"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(add_run_to_panel),
			   NULL);
	setup_internal_applet_drag(menuitem, "RUN:NEW");

	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem (menuitem, 
			gnome_stock_pixmap_widget (menu,
						   GNOME_STOCK_PIXMAP_ADD),
			_("Swallowed app..."));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(ask_about_swallowing_cb),NULL);
	setup_internal_applet_drag(menuitem, "SWALLOW:ASK");

	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem(menuitem, 0, _("Status dock"));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(try_add_status_to_panel),NULL);
	setup_internal_applet_drag(menuitem, "STATUS:TRY");
}

static void
add_to_panel_menu_tearoff_new_menu(GtkWidget *w, gpointer data)
{
	GtkWidget *menu;
	TearoffMenu *tm;
	char *wmclass = get_unique_tearoff_wmclass();
	PanelWidget *menu_panel;

	menu = menu_new();
	make_add_submenu(menu, TRUE);
	
	/*set the panel to use as the data*/
	menu_panel = get_panel_from_menu_data(w, TRUE);
	gtk_object_set_data(GTK_OBJECT(menu), "menu_panel", menu_panel);
	gtk_signal_connect_object_while_alive(GTK_OBJECT(menu_panel),
		      "destroy", GTK_SIGNAL_FUNC(gtk_widget_unref),
		      GTK_OBJECT(menu));
	show_tearoff_menu(menu, _("Add to panel"),TRUE,0,0, wmclass);

	tm = g_new0(TearoffMenu,1);
	tm->menu = menu;
	tm->mfl = NULL;
	tm->title = g_strdup(_("Add to panel"));
	tm->special = g_strdup("ADD_TO_PANEL");
	tm->wmclass = g_strdup(wmclass);
	gtk_signal_connect(GTK_OBJECT(menu), "destroy",
			   GTK_SIGNAL_FUNC(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend(tearoffs,tm);
}

/* just run the gnome-panel-properties */
static void
panel_config_global(void)
{
	char *argv[2] = {"gnome-panel-properties-capplet", NULL};
	if (gnome_execute_async (g_get_home_dir (), 1, argv) < 0)
		panel_error_dialog(_("Cannot execute panel global properties"));
}

static void
setup_remove_this_panel(GtkWidget *menu, GtkWidget *menuitem)
{
	PanelWidget *panel = get_panel_from_menu_data(menu, TRUE);
	GtkWidget *label;

	g_assert(panel->panel_parent);

	if(!GTK_MENU(menu)->torn_off &&
	   !IS_DRAWER_WIDGET(panel->panel_parent) &&
	   base_panels == 1)
		gtk_widget_set_sensitive(menuitem, FALSE);
	else
		gtk_widget_set_sensitive(menuitem, TRUE);

	label = GTK_BIN(menuitem)->child;
	if(GTK_IS_BOX(label)) {
		GList *li, *list;
		list = gtk_container_children(GTK_CONTAINER(label));
		for(li = list; li; li = li->next) {
			if(GTK_IS_LABEL(li->data)) {
				label = li->data;
				break;
			}
		}
		g_list_free(list);
	}
	if(!GTK_IS_LABEL(label)) {
		g_warning("We can't find the label of a menu item");
		return;
	}


	/* this will not handle the case of menu being torn off
	 * and then the confirm_panel_remove changed, but oh well */
	if((GTK_MENU(menu)->torn_off || panel->applet_list) &&
	   global_config.confirm_panel_remove)
		gtk_label_set_text(GTK_LABEL(label), _("Remove this panel..."));
	else
		gtk_label_set_text(GTK_LABEL(label), _("Remove this panel"));
}

static void
show_panel_help (GtkWidget *w, gpointer data)
{
	panel_show_help ("index.html");
}

static void
panel_launch_gmenu (GtkWidget *widget, gpointer data)
{
	char *argv[2] = {"gmenu", NULL};
	if (gnome_execute_async (g_get_home_dir (), 2, argv) < 0)
		   panel_error_dialog (_("Cannot launch gmenu!"));
}

void
make_panel_submenu (GtkWidget *menu, gboolean fake_submenus, gboolean is_basep)
{
	GtkWidget *menuitem, *submenu, *submenuitem;

	if ( ! commie_mode) {
		menuitem = gtk_menu_item_new ();
		gtk_widget_lock_accelerators (menuitem);
		setup_menuitem (menuitem,
				gnome_stock_pixmap_widget (menu,
							   GNOME_STOCK_PIXMAP_ADD),
				_("Add to panel"));
		gtk_menu_append (GTK_MENU (menu), menuitem);

		submenu = menu_new ();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
					   submenu);

		if (gnome_preferences_get_menus_have_tearoff ()) {
			menuitem = tearoff_item_new();
			gtk_widget_show(menuitem);
			gtk_menu_prepend(GTK_MENU(submenu),menuitem);

			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(add_to_panel_menu_tearoff_new_menu),
					   NULL);
		}

		make_add_submenu (submenu, fake_submenus);

		menuitem = gtk_menu_item_new ();
		gtk_widget_lock_accelerators (menuitem);
		setup_menuitem (menuitem, 
				gnome_stock_pixmap_widget (menu,
							   GNOME_STOCK_MENU_NEW),
				_("Create panel"));
		gtk_menu_append (GTK_MENU (menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
					   create_add_panel_submenu(TRUE));

		menuitem = gtk_menu_item_new ();
		gtk_widget_lock_accelerators (menuitem);

		setup_menuitem (menuitem, 
				gnome_stock_pixmap_widget (NULL,
							   GNOME_STOCK_PIXMAP_REMOVE),
				_("Remove this panel"));
		gtk_menu_append (GTK_MENU (menu), menuitem);
		gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
				    GTK_SIGNAL_FUNC (remove_panel_query),
				    NULL);
		gtk_signal_connect (GTK_OBJECT (menu), "show",
				    GTK_SIGNAL_FUNC(setup_remove_this_panel),
				    menuitem);

		add_menu_separator(menu);

		if (is_basep) {
			menuitem = gtk_menu_item_new ();
			gtk_widget_lock_accelerators (menuitem);
			setup_menuitem (menuitem, 
					gnome_stock_pixmap_widget (menu,
								   GNOME_STOCK_MENU_PROP),
					_("Properties"));
			gtk_menu_append (GTK_MENU (menu), menuitem);

			submenu = menu_new ();
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), submenu);

			submenuitem = gtk_menu_item_new ();
			gtk_widget_lock_accelerators (menuitem);
			setup_menuitem (submenuitem,
					gnome_stock_pixmap_widget(submenu,
								  GNOME_STOCK_MENU_PROP),
					_("All properties..."));

			gtk_menu_append (GTK_MENU (submenu), submenuitem);
			gtk_signal_connect (GTK_OBJECT (submenuitem), "activate",
					    GTK_SIGNAL_FUNC(current_panel_config), 
					    NULL);

			/*add_menu_separator (submenu);*/
			make_properties_submenu (submenu);
		}

		menuitem = gtk_menu_item_new ();
		gtk_widget_lock_accelerators (menuitem);
		setup_menuitem (menuitem,
				gnome_stock_pixmap_widget(menu,
							  GNOME_STOCK_MENU_PREF),
				_("Global Preferences..."));
		gtk_menu_append (GTK_MENU (menu), menuitem);
		gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
				    GTK_SIGNAL_FUNC(panel_config_global), 
				    NULL);

		menuitem = gtk_menu_item_new ();
		gtk_widget_lock_accelerators (menuitem);
		setup_menuitem_try_pixmap (menuitem,
					   "gnome-gmenu.png",
					   _("Edit menus..."), SMALL_ICON_SIZE);
		gtk_menu_append (GTK_MENU (menu), menuitem);
		gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
				    GTK_SIGNAL_FUNC(panel_launch_gmenu), 
				    NULL);

		if ( ! global_config.menu_check) {
			menuitem = gtk_menu_item_new ();
			gtk_widget_lock_accelerators (menuitem);
			setup_menuitem (menuitem,
					NULL,
					_("Reread all menus"));
			gtk_menu_append (GTK_MENU (menu), menuitem);
			gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
					    GTK_SIGNAL_FUNC (fr_force_reread), 
					    NULL);
		}

		add_menu_separator (menu);
	}

	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget_at_size (
				menu, GNOME_STOCK_PIXMAP_HELP,
				SMALL_ICON_SIZE, SMALL_ICON_SIZE),
			_("Panel Manual..."));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC (show_panel_help), NULL);
}

void
panel_lock (GtkWidget *widget, gpointer data)
{
	char *argv[3] = {"xscreensaver-command", "-lock", NULL};
	if (gnome_execute_async (g_get_home_dir (), 2, argv) < 0)
		panel_error_dialog (_("Cannot execute xscreensaver"));
}

static void
panel_menu_tearoff_new_menu(GtkWidget *w, gpointer data)
{
	TearoffMenu *tm;
	char *wmclass = get_unique_tearoff_wmclass();
	PanelWidget *menu_panel = get_panel_from_menu_data(w, TRUE);
	GtkWidget *menu = create_panel_submenu (
		NULL, TRUE, FALSE, IS_BASEP_WIDGET (menu_panel->panel_parent));
		
	/*set the panel to use as the data*/
	gtk_object_set_data(GTK_OBJECT(menu), "menu_panel", menu_panel);
	gtk_signal_connect_object_while_alive(GTK_OBJECT(menu_panel),
		      "destroy", GTK_SIGNAL_FUNC(gtk_widget_unref),
		      GTK_OBJECT(menu));

	show_tearoff_menu (menu, _("Panel"), TRUE, 0, 0, wmclass);

	tm = g_new0(TearoffMenu,1);
	tm->menu = menu;
	tm->mfl = NULL;
	tm->title = g_strdup(_("Panel"));
	tm->special = g_strdup("PANEL_SUBMENU");
	tm->wmclass = g_strdup(wmclass);
	gtk_signal_connect(GTK_OBJECT(menu), "destroy",
			   GTK_SIGNAL_FUNC(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend(tearoffs,tm);
}

static void
desktop_menu_tearoff_new_menu (GtkWidget *w, gpointer data)
{
	TearoffMenu *tm;
	char *wmclass = get_unique_tearoff_wmclass ();
	PanelWidget *menu_panel;
	GtkWidget *menu = create_desktop_menu (NULL, TRUE, FALSE);

	/*set the panel to use as the data*/
	menu_panel = get_panel_from_menu_data(w, TRUE);
	gtk_object_set_data(GTK_OBJECT(menu), "menu_panel", menu_panel);
	gtk_signal_connect_object_while_alive(GTK_OBJECT(menu_panel),
		      "destroy", GTK_SIGNAL_FUNC(gtk_widget_unref),
		      GTK_OBJECT(menu));

	show_tearoff_menu (menu, _("Desktop"), TRUE, 0, 0, wmclass);

	tm = g_new0(TearoffMenu,1);
	tm->menu = menu;
	tm->mfl = NULL;
	tm->title = g_strdup(_("Desktop"));
	tm->special = g_strdup("DESKTOP");
	tm->wmclass = g_strdup(wmclass);
	gtk_signal_connect(GTK_OBJECT(menu), "destroy",
			   GTK_SIGNAL_FUNC(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend(tearoffs,tm);
}

static GtkWidget *
create_panel_submenu(GtkWidget *menu, gboolean fake_submenus, gboolean tearoff, 
		     gboolean is_basep)
{
	GtkWidget *menuitem;
	char *char_tmp;

	if (!menu) {
		menu = menu_new();
	}

	if(tearoff && gnome_preferences_get_menus_have_tearoff ()) {
		menuitem = tearoff_item_new();
		gtk_widget_show(menuitem);
		gtk_menu_prepend (GTK_MENU (menu), menuitem);

		gtk_signal_connect(GTK_OBJECT(menuitem),"activate",
				   GTK_SIGNAL_FUNC(panel_menu_tearoff_new_menu),
				   NULL);
	}

	make_panel_submenu (menu, fake_submenus, is_basep);

	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem (menuitem,
			gnome_stock_pixmap_widget(menu,
						  GNOME_STOCK_PIXMAP_ABOUT),
			_("About the panel..."));
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(about_cb),
			    NULL);
	
	char_tmp = panel_is_program_in_path("gnome-about");
	if(!char_tmp)
		char_tmp = panel_is_program_in_path ("guname");

	if (char_tmp) {
		menuitem = gtk_menu_item_new ();
		gtk_widget_lock_accelerators (menuitem);
		setup_menuitem (menuitem,
				gnome_stock_pixmap_widget(menu,
							  GNOME_STOCK_PIXMAP_ABOUT),
				_("About GNOME..."));
		gtk_menu_append (GTK_MENU (menu), menuitem);
		gtk_signal_connect_full(GTK_OBJECT (menuitem), "activate",
					GTK_SIGNAL_FUNC(about_gnome_cb),NULL,
					char_tmp, (GtkDestroyNotify)g_free,
					FALSE,TRUE);
	}
	return menu;
}

static void
run_cb (GtkWidget *w, gpointer data)
{
	show_run_dialog ();
}

static GtkWidget *
create_desktop_menu (GtkWidget *menu, gboolean fake_submenus, gboolean tearoff)
{
	GtkWidget *menuitem;
	char *char_tmp;
	/* Panel entry */

	if (menu == NULL) {
		menu = menu_new ();
	}

	if(tearoff && gnome_preferences_get_menus_have_tearoff ()) {
		menuitem = tearoff_item_new();
		gtk_widget_show(menuitem);
		gtk_menu_prepend (GTK_MENU (menu), menuitem);

		gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
				    GTK_SIGNAL_FUNC(desktop_menu_tearoff_new_menu),
				    NULL);
	}

	char_tmp = panel_is_program_in_path ("xscreensaver");
	if (char_tmp) {	
		menuitem = gtk_menu_item_new ();
		gtk_widget_lock_accelerators (menuitem);
		setup_menuitem_try_pixmap (menuitem,
					   "gnome-lockscreen.png",
					   _("Lock screen"), SMALL_ICON_SIZE);
		gtk_menu_append (GTK_MENU (menu), menuitem);
		gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
				    GTK_SIGNAL_FUNC(panel_lock), 0);
		setup_internal_applet_drag(menuitem, "LOCK:NEW");
		gtk_tooltips_set_tip (panel_tooltips, menuitem,
				      _("Lock the screen so that you can "
					"temporairly leave your computer"), NULL);
	}
	g_free (char_tmp);

	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem_try_pixmap (menuitem,
				   "gnome-term-night.png",
				   _("Log out"), SMALL_ICON_SIZE);
	gtk_menu_append (GTK_MENU (menu), menuitem);
	gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
			    GTK_SIGNAL_FUNC(panel_quit), 0);
	setup_internal_applet_drag(menuitem, "LOGOUT:NEW");
	gtk_tooltips_set_tip (panel_tooltips, menuitem,
			      _("Log out of this session to log in as "
				"a different user or to shut down your "
				"computer"),
			      NULL);

	return menu;
}

static void
add_distribution_submenu (GtkWidget *root_menu, gboolean fake_submenus,
			  gboolean launcher_add,
			  gboolean favourites_add)
{
	GtkWidget *menu;
	GtkWidget *menuitem;
	const DistributionInfo *distribution_info = get_distribution_info ();
	IconSize size = global_config.use_large_icons 
		? MEDIUM_ICON_SIZE : SMALL_ICON_SIZE;

	if (distribution_info == NULL)
		return;

	menu = create_distribution_menu(NULL, fake_submenus,
					TRUE, TRUE, launcher_add,
					favourites_add);
	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem_try_pixmap (menuitem,
				   distribution_info->menu_icon,
				   _(distribution_info->menu_name), size);
	gtk_menu_append (GTK_MENU (root_menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
				   menu);
	if (distribution_info->menu_show_func)
		gtk_signal_connect(GTK_OBJECT(menu),"show",
				   GTK_SIGNAL_FUNC(distribution_info->menu_show_func),
				   menuitem);
	gtk_signal_connect(GTK_OBJECT(menu),"show",
			   GTK_SIGNAL_FUNC(submenu_to_display),
			   NULL);
}

static void
add_kde_submenu (GtkWidget *root_menu, gboolean fake_submenus,
		 gboolean launcher_add,
		 gboolean favourites_add)
{
	GtkWidget *pixmap = NULL;
	GtkWidget *menu;
	GtkWidget *menuitem;
	char *pixmap_path;
	IconSize size = global_config.use_large_icons 
		? MEDIUM_ICON_SIZE : SMALL_ICON_SIZE;

	menu = create_kde_menu (NULL, fake_submenus, TRUE, TRUE,
				TRUE, launcher_add, favourites_add);
	pixmap_path = g_concat_dir_and_file (kde_icondir, "exec.xpm");

	if (panel_file_exists(pixmap_path)) {
		pixmap = fake_pixmap_at_size (pixmap_path, size);
		if (pixmap)
			gtk_widget_show (pixmap);
	}
	g_free (pixmap_path);
	menuitem = gtk_menu_item_new ();
	gtk_widget_lock_accelerators (menuitem);
	setup_menuitem_with_size (menuitem, pixmap, _("KDE menus"),
				  size);
	gtk_menu_append (GTK_MENU (root_menu), menuitem);
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
	gtk_signal_connect(GTK_OBJECT(menu),"show",
			   GTK_SIGNAL_FUNC(submenu_to_display), NULL);
}

GtkWidget *
create_root_menu (GtkWidget *root_menu,
		  gboolean fake_submenus,
		  int flags,
		  gboolean tearoff,
		  gboolean is_basep,
		  gboolean title)
{
	GtkWidget *menu;
	GtkWidget *menuitem;

	gboolean has_inline = (flags & (MAIN_MENU_SYSTEM |
					MAIN_MENU_USER |
					MAIN_MENU_APPLETS |
					MAIN_MENU_KDE));

	gboolean has_subs = (flags & (MAIN_MENU_SYSTEM_SUB |
				      MAIN_MENU_USER_SUB |
				      MAIN_MENU_APPLETS_SUB |
				      MAIN_MENU_KDE_SUB));

	gboolean has_inline2 = (flags & (MAIN_MENU_DESKTOP |
					 MAIN_MENU_PANEL));
	gboolean has_subs2 = (flags & (MAIN_MENU_DESKTOP_SUB |
				       MAIN_MENU_PANEL_SUB));

	IconSize size = global_config.use_large_icons 
		? MEDIUM_ICON_SIZE : SMALL_ICON_SIZE;

	const DistributionInfo *distribution_info = get_distribution_info ();

	if (distribution_info != NULL) {
		has_inline |= (flags & (MAIN_MENU_DISTRIBUTION));
		has_subs |= (flags & (MAIN_MENU_DISTRIBUTION_SUB));
	}


	if(!root_menu)
		root_menu = menu_new ();
	if (tearoff && gnome_preferences_get_menus_have_tearoff ()) {
		GtkWidget *menuitem = tearoff_item_new ();
		gtk_widget_show (menuitem);
		gtk_menu_prepend (GTK_MENU (root_menu), menuitem);
		gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
				    GTK_SIGNAL_FUNC (panel_tearoff_new_menu),
				    GINT_TO_POINTER(flags));
	}
	
	if (flags & MAIN_MENU_SYSTEM)
		create_system_menu(root_menu, fake_submenus,
				   FALSE, title,
				   FALSE /* launcher_add */,
				   FALSE /* favourites_add */);
	if (flags & MAIN_MENU_USER)
		/* FIXME: add the add to favourites somehow here */
		create_user_menu(_("Favorites"), "apps",
				 root_menu, "gnome-favorites.png",
				 fake_submenus, FALSE, FALSE, title,
				 FALSE /* launcher_add */,
				 FALSE /* favourites_add */);
	/* in commie mode the applets menu doesn't make sense */
	if ( ! commie_mode &&
	    flags & MAIN_MENU_APPLETS)
		create_applets_menu(root_menu, fake_submenus, title);

	if (flags & MAIN_MENU_DISTRIBUTION &&
	    distribution_info != NULL) {
		if (distribution_info->menu_show_func)
			distribution_info->menu_show_func(NULL,NULL);

		create_distribution_menu(root_menu, fake_submenus, FALSE,
					 title,
					 FALSE /* launcher_add */,
					 FALSE /* favourites_add */);
	}
	if (flags & MAIN_MENU_KDE)
		create_kde_menu(root_menu, fake_submenus, FALSE, FALSE,
				title,
				FALSE /* launcher_add */,
				FALSE /* favourites_add */);

	/*others here*/

	if (has_subs && has_inline)
		add_menu_separator (root_menu);

	
	if (flags & MAIN_MENU_SYSTEM_SUB) {
		menu = create_system_menu(NULL, fake_submenus, TRUE,
					  TRUE,
					  FALSE /* launcher_add */,
					  FALSE /* favourites_add */);
		menuitem = gtk_menu_item_new ();
		gtk_widget_lock_accelerators (menuitem);
		setup_menuitem_try_pixmap (menuitem,
					   "gnome-logo-icon-transparent.png",
					   _("Programs"), size);
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		if(menu) {
			gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem),
						   menu);
			gtk_signal_connect(GTK_OBJECT(menu),"show",
					   GTK_SIGNAL_FUNC(submenu_to_display),
					   NULL);
		}
	}

	if (flags & MAIN_MENU_USER_SUB) {
		menu = start_favourites_menu (NULL, fake_submenus);
		create_user_menu(_("Favorites"), "apps", menu,
				 "gnome-favorites.png",
				 fake_submenus, TRUE, TRUE, TRUE,
				 FALSE /* launcher_add */,
				 FALSE /* favourites_add */);
		menuitem = gtk_menu_item_new ();
		gtk_widget_lock_accelerators (menuitem);
		setup_menuitem_try_pixmap (menuitem, "gnome-favorites.png",
					   _("Favorites"), size);
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
		gtk_signal_connect(GTK_OBJECT(menu),"show",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   NULL);
	}
	/* in commie mode the applets menu doesn't make sense */
	if ( ! commie_mode &&
	    flags & MAIN_MENU_APPLETS_SUB) {
		menu = create_applets_menu(NULL, fake_submenus, FALSE);
		menuitem = gtk_menu_item_new ();
		gtk_widget_lock_accelerators (menuitem);
		setup_menuitem_try_pixmap (menuitem, "gnome-applets.png",
					  _("Applets"), size);
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
		gtk_signal_connect(GTK_OBJECT(menu),"show",
				   GTK_SIGNAL_FUNC(submenu_to_display),
				   NULL);
	}
	if (flags & MAIN_MENU_DISTRIBUTION_SUB) {
		add_distribution_submenu (root_menu, fake_submenus,
					  FALSE /*launcher_add */,
					  FALSE /*favourites_add */);
	}
	if (flags & MAIN_MENU_KDE_SUB) {
		add_kde_submenu (root_menu, fake_submenus,
				 FALSE /*launcher_add */,
				 FALSE /*favourites_add */);
	}

	if ( ! no_run_box) {
		menuitem = gtk_menu_item_new ();
		gtk_widget_lock_accelerators (menuitem);
		setup_menuitem_try_pixmap (menuitem, "gnome-run.png", 
					   _("Run..."), size);
		gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
				    GTK_SIGNAL_FUNC (run_cb), NULL);
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		setup_internal_applet_drag(menuitem, "RUN:NEW");
		gtk_tooltips_set_tip (panel_tooltips, menuitem,
				      _("Execute a command line"), NULL);
	}

	if (((has_inline && !has_subs) || has_subs) && has_subs2)
		add_menu_separator (root_menu);

	if (flags & MAIN_MENU_PANEL_SUB) {
		menu = create_panel_submenu (NULL, fake_submenus, TRUE, is_basep);
		menuitem = gtk_menu_item_new ();
		gtk_widget_lock_accelerators (menuitem);
		setup_menuitem_try_pixmap (menuitem, "gnome-panel.png", 
					   _("Panel"), SMALL_ICON_SIZE);
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
	}
	if (flags & MAIN_MENU_DESKTOP_SUB) {
		menu = create_desktop_menu (NULL, fake_submenus, TRUE);
		menuitem = gtk_menu_item_new ();
		gtk_widget_lock_accelerators (menuitem);
		setup_menuitem_try_pixmap (menuitem, "gnome-ccdesktop.png",
					   _("Desktop"), SMALL_ICON_SIZE);
		gtk_menu_append (GTK_MENU (root_menu), menuitem);
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (menuitem), menu);
	}

	if (!has_inline2)
		return root_menu;

	if (has_subs2 || has_subs || has_inline)
		add_menu_separator (root_menu);

	if (flags & MAIN_MENU_PANEL) {
		make_panel_submenu (root_menu, fake_submenus, is_basep);
	}
	
	if (flags & MAIN_MENU_DESKTOP) {
		if (flags & MAIN_MENU_PANEL)
			add_menu_separator (root_menu);
		create_desktop_menu (root_menu, fake_submenus, FALSE);
	}
	
	return root_menu;
}
	
void
add_menu_widget (Menu *menu, PanelWidget *panel, GSList *menudirl,
		 gboolean main_menu, gboolean fake_subs)
{
	GSList *li;

	/* one of these has to be there in order to get the panel of the
	   applet */
	g_return_if_fail (menu->menu || panel);

	if(menu->menu) {
		panel = get_panel_from_menu_data(menu->menu, TRUE);
		gtk_widget_unref(menu->menu);
		menu->menu = NULL;
	}

	if(panel == NULL) {
		g_warning ("Menu is seriously weird");
		return;
	}

	if (main_menu) {
		int flags;
		if (menu->global_main)
			flags = global_config.menu_flags;
		else
			flags = menu->main_menu_flags;
		menu->menu = create_root_menu(NULL,
			fake_subs, flags, TRUE,
			IS_BASEP_WIDGET (panel->panel_parent), TRUE);

		gtk_tooltips_set_tip (panel_tooltips, menu->button,
				      _("Main Menu"), NULL);
	} else {
		menu->menu = NULL;
		for(li = menudirl; li != NULL; li = li->next)
			menu->menu = create_menu_at (menu->menu, li->data,
						     FALSE /* applets */,
						     FALSE /* launcher_add */,
						     FALSE /* favourites_add */,
						     NULL, NULL,
						     fake_subs, FALSE, TRUE);

		/* FIXME: A more descriptive name would be better */
		gtk_tooltips_set_tip (panel_tooltips, menu->button,
				      _("Menu"), NULL);

		if(menu->menu == NULL) {
			int flags;
			if (menu->global_main)
				flags = global_config.menu_flags;
			else
				flags = menu->main_menu_flags;
			g_warning(_("Can't create menu, using main menu!"));
			menu->menu = create_root_menu(NULL,
				fake_subs, flags, TRUE,
				IS_BASEP_WIDGET (panel->panel_parent),
				TRUE);
			gtk_tooltips_set_tip (panel_tooltips, menu->button,
					      _("Main Menu"), NULL);
		}

	}
	gtk_signal_connect (GTK_OBJECT (menu->menu), "deactivate",
			    GTK_SIGNAL_FUNC (menu_deactivate), menu);

	gtk_object_set_data(GTK_OBJECT(menu->menu), "menu_panel", panel);
	gtk_signal_connect_object_while_alive(
	      GTK_OBJECT(panel),
	      "destroy", GTK_SIGNAL_FUNC(gtk_widget_unref),
	      GTK_OBJECT(menu->menu));
}

static void
menu_button_pressed (GtkWidget *widget, gpointer data)
{
	Menu *menu = data;
	GdkEventButton *bevent = (GdkEventButton*)gtk_get_current_event();
	GtkWidget *wpanel = get_panel_parent(menu->button);
	gboolean main_menu = (strcmp (menu->path, ".") == 0);
	int flags;
	const DistributionInfo *distribution_info = get_distribution_info ();

	if (menu->global_main)
		flags = global_config.menu_flags;
	else
		flags = menu->main_menu_flags;

	if (menu->menu == NULL) {
		char *this_menu = get_real_menu_path(menu->path);
		GSList *list = g_slist_append(NULL,this_menu);
		
		add_menu_widget(menu, PANEL_WIDGET(menu->button->parent),
				list, strcmp(menu->path, ".")==0, TRUE);
		
		g_free(this_menu);

		g_slist_free(list);
	} else {
		if(flags & MAIN_MENU_DISTRIBUTION &&
		   ! (flags & MAIN_MENU_DISTRIBUTION_SUB) &&
		   distribution_info != NULL &&
		   distribution_info->menu_show_func)
			distribution_info->menu_show_func(NULL, NULL);

		check_and_reread_applet(menu, main_menu);
	}

	/*so that the panel doesn't pop down until we're
	  done with the menu */
	if(IS_BASEP_WIDGET(wpanel)) {
		BASEP_WIDGET(wpanel)->autohide_inhibit = TRUE;
		basep_widget_autohide(BASEP_WIDGET(wpanel));
	}

	BUTTON_WIDGET(menu->button)->ignore_leave = TRUE;
	gtk_grab_remove(menu->button);

	menu->age = 0;
	gtk_menu_popup(GTK_MENU(menu->menu), 0, 0, 
		       applet_menu_position,
		       menu->info, bevent->button, bevent->time);
	gdk_event_free((GdkEvent *)bevent);
}

static void  
drag_data_get_cb (GtkWidget          *widget,
		  GdkDragContext     *context,
		  GtkSelectionData   *selection_data,
		  guint               info,
		  guint               time,
		  gpointer            data)
{
	char *foo;

	foo = g_strdup_printf ("MENU:%d", find_applet (widget));

	gtk_selection_data_set (selection_data,
				selection_data->target, 8, (guchar *)foo,
				strlen (foo));

	g_free (foo);
}

static Menu *
create_panel_menu (PanelWidget *panel, const char *menudir, gboolean main_menu,
		   PanelOrientType orient, int main_menu_flags,
		   gboolean global_main,
		   gboolean custom_icon, const char *custom_icon_file)
{
        static GtkTargetEntry dnd_targets[] = {
		{ "application/x-panel-applet-internal", 0, 0 }
	};
	Menu *menu;
	
	char *pixmap_name;

	menu = g_new0(Menu, 1);

	menu->custom_icon = custom_icon;
	if ( ! string_empty (custom_icon_file))
		menu->custom_icon_file = g_strdup (custom_icon_file);
	else
		menu->custom_icon_file = NULL;

	if (menu->custom_icon &&
	    menu->custom_icon_file != NULL &&
	    panel_file_exists (menu->custom_icon_file))
		pixmap_name = g_strdup (menu->custom_icon_file);
	else
		pixmap_name = get_pixmap (menudir, main_menu);

	menu->main_menu_flags = main_menu_flags;
	menu->global_main = global_main;

	/*make the pixmap*/
	menu->button = button_widget_new (pixmap_name, -1, MENU_TILE,
					  TRUE, orient, _("Menu"));

	/*A hack since this function only pretends to work on window
	  widgets (which we actually kind of are) this will select
	  some (already selected) events on the panel instead of
	  the button window (where they are also selected) but
	  we don't mind*/
	GTK_WIDGET_UNSET_FLAGS (menu->button, GTK_NO_WINDOW);
	gtk_drag_source_set (menu->button,
			     GDK_BUTTON1_MASK,
			     dnd_targets, 1,
			     GDK_ACTION_MOVE);
	GTK_WIDGET_SET_FLAGS (menu->button, GTK_NO_WINDOW);

	if (main_menu)
		gtk_tooltips_set_tip (panel_tooltips, menu->button,
				      _("Main Menu"), NULL);
	else
		/* FIXME: A more descriptive name would be better */
		gtk_tooltips_set_tip (panel_tooltips, menu->button,
				      _("Menu"), NULL);

	gtk_signal_connect (GTK_OBJECT (menu->button), "drag_data_get",
			    GTK_SIGNAL_FUNC (drag_data_get_cb),
			    NULL);

	gtk_signal_connect_after (GTK_OBJECT (menu->button), "pressed",
				  GTK_SIGNAL_FUNC (menu_button_pressed), menu);
	gtk_signal_connect (GTK_OBJECT (menu->button), "destroy",
			    GTK_SIGNAL_FUNC (destroy_menu), menu);
	gtk_widget_show(menu->button);

	/*if we are allowed to be pigs and load all the menus to increase
	  speed, load them*/
	if(global_config.hungry_menus) {
		GSList *list = g_slist_append(NULL, (gpointer)menudir);
		add_menu_widget(menu, panel, list, main_menu, TRUE);
		g_slist_free(list);
	}

	g_free (pixmap_name);

	return menu;
}

static Menu *
create_menu_applet(PanelWidget *panel, const char *arguments,
		   PanelOrientType orient, int main_menu_flags,
		   gboolean global_main,
		   gboolean custom_icon, const char *custom_icon_file)
{
	Menu *menu;
	gboolean main_menu;

	char *this_menu = get_real_menu_path(arguments);

	if (this_menu == NULL)
		return NULL;

	if(gnome_folder == NULL)
		gnome_folder = gnome_pixmap_file("gnome-folder.png");

	main_menu = (string_empty (arguments) ||
		     (strcmp (arguments, ".") == 0));

	menu = create_panel_menu (panel, this_menu, main_menu,
				  orient, main_menu_flags, global_main,
				  custom_icon, custom_icon_file);
	if (arguments && *arguments)
		menu->path = g_strdup(arguments);
	else
		menu->path = g_strdup(".");

	gtk_object_set_user_data(GTK_OBJECT(menu->button), menu);

	g_free (this_menu);
	return menu;
}

void
set_menu_applet_orient(Menu *menu, PanelOrientType orient)
{
	g_return_if_fail(menu!=NULL);

	button_widget_set_params(BUTTON_WIDGET(menu->button),
				 MENU_TILE, TRUE, orient);
}

void
load_menu_applet(const char *params, int main_menu_flags, gboolean global_main,
		 gboolean custom_icon, const char *custom_icon_file,
		 PanelWidget *panel, int pos, gboolean exactpos)
{
	Menu *menu;

	menu = create_menu_applet(panel, params, ORIENT_UP,
				  main_menu_flags, global_main,
				  custom_icon, custom_icon_file);

	if(menu != NULL) {
		char *tmp;

		if(!register_toy(menu->button,
				 menu, free_menu,
				 panel, pos, exactpos, APPLET_MENU))
			return;

		menu->info = applets_last->data;

		if ( ! commie_mode) {
			applet_add_callback(menu->info, "properties",
					    GNOME_STOCK_MENU_PROP,
					    _("Properties..."));
			if(params && strcmp(params, ".")==0 &&
			   (tmp = panel_is_program_in_path("gmenu")))  {
				g_free(tmp);
				applet_add_callback(menu->info, "edit_menus",
						    NULL, _("Edit menus..."));
			}
		}
		applet_add_callback(applets_last->data, "help",
				    GNOME_STOCK_PIXMAP_HELP,
				    _("Help"));
	}
}

void
save_tornoff (void)
{
	GSList *li;
	int i;

	gnome_config_push_prefix (PANEL_CONFIG_PATH "panel/Config/");

	gnome_config_set_int("tearoffs_count",g_slist_length(tearoffs));

	gnome_config_pop_prefix ();

	for(i = 0, li = tearoffs;
	    li != NULL;
	    i++, li = li->next) {
		TearoffMenu *tm = li->data;
		int x = 0, y = 0;
		GtkWidget *tw;
		int menu_panel = 0;
		int menu_panel_id = 0;
		PanelWidget *menu_panel_widget = NULL;
		GSList *l;
		char *s;
		int j;

		s = g_strdup_printf ("%spanel/TornoffMenu_%d/",
				     PANEL_CONFIG_PATH, i);
		gnome_config_push_prefix (s);
		g_free (s);

		tw = GTK_MENU(tm->menu)->tearoff_window;

		if (tw != NULL &&
		    tw->window != NULL) {
			gdk_window_get_root_origin (tw->window, &x, &y);
			/* unfortunately we must do this or set_uposition
			   will crap out */
			if (x < 0)
				x = 0;
			if (y < 0)
				y = 0;
		}

		gnome_config_set_string ("title", tm->title);
		gnome_config_set_string ("wmclass", tm->wmclass);
		gnome_config_set_int ("x", x);
		gnome_config_set_int ("y", y);

		menu_panel_widget = gtk_object_get_data(GTK_OBJECT(tm->menu),
							"menu_panel");
		menu_panel = g_slist_index(panels, menu_panel_widget);
		if(menu_panel < 0)
			menu_panel = 0;
		if (menu_panel_widget != NULL)
			menu_panel_id = menu_panel_widget->unique_id;

		gnome_config_set_int("menu_panel", menu_panel);
		gnome_config_set_int("menu_unique_panel_id", menu_panel_id);

		gnome_config_set_int("workspace",
				     gnome_win_hints_get_workspace(tw));
		gnome_config_set_int("hints",
				     gnome_win_hints_get_hints(tw));
		gnome_config_set_int("state",
				     gnome_win_hints_get_state(tw));

		gnome_config_set_string("special",
					sure_string (tm->special));

		gnome_config_set_int("mfl_count", g_slist_length(tm->mfl));

		for(j=0,l=tm->mfl;l;j++,l=l->next) {
			MenuFinfo *mf = l->data;
			char name[256];
			g_snprintf(name, sizeof (name), "name_%d", j);
			gnome_config_set_string(name, mf->menudir);
			g_snprintf(name, sizeof (name), "dir_name_%d", j);
			gnome_config_set_string(name, mf->dir_name);
			g_snprintf(name, sizeof (name), "pixmap_name_%d", j);
			gnome_config_set_string(name, mf->pixmap_name);
			g_snprintf(name, sizeof (name), "applets_%d", j);
			gnome_config_set_bool(name, mf->applets);
			g_snprintf(name, sizeof (name), "launcher_add_%d", j);
			gnome_config_set_bool(name, mf->launcher_add);
			g_snprintf(name, sizeof (name), "favourites_add_%d", j);
			gnome_config_set_bool(name, mf->favourites_add);
		}

		gnome_config_pop_prefix();
	}
}

static GtkWidget *
create_special_menu(char *special, PanelWidget *menu_panel_widget)
{
	GtkWidget *menu = NULL;

	if(strcmp(special, "ADD_PANEL")==0) {
		menu = create_add_panel_submenu(FALSE);
	} else if(strncmp(special, "PANEL", strlen("PANEL"))==0) {
		int flags;
		if(sscanf(special, "PANEL:%d", &flags) != 1)
			flags = global_config.menu_flags;
		menu = create_root_menu (NULL, TRUE, flags, FALSE,
					 IS_BASEP_WIDGET (menu_panel_widget->panel_parent),
					 TRUE);
	} else if(strcmp(special, "DESKTOP")==0) {
		menu = create_desktop_menu (NULL, TRUE, FALSE);
	} else if(strcmp(special, "ADD_TO_PANEL")==0) {
		menu = menu_new();
		make_add_submenu(menu, TRUE);
	} else if(strcmp(special, "PANEL_SUBMENU")==0) {
		menu = create_panel_submenu (
			NULL, TRUE, FALSE,
			IS_BASEP_WIDGET (menu_panel_widget->panel_parent));

	}

	return menu;
}

static void
load_tearoff_menu(void)
{
	GtkWidget *menu;
	char *title, *wmclass, *special;
	int x, y, i;
	int workspace, hints, state;
	int mfl_count;
	TearoffMenu *tm;
	gulong wmclass_num;
	PanelWidget *menu_panel_widget = NULL;

	if ( ! conditional_true ("Conditional"))
		return;

	title = conditional_get_string ("title", NULL, NULL);
	wmclass = conditional_get_string ("wmclass", NULL, NULL);

	if(string_empty (title) ||
	   string_empty (wmclass)) {
		g_free(title);
		g_free(wmclass);
		return;
	}

	x = conditional_get_int ("x", 0, NULL);
	y = conditional_get_int ("y", 0, NULL);
	workspace = conditional_get_int ("workspace", 0, NULL);
	hints = conditional_get_int ("hints", 0, NULL);
	state = conditional_get_int ("state", 0, NULL);

	i = conditional_get_int("menu_panel_id", -1, NULL);
	if (i < 0) {
		i = conditional_get_int("menu_panel", 0, NULL);
		if (i < 0)
			i = 0;
		menu_panel_widget = g_slist_nth_data(panels, i);
	} else {
		menu_panel_widget = panel_widget_get_by_id (i);
	}
	if (menu_panel_widget == NULL)
		menu_panel_widget = panels->data;
	if ( ! IS_PANEL_WIDGET(menu_panel_widget))
		g_warning("panels list is on crack");

	mfl_count = conditional_get_int("mfl_count", 0, NULL);

	special = conditional_get_string("special", NULL, NULL);
	if (string_empty (special)) {
		g_free (special);
		special = NULL;
	}

	/* find out the wmclass_number that was used
	   for this wmclass and make our default one 1 higher
	   so that we will always get unique wmclasses */
	wmclass_num = 0;
	sscanf (wmclass, "panel_tearoff_%lu", &wmclass_num);
	if (wmclass_num >= wmclass_number)
		wmclass_number = wmclass_num+1;

	menu = NULL;

	if (special != NULL) {
		menu = create_special_menu (special, menu_panel_widget);
	} else {
		for(i = 0; i < mfl_count; i++) {
			char propname[256];
			char *name;
			gboolean applets;
			gboolean launcher_add;
			gboolean favourites_add;
			char *dir_name;
			char *pixmap_name;

			g_snprintf (propname, sizeof (propname), "name_%d", i);
			name = conditional_get_string(propname, NULL, NULL);
			g_snprintf (propname, sizeof (propname),
				    "applets_%d", i);
			applets = conditional_get_bool(propname, FALSE, NULL);
			g_snprintf (propname, sizeof (propname),
				    "launcher_add_%d", i);
			launcher_add = conditional_get_bool(propname, FALSE, NULL);
			g_snprintf (propname, sizeof (propname),
				    "favourites_add_%d", i);
			favourites_add = conditional_get_bool(propname, FALSE, NULL);
			g_snprintf (propname, sizeof (propname),
				    "dir_name_%d", i);
			dir_name = conditional_get_string(propname, NULL, NULL);
			g_snprintf (propname, sizeof (propname),
				    "pixmap_name_%d", i);
			pixmap_name = conditional_get_string(propname, NULL, NULL);

			if(!menu) {
				menu = menu_new ();
			}

			menu = create_menu_at (menu, name,
					       applets,
					       launcher_add,
					       favourites_add,
					       dir_name,
					       pixmap_name, TRUE, FALSE, TRUE);

			g_free(name);
			g_free(dir_name);
			g_free(pixmap_name);
		}

		if(menu != NULL&&
		   gtk_object_get_data (GTK_OBJECT (menu), "mf") == NULL) {
			gtk_widget_unref (menu);
			menu = NULL;
		}
	}

	if (menu == NULL) {
		g_free (special);
		g_free (title);
		g_free (wmclass);
		return;
	}

	/*set the panel to use as the data, or we will use current_panel*/
	gtk_object_set_data(GTK_OBJECT(menu), "menu_panel",
			    menu_panel_widget);
	gtk_signal_connect_object_while_alive(
	      GTK_OBJECT(menu_panel_widget),
	      "destroy", GTK_SIGNAL_FUNC(gtk_widget_unref),
	      GTK_OBJECT(menu));
	
	/* This is so that we get size of the menu right */
	show_tearoff_menu (menu, title, FALSE, x, y, wmclass);

	{
		GtkWidget *window = GTK_MENU(menu)->tearoff_window;
		gnome_win_hints_set_workspace(window,workspace);
		gnome_win_hints_set_hints(window,hints);
		gnome_win_hints_set_state(window,state);
	}

	tm = g_new0(TearoffMenu,1);
	tm->menu = menu;
	tm->mfl = gtk_object_get_data(GTK_OBJECT(menu), "mf");
	tm->title = title;
	tm->special = special;
	tm->wmclass = wmclass;
	gtk_signal_connect(GTK_OBJECT(menu), "destroy",
			   GTK_SIGNAL_FUNC(tearoff_destroyed), tm);

	tearoffs = g_slist_prepend(tearoffs,tm);
}

void
load_tornoff(void)
{
	char *s;
	int i, length;

	push_correct_global_prefix ();
	length = conditional_get_int("tearoffs_count", 0, NULL);
	gnome_config_pop_prefix();

	for (i = 0; i < length; i++) {
		char *prefix;
		const char *sep;

		prefix = get_correct_prefix (&sep);
		s = g_strdup_printf ("%spanel%s/TornoffMenu_%d/",
				     prefix, sep, i);
		g_free (prefix);
		gnome_config_push_prefix (s);
		g_free (s);

		load_tearoff_menu ();

		gnome_config_pop_prefix ();
	}
}

static void
menu_allocated (GtkWidget *menu, GtkAllocation *alloc)
{
	int screen = 0;
	PanelWidget *cur_panel = get_panel_from_menu_data (menu, FALSE);
	int x, y;
	GtkWidget *menutop;

	menutop = GTK_MENU_SHELL (menu)->active ?
		GTK_MENU (menu)->toplevel : GTK_MENU (menu)->tearoff_window;

	if (cur_panel == NULL) {
		screen = multiscreen_screen_from_pos (menutop->allocation.x,
						      menutop->allocation.y);
		if (screen < 0)
			screen = multiscreen_screen_from_pos
				(menutop->allocation.x +
				   menutop->allocation.width,
				 menutop->allocation.y);
		if (screen < 0)
			screen = multiscreen_screen_from_pos
				(menutop->allocation.x +
				   menutop->allocation.width,
				 menutop->allocation.y +
				   menutop->allocation.height);
		if (screen < 0)
			screen = multiscreen_screen_from_pos
				(menutop->allocation.x,
				 menutop->allocation.y +
				   menutop->allocation.height);
		if (screen < 0)
			screen = 0;
	} else if (IS_BASEP_WIDGET (cur_panel->panel_parent)) {
		screen = BASEP_WIDGET (cur_panel->panel_parent)->screen;
	} else if (IS_FOOBAR_WIDGET (cur_panel->panel_parent)) {
		screen = FOOBAR_WIDGET (cur_panel->panel_parent)->screen;
	}

	scroll_menu_set_screen_size (SCROLL_MENU (menu),
				     multiscreen_y (screen),
				     multiscreen_y (screen) +
				     	multiscreen_height (screen));

	x = menutop->allocation.x;
	y = menutop->allocation.y;

	if (x + menutop->allocation.width >
	    multiscreen_x (screen) + multiscreen_width (screen))
		x = multiscreen_x (screen) + multiscreen_width (screen) -
			menutop->allocation.width;
	if (y + menutop->allocation.height >
	    multiscreen_y (screen) + multiscreen_height (screen))
		y = multiscreen_y (screen) + multiscreen_height (screen) -
			menutop->allocation.height;

	if (x < multiscreen_x (screen))
		x = multiscreen_x (screen);
	if (y < multiscreen_y (screen))
		y = multiscreen_y (screen);

	if (x != menutop->allocation.x ||
	    y != menutop->allocation.y) {
		gtk_widget_set_uposition (menutop, x, y);
	}
}


/* Why the hell do we have a "hack", when we have scroll-menu in
 * our own codebase?  Well cuz I don't want to require panel code in
 * scroll-menu, since people copy it around */
GtkWidget *
hack_scroll_menu_new (void)
{
	GtkWidget *menu = scroll_menu_new ();

	gtk_signal_connect_after (GTK_OBJECT (menu), "size_allocate",
				  GTK_SIGNAL_FUNC (menu_allocated),
				  NULL);

	return menu;
}
