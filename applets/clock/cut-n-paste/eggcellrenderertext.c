/* eggcellrenderertext.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include "eggcellrenderertext.h"
#include <gtk/gtkeditable.h>
#include <gtk/gtkentry.h>
#include "eggtreeprivate.h"
#include "eggmarshalers.h"
#include "eggintl.h"
#include "eel-pango-extensions.h"

/* Disable editing because it the code uses
 * treeview internals
 */
#define DISABLE_EDITING

static void egg_cell_renderer_text_init       (EggCellRendererText      *celltext);
static void egg_cell_renderer_text_class_init (EggCellRendererTextClass *class);
static void egg_cell_renderer_text_finalize   (GObject                  *object);

static void egg_cell_renderer_text_get_property  (GObject                  *object,
						  guint                     param_id,
						  GValue                   *value,
						  GParamSpec               *pspec);
static void egg_cell_renderer_text_set_property  (GObject                  *object,
						  guint                     param_id,
						  const GValue             *value,
						  GParamSpec               *pspec);
static void egg_cell_renderer_text_get_size   (GtkCellRenderer          *cell,
					       GtkWidget                *widget,
					       GdkRectangle             *cell_area,
					       gint                     *x_offset,
					       gint                     *y_offset,
					       gint                     *width,
					       gint                     *height);
static void egg_cell_renderer_text_render     (GtkCellRenderer          *cell,
					       GdkWindow                *window,
					       GtkWidget                *widget,
					       GdkRectangle             *background_area,
					       GdkRectangle             *cell_area,
					       GdkRectangle             *expose_area,
					       GtkCellRendererState      flags);

#ifndef DISABLE_EDITING
static GtkCellEditable *egg_cell_renderer_text_start_editing (GtkCellRenderer      *cell,
							      GdkEvent             *event,
							      GtkWidget            *widget,
							      const gchar          *path,
							      GdkRectangle         *background_area,
							      GdkRectangle         *cell_area,
							      GtkCellRendererState  flags);
#endif /* DISABLE_EDITING */

enum {
  EDITED,
  LAST_SIGNAL
};

enum {
  PROP_0,

  PROP_TEXT,
  PROP_MARKUP,
  PROP_ATTRIBUTES,
  PROP_ELLIPSIZE,
  
  /* Style args */
  PROP_BACKGROUND,
  PROP_FOREGROUND,
  PROP_BACKGROUND_GDK,
  PROP_FOREGROUND_GDK,
  PROP_FONT,
  PROP_FONT_DESC,
  PROP_FAMILY,
  PROP_STYLE,
  PROP_VARIANT,
  PROP_WEIGHT,
  PROP_STRETCH,
  PROP_SIZE,
  PROP_SIZE_POINTS,
  PROP_SCALE,
  PROP_EDITABLE,
  PROP_STRIKETHROUGH,
  PROP_UNDERLINE,
  PROP_RISE,
  
  /* Whether-a-style-arg-is-set args */
  PROP_BACKGROUND_SET,
  PROP_FOREGROUND_SET,
  PROP_FAMILY_SET,
  PROP_STYLE_SET,
  PROP_VARIANT_SET,
  PROP_WEIGHT_SET,
  PROP_STRETCH_SET,
  PROP_SIZE_SET,
  PROP_SCALE_SET,
  PROP_EDITABLE_SET,
  PROP_STRIKETHROUGH_SET,
  PROP_UNDERLINE_SET,
  PROP_RISE_SET
};

static gpointer parent_class;
static guint text_cell_renderer_signals [LAST_SIGNAL];

#define EGG_CELL_RENDERER_TEXT_PATH "egg-cell-renderer-text-path"

GType
egg_cell_renderer_text_get_type (void)
{
  static GType cell_text_type = 0;

  if (!cell_text_type)
    {
      static const GTypeInfo cell_text_info =
      {
        sizeof (EggCellRendererTextClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) egg_cell_renderer_text_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (EggCellRendererText),
	0,              /* n_preallocs */
        (GInstanceInitFunc) egg_cell_renderer_text_init,
      };

      cell_text_type =
	g_type_register_static (GTK_TYPE_CELL_RENDERER, "EggCellRendererText",
				&cell_text_info, 0);
    }

  return cell_text_type;
}

static void
egg_cell_renderer_text_init (EggCellRendererText *celltext)
{
  GTK_CELL_RENDERER (celltext)->xalign = 0.0;
  GTK_CELL_RENDERER (celltext)->yalign = 0.5;
  GTK_CELL_RENDERER (celltext)->xpad = 2;
  GTK_CELL_RENDERER (celltext)->ypad = 2;
  celltext->fixed_height_rows = -1;
  celltext->font = pango_font_description_new ();
}

static void
egg_cell_renderer_text_class_init (EggCellRendererTextClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (class);

  parent_class = g_type_class_peek_parent (class);
  
  object_class->finalize = egg_cell_renderer_text_finalize;
  
  object_class->get_property = egg_cell_renderer_text_get_property;
  object_class->set_property = egg_cell_renderer_text_set_property;

  cell_class->get_size = egg_cell_renderer_text_get_size;
  cell_class->render = egg_cell_renderer_text_render;
#ifndef DISABLE_EDITING
  cell_class->start_editing = egg_cell_renderer_text_start_editing;
#endif /* DISABLE_EDITING */
  
  g_object_class_install_property (object_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text",
                                                        "Text",
                                                        "Text to render",
                                                        NULL,
                                                        G_PARAM_READWRITE));
  
  g_object_class_install_property (object_class,
                                   PROP_MARKUP,
                                   g_param_spec_string ("markup",
                                                        "Markup",
                                                        "Marked up text to render",
                                                        NULL,
                                                        G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_ATTRIBUTES,
				   g_param_spec_boxed ("attributes",
						       "Attributes",
						       "A list of style attributes to apply to the text of the renderer",
						       PANGO_TYPE_ATTR_LIST,
						       G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
				   PROP_ELLIPSIZE,
				   g_param_spec_boolean ("ellipsize",
							 "Ellipsize",
							 "Whether to crop the text and display \"...\" when space is limited",
							 FALSE,
							 G_PARAM_READWRITE));
  
  g_object_class_install_property (object_class,
                                   PROP_BACKGROUND,
                                   g_param_spec_string ("background",
                                                        "Background color name",
                                                        "Background color as a string",
                                                        NULL,
                                                        G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_BACKGROUND_GDK,
                                   g_param_spec_boxed ("background_gdk",
                                                       "Background color",
                                                       "Background color as a GdkColor",
                                                       GDK_TYPE_COLOR,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));  

  g_object_class_install_property (object_class,
                                   PROP_FOREGROUND,
                                   g_param_spec_string ("foreground",
                                                        "Foreground color name",
                                                        "Foreground color as a string",
                                                        NULL,
                                                        G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_FOREGROUND_GDK,
                                   g_param_spec_boxed ("foreground_gdk",
                                                       "Foreground color",
                                                       "Foreground color as a GdkColor",
                                                       GDK_TYPE_COLOR,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));


  g_object_class_install_property (object_class,
                                   PROP_EDITABLE,
                                   g_param_spec_boolean ("editable",
                                                         "Editable",
                                                         "Whether the text can be modified by the user",
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_FONT,
                                   g_param_spec_string ("font",
                                                        "Font",
                                                        "Font description as a string",
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_FONT_DESC,
                                   g_param_spec_boxed ("font_desc",
                                                       "Font",
                                                       "Font description as a PangoFontDescription struct",
                                                       PANGO_TYPE_FONT_DESCRIPTION,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));

  
  g_object_class_install_property (object_class,
                                   PROP_FAMILY,
                                   g_param_spec_string ("family",
                                                        "Font family",
                                                        "Name of the font family, e.g. Sans, Helvetica, Times, Monospace",
                                                        NULL,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_STYLE,
                                   g_param_spec_enum ("style",
                                                      "Font style",
                                                      "Font style",
                                                      PANGO_TYPE_STYLE,
                                                      PANGO_STYLE_NORMAL,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_VARIANT,
                                   g_param_spec_enum ("variant",
						      "Font variant",
						      "Font variant",
                                                      PANGO_TYPE_VARIANT,
                                                      PANGO_VARIANT_NORMAL,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_WEIGHT,
                                   g_param_spec_int ("weight",
                                                     "Font weight",
                                                     "Font weight",
                                                     0,
                                                     G_MAXINT,
                                                     PANGO_WEIGHT_NORMAL,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));

   g_object_class_install_property (object_class,
                                   PROP_STRETCH,
                                   g_param_spec_enum ("stretch",
                                                      "Font stretch",
                                                      "Font stretch",
                                                      PANGO_TYPE_STRETCH,
                                                      PANGO_STRETCH_NORMAL,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_SIZE,
                                   g_param_spec_int ("size",
                                                     "Font size",
                                                     "Font size",
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_SIZE_POINTS,
                                   g_param_spec_double ("size_points",
                                                        "Font points",
                                                        "Font size in points",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));  

  g_object_class_install_property (object_class,
                                   PROP_SCALE,
                                   g_param_spec_double ("scale",
                                                        "Font scale",
                                                        "Font scaling factor",
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        1.0,
                                                        G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_RISE,
                                   g_param_spec_int ("rise",
                                                     "Rise",
                                                     "Offset of text above the baseline (below the baseline if rise is negative)",
                                                     -G_MAXINT,
                                                     G_MAXINT,
                                                     0,
                                                     G_PARAM_READABLE | G_PARAM_WRITABLE));


  g_object_class_install_property (object_class,
                                   PROP_STRIKETHROUGH,
                                   g_param_spec_boolean ("strikethrough",
                                                         "Strikethrough",
                                                         "Whether to strike through the text",
                                                         FALSE,
                                                         G_PARAM_READABLE | G_PARAM_WRITABLE));
  
  g_object_class_install_property (object_class,
                                   PROP_UNDERLINE,
                                   g_param_spec_enum ("underline",
                                                      "Underline",
                                                      "Style of underline for this text",
                                                      PANGO_TYPE_UNDERLINE,
                                                      PANGO_UNDERLINE_NONE,
                                                      G_PARAM_READABLE | G_PARAM_WRITABLE));

  /* Style props are set or not */

#define ADD_SET_PROP(propname, propval, nick, blurb) g_object_class_install_property (object_class, propval, g_param_spec_boolean (propname, nick, blurb, FALSE, G_PARAM_READABLE | G_PARAM_WRITABLE))

  ADD_SET_PROP ("background_set", PROP_BACKGROUND_SET,
                _("Background set"),
                _("Whether this tag affects the background color"));

  ADD_SET_PROP ("foreground_set", PROP_FOREGROUND_SET,
                _("Foreground set"),
                _("Whether this tag affects the foreground color"));
  
  ADD_SET_PROP ("editable_set", PROP_EDITABLE_SET,
                _("Editability set"),
                _("Whether this tag affects text editability"));

  ADD_SET_PROP ("family_set", PROP_FAMILY_SET,
                _("Font family set"),
                _("Whether this tag affects the font family"));  

  ADD_SET_PROP ("style_set", PROP_STYLE_SET,
                _("Font style set"),
                _("Whether this tag affects the font style"));

  ADD_SET_PROP ("variant_set", PROP_VARIANT_SET,
                _("Font variant set"),
                _("Whether this tag affects the font variant"));

  ADD_SET_PROP ("weight_set", PROP_WEIGHT_SET,
                _("Font weight set"),
                _("Whether this tag affects the font weight"));

  ADD_SET_PROP ("stretch_set", PROP_STRETCH_SET,
                _("Font stretch set"),
                _("Whether this tag affects the font stretch"));

  ADD_SET_PROP ("size_set", PROP_SIZE_SET,
                _("Font size set"),
                _("Whether this tag affects the font size"));

  ADD_SET_PROP ("scale_set", PROP_SCALE_SET,
                _("Font scale set"),
                _("Whether this tag scales the font size by a factor"));
  
  ADD_SET_PROP ("rise_set", PROP_RISE_SET,
                _("Rise set"),
                _("Whether this tag affects the rise"));

  ADD_SET_PROP ("strikethrough_set", PROP_STRIKETHROUGH_SET,
                _("Strikethrough set"),
                _("Whether this tag affects strikethrough"));

  ADD_SET_PROP ("underline_set", PROP_UNDERLINE_SET,
                _("Underline set"),
                _("Whether this tag affects underlining"));

  text_cell_renderer_signals [EDITED] =
    g_signal_new ("edited",
		  G_OBJECT_CLASS_TYPE (object_class),
		  G_SIGNAL_RUN_LAST,
		  G_STRUCT_OFFSET (EggCellRendererTextClass, edited),
		  NULL, NULL,
		  _egg_marshal_VOID__STRING_STRING,
		  G_TYPE_NONE, 2,
		  G_TYPE_STRING,
		  G_TYPE_STRING);

}

static void
egg_cell_renderer_text_finalize (GObject *object)
{
  EggCellRendererText *celltext = EGG_CELL_RENDERER_TEXT (object);

  pango_font_description_free (celltext->font);

  if (celltext->text)
    g_free (celltext->text);

  if (celltext->extra_attrs)
    pango_attr_list_unref (celltext->extra_attrs);

  (* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static PangoFontMask
get_property_font_set_mask (guint prop_id)
{
  switch (prop_id)
    {
    case PROP_FAMILY_SET:
      return PANGO_FONT_MASK_FAMILY;
    case PROP_STYLE_SET:
      return PANGO_FONT_MASK_STYLE;
    case PROP_VARIANT_SET:
      return PANGO_FONT_MASK_VARIANT;
    case PROP_WEIGHT_SET:
      return PANGO_FONT_MASK_WEIGHT;
    case PROP_STRETCH_SET:
      return PANGO_FONT_MASK_STRETCH;
    case PROP_SIZE_SET:
      return PANGO_FONT_MASK_SIZE;
    }

  return 0;
}

static void
egg_cell_renderer_text_get_property (GObject        *object,
				     guint           param_id,
				     GValue         *value,
				     GParamSpec     *pspec)
{
  EggCellRendererText *celltext = EGG_CELL_RENDERER_TEXT (object);

  switch (param_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, celltext->text);
      break;

    case PROP_ATTRIBUTES:
      g_value_set_boxed (value, celltext->extra_attrs);
      break;

    case PROP_ELLIPSIZE:
      g_value_set_boolean (value, celltext->ellipsize);
      break;

    case PROP_BACKGROUND_GDK:
      {
        GdkColor color;
        
        color.red = celltext->background.red;
        color.green = celltext->background.green;
        color.blue = celltext->background.blue;
        
        g_value_set_boxed (value, &color);
      }
      break;

    case PROP_FOREGROUND_GDK:
      {
        GdkColor color;
        
        color.red = celltext->foreground.red;
        color.green = celltext->foreground.green;
        color.blue = celltext->foreground.blue;
        
        g_value_set_boxed (value, &color);
      }
      break;

    case PROP_FONT:
      {
        /* FIXME GValue imposes a totally gratuitous string copy
         * here, we could just hand off string ownership
         */
        gchar *str = pango_font_description_to_string (celltext->font);
        g_value_set_string (value, str);
        g_free (str);
      }
      break;
      
    case PROP_FONT_DESC:
      g_value_set_boxed (value, celltext->font);
      break;

    case PROP_FAMILY:
      g_value_set_string (value, pango_font_description_get_family (celltext->font));
      break;

    case PROP_STYLE:
      g_value_set_enum (value, pango_font_description_get_style (celltext->font));
      break;

    case PROP_VARIANT:
      g_value_set_enum (value, pango_font_description_get_variant (celltext->font));
      break;

    case PROP_WEIGHT:
      g_value_set_int (value, pango_font_description_get_weight (celltext->font));
      break;

    case PROP_STRETCH:
      g_value_set_enum (value, pango_font_description_get_stretch (celltext->font));
      break;

    case PROP_SIZE:
      g_value_set_int (value, pango_font_description_get_size (celltext->font));
      break;

    case PROP_SIZE_POINTS:
      g_value_set_double (value, ((double)pango_font_description_get_size (celltext->font)) / (double)PANGO_SCALE);
      break;

    case PROP_SCALE:
      g_value_set_double (value, celltext->font_scale);
      break;
      
    case PROP_EDITABLE:
      g_value_set_boolean (value, celltext->editable);
      break;

    case PROP_STRIKETHROUGH:
      g_value_set_boolean (value, celltext->strikethrough);
      break;

    case PROP_UNDERLINE:
      g_value_set_enum (value, celltext->underline_style);
      break;

    case PROP_RISE:
      g_value_set_int (value, celltext->rise);
      break;  

    case PROP_BACKGROUND_SET:
      g_value_set_boolean (value, celltext->background_set);
      break;

    case PROP_FOREGROUND_SET:
      g_value_set_boolean (value, celltext->foreground_set);
      break;

    case PROP_FAMILY_SET:
    case PROP_STYLE_SET:
    case PROP_VARIANT_SET:
    case PROP_WEIGHT_SET:
    case PROP_STRETCH_SET:
    case PROP_SIZE_SET:
      {
	PangoFontMask mask = get_property_font_set_mask (param_id);
	g_value_set_boolean (value, (pango_font_description_get_set_fields (celltext->font) & mask) != 0);
	
	break;
      }

    case PROP_SCALE_SET:
      g_value_set_boolean (value, celltext->scale_set);
      break;
      
    case PROP_EDITABLE_SET:
      g_value_set_boolean (value, celltext->editable_set);
      break;

    case PROP_STRIKETHROUGH_SET:
      g_value_set_boolean (value, celltext->strikethrough_set);
      break;

    case PROP_UNDERLINE_SET:
      g_value_set_boolean (value, celltext->underline_set);
      break;

    case  PROP_RISE_SET:
      g_value_set_boolean (value, celltext->rise_set);
      break;
      
    case PROP_BACKGROUND:
    case PROP_FOREGROUND:
    case PROP_MARKUP:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}


static void
set_bg_color (EggCellRendererText *celltext,
              GdkColor            *color)
{
  if (color)
    {
      if (!celltext->background_set)
        {
          celltext->background_set = TRUE;
          g_object_notify (G_OBJECT (celltext), "background_set");
        }
      
      celltext->background.red = color->red;
      celltext->background.green = color->green;
      celltext->background.blue = color->blue;
    }
  else
    {
      if (celltext->background_set)
        {
          celltext->background_set = FALSE;
          g_object_notify (G_OBJECT (celltext), "background_set");
        }
    }
}


static void
set_fg_color (EggCellRendererText *celltext,
              GdkColor            *color)
{
  if (color)
    {
      if (!celltext->foreground_set)
        {
          celltext->foreground_set = TRUE;
          g_object_notify (G_OBJECT (celltext), "foreground_set");
        }
      
      celltext->foreground.red = color->red;
      celltext->foreground.green = color->green;
      celltext->foreground.blue = color->blue;
    }
  else
    {
      if (celltext->foreground_set)
        {
          celltext->foreground_set = FALSE;
          g_object_notify (G_OBJECT (celltext), "foreground_set");
        }
    }
}

static PangoFontMask
set_font_desc_fields (PangoFontDescription *desc,
		      PangoFontMask         to_set)
{
  PangoFontMask changed_mask = 0;
  
  if (to_set & PANGO_FONT_MASK_FAMILY)
    {
      const char *family = pango_font_description_get_family (desc);
      if (!family)
	{
	  family = "sans";
	  changed_mask |= PANGO_FONT_MASK_FAMILY;
	}

      pango_font_description_set_family (desc, family);
    }
  if (to_set & PANGO_FONT_MASK_STYLE)
    pango_font_description_set_style (desc, pango_font_description_get_style (desc));
  if (to_set & PANGO_FONT_MASK_VARIANT)
    pango_font_description_set_variant (desc, pango_font_description_get_variant (desc));
  if (to_set & PANGO_FONT_MASK_WEIGHT)
    pango_font_description_set_weight (desc, pango_font_description_get_weight (desc));
  if (to_set & PANGO_FONT_MASK_STRETCH)
    pango_font_description_set_stretch (desc, pango_font_description_get_stretch (desc));
  if (to_set & PANGO_FONT_MASK_SIZE)
    {
      gint size = pango_font_description_get_size (desc);
      if (size <= 0)
	{
	  size = 10 * PANGO_SCALE;
	  changed_mask |= PANGO_FONT_MASK_SIZE;
	}
      
      pango_font_description_set_size (desc, size);
    }

  return changed_mask;
}

static void
notify_set_changed (GObject       *object,
		    PangoFontMask  changed_mask)
{
  if (changed_mask & PANGO_FONT_MASK_FAMILY)
    g_object_notify (object, "family_set");
  if (changed_mask & PANGO_FONT_MASK_STYLE)
    g_object_notify (object, "style_set");
  if (changed_mask & PANGO_FONT_MASK_VARIANT)
    g_object_notify (object, "variant_set");
  if (changed_mask & PANGO_FONT_MASK_WEIGHT)
    g_object_notify (object, "weight_set");
  if (changed_mask & PANGO_FONT_MASK_STRETCH)
    g_object_notify (object, "stretch_set");
  if (changed_mask & PANGO_FONT_MASK_SIZE)
    g_object_notify (object, "size_set");
}

static void
notify_fields_changed (GObject       *object,
		       PangoFontMask  changed_mask)
{
  if (changed_mask & PANGO_FONT_MASK_FAMILY)
    g_object_notify (object, "family");
  if (changed_mask & PANGO_FONT_MASK_STYLE)
    g_object_notify (object, "style");
  if (changed_mask & PANGO_FONT_MASK_VARIANT)
    g_object_notify (object, "variant");
  if (changed_mask & PANGO_FONT_MASK_WEIGHT)
    g_object_notify (object, "weight");
  if (changed_mask & PANGO_FONT_MASK_STRETCH)
    g_object_notify (object, "stretch");
  if (changed_mask & PANGO_FONT_MASK_SIZE)
    g_object_notify (object, "size");
}

static void
set_font_description (EggCellRendererText  *celltext,
                      PangoFontDescription *font_desc)
{
  GObject *object = G_OBJECT (celltext);
  PangoFontDescription *new_font_desc;
  PangoFontMask old_mask, new_mask, changed_mask, set_changed_mask;
  
  if (font_desc)
    new_font_desc = pango_font_description_copy (font_desc);
  else
    new_font_desc = pango_font_description_new ();

  old_mask = pango_font_description_get_set_fields (celltext->font);
  new_mask = pango_font_description_get_set_fields (new_font_desc);

  changed_mask = old_mask | new_mask;
  set_changed_mask = old_mask ^ new_mask;

  pango_font_description_free (celltext->font);
  celltext->font = new_font_desc;
  
  g_object_freeze_notify (object);

  g_object_notify (object, "font_desc");
  g_object_notify (object, "font");
  
  if (changed_mask & PANGO_FONT_MASK_FAMILY)
    g_object_notify (object, "family");
  if (changed_mask & PANGO_FONT_MASK_STYLE)
    g_object_notify (object, "style");
  if (changed_mask & PANGO_FONT_MASK_VARIANT)
    g_object_notify (object, "variant");
  if (changed_mask & PANGO_FONT_MASK_WEIGHT)
    g_object_notify (object, "weight");
  if (changed_mask & PANGO_FONT_MASK_STRETCH)
    g_object_notify (object, "stretch");
  if (changed_mask & PANGO_FONT_MASK_SIZE)
    {
      g_object_notify (object, "size");
      g_object_notify (object, "size_points");
    }

  notify_set_changed (object, set_changed_mask);
  
  g_object_thaw_notify (object);
}

static void
egg_cell_renderer_text_set_property (GObject      *object,
				     guint         param_id,
				     const GValue *value,
				     GParamSpec   *pspec)
{
  EggCellRendererText *celltext = EGG_CELL_RENDERER_TEXT (object);

  switch (param_id)
    {
    case PROP_TEXT:
      if (celltext->text)
        g_free (celltext->text);
      celltext->text = g_strdup (g_value_get_string (value));
      g_object_notify (object, "text");
      break;

    case PROP_ATTRIBUTES:
      if (celltext->extra_attrs)
	pango_attr_list_unref (celltext->extra_attrs);

      celltext->extra_attrs = g_value_get_boxed (value);
      if (celltext->extra_attrs)
        pango_attr_list_ref (celltext->extra_attrs);
      break;

    case PROP_ELLIPSIZE:
      egg_cell_renderer_text_set_ellipsize (celltext,
					    g_value_get_boolean (value));
      break;

    case PROP_MARKUP:
      {
	const gchar *str;
	gchar *text = NULL;
	GError *error = NULL;
	PangoAttrList *attrs = NULL;

	str = g_value_get_string (value);
	if (str && !pango_parse_markup (str,
					-1,
					0,
					&attrs,
					&text,
					NULL,
					&error))
	  {
	    g_warning ("Failed to set cell text from markup due to error parsing markup: %s",
		       error->message);
	    g_error_free (error);
	    return;
	  }

	if (celltext->text)
	  g_free (celltext->text);

	if (celltext->extra_attrs)
	  pango_attr_list_unref (celltext->extra_attrs);

	celltext->text = text;
	celltext->extra_attrs = attrs;
      }
      break;
      
    case PROP_BACKGROUND:
      {
        GdkColor color;

        if (!g_value_get_string (value))
          set_bg_color (celltext, NULL);       /* reset to backgrounmd_set to FALSE */
        else if (gdk_color_parse (g_value_get_string (value), &color))
          set_bg_color (celltext, &color);
        else
          g_warning ("Don't know color `%s'", g_value_get_string (value));

        g_object_notify (object, "background_gdk");
      }
      break;
      
    case PROP_FOREGROUND:
      {
        GdkColor color;

        if (!g_value_get_string (value))
          set_fg_color (celltext, NULL);       /* reset to foreground_set to FALSE */
        else if (gdk_color_parse (g_value_get_string (value), &color))
          set_fg_color (celltext, &color);
        else
          g_warning ("Don't know color `%s'", g_value_get_string (value));

        g_object_notify (object, "foreground_gdk");
      }
      break;

    case PROP_BACKGROUND_GDK:
      /* This notifies the GObject itself. */
      set_bg_color (celltext, g_value_get_boxed (value));
      break;

    case PROP_FOREGROUND_GDK:
      /* This notifies the GObject itself. */
      set_fg_color (celltext, g_value_get_boxed (value));
      break;

    case PROP_FONT:
      {
        PangoFontDescription *font_desc = NULL;
        const gchar *name;

        name = g_value_get_string (value);

        if (name)
          font_desc = pango_font_description_from_string (name);

        set_font_description (celltext, font_desc);
        
	if (celltext->fixed_height_rows != -1)
	  celltext->calc_fixed_height = TRUE;
      }
      break;

    case PROP_FONT_DESC:
      set_font_description (celltext, g_value_get_boxed (value));
      
      if (celltext->fixed_height_rows != -1)
	celltext->calc_fixed_height = TRUE;
      break;

    case PROP_FAMILY:
    case PROP_STYLE:
    case PROP_VARIANT:
    case PROP_WEIGHT:
    case PROP_STRETCH:
    case PROP_SIZE:
    case PROP_SIZE_POINTS:
      {
	PangoFontMask old_set_mask = pango_font_description_get_set_fields (celltext->font);
	
	switch (param_id)
	  {
	  case PROP_FAMILY:
	    pango_font_description_set_family (celltext->font,
					       g_value_get_string (value));
	    break;
	  case PROP_STYLE:
	    pango_font_description_set_style (celltext->font,
					      g_value_get_enum (value));
	    break;
	  case PROP_VARIANT:
	    pango_font_description_set_variant (celltext->font,
						g_value_get_enum (value));
	    break;
	  case PROP_WEIGHT:
	    pango_font_description_set_weight (celltext->font,
					       g_value_get_int (value));
	    break;
	  case PROP_STRETCH:
	    pango_font_description_set_stretch (celltext->font,
						g_value_get_enum (value));
	    break;
	  case PROP_SIZE:
	    pango_font_description_set_size (celltext->font,
					     g_value_get_int (value));
	    g_object_notify (object, "size_points");
	    break;
	  case PROP_SIZE_POINTS:
	    pango_font_description_set_size (celltext->font,
					     g_value_get_double (value) * PANGO_SCALE);
	    g_object_notify (object, "size");
	    break;
	  }
	
	if (celltext->fixed_height_rows != -1)
	  celltext->calc_fixed_height = TRUE;
	
	notify_set_changed (object, old_set_mask & pango_font_description_get_set_fields (celltext->font));
	g_object_notify (object, "font_desc");
	g_object_notify (object, "font");

	break;
      }
      
    case PROP_SCALE:
      celltext->font_scale = g_value_get_double (value);
      celltext->scale_set = TRUE;
      if (celltext->fixed_height_rows != -1)
	celltext->calc_fixed_height = TRUE;
      g_object_notify (object, "scale_set");
      break;
      
    case PROP_EDITABLE:
      celltext->editable = g_value_get_boolean (value);
      celltext->editable_set = TRUE;
      if (celltext->editable)
        GTK_CELL_RENDERER (celltext)->mode = GTK_CELL_RENDERER_MODE_EDITABLE;
      else
        GTK_CELL_RENDERER (celltext)->mode = GTK_CELL_RENDERER_MODE_INERT;
      g_object_notify (object, "editable_set");
      break;

    case PROP_STRIKETHROUGH:
      celltext->strikethrough = g_value_get_boolean (value);
      celltext->strikethrough_set = TRUE;
      g_object_notify (object, "strikethrough_set");
      break;

    case PROP_UNDERLINE:
      celltext->underline_style = g_value_get_enum (value);
      celltext->underline_set = TRUE;
      g_object_notify (object, "underline_set");
            
      break;

    case PROP_RISE:
      celltext->rise = g_value_get_int (value);
      celltext->rise_set = TRUE;
      g_object_notify (object, "rise_set");
      if (celltext->fixed_height_rows != -1)
	celltext->calc_fixed_height = TRUE;
      break;  

    case PROP_BACKGROUND_SET:
      celltext->background_set = g_value_get_boolean (value);
      break;

    case PROP_FOREGROUND_SET:
      celltext->foreground_set = g_value_get_boolean (value);
      break;

    case PROP_FAMILY_SET:
    case PROP_STYLE_SET:
    case PROP_VARIANT_SET:
    case PROP_WEIGHT_SET:
    case PROP_STRETCH_SET:
    case PROP_SIZE_SET:
      if (!g_value_get_boolean (value))
	{
	  pango_font_description_unset_fields (celltext->font,
					       get_property_font_set_mask (param_id));
	}
      else
	{
	  PangoFontMask changed_mask;
	  
	  changed_mask = set_font_desc_fields (celltext->font,
					       get_property_font_set_mask (param_id));
	  notify_fields_changed (G_OBJECT (celltext), changed_mask);
	}
      break;

    case PROP_SCALE_SET:
      celltext->scale_set = g_value_get_boolean (value);
      break;
      
    case PROP_EDITABLE_SET:
      celltext->editable_set = g_value_get_boolean (value);
      break;

    case PROP_STRIKETHROUGH_SET:
      celltext->strikethrough_set = g_value_get_boolean (value);
      break;

    case PROP_UNDERLINE_SET:
      celltext->underline_set = g_value_get_boolean (value);
      break;

    case PROP_RISE_SET:
      celltext->rise_set = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}

/**
 * egg_cell_renderer_text_new:
 * 
 * Creates a new #EggCellRendererText. Adjust how text is drawn using
 * object properties. Object properties can be
 * set globally (with g_object_set()). Also, with #GtkTreeViewColumn,
 * you can bind a property to a value in a #GtkTreeModel. For example,
 * you can bind the "text" property on the cell renderer to a string
 * value in the model, thus rendering a different string in each row
 * of the #GtkTreeView
 * 
 * Return value: the new cell renderer
 **/
GtkCellRenderer *
egg_cell_renderer_text_new (void)
{
  return g_object_new (EGG_TYPE_CELL_RENDERER_TEXT, NULL);
}

static void
add_attr (PangoAttrList  *attr_list,
          PangoAttribute *attr)
{
  attr->start_index = 0;
  attr->end_index = G_MAXINT;
  
  pango_attr_list_insert (attr_list, attr);
}

static PangoLayout*
get_layout (EggCellRendererText *celltext,
            GtkWidget           *widget,
            gboolean             will_render,
	    gboolean             ellipsize,
	    gint                 alloc_width,
            GtkCellRendererState flags)
{
  PangoAttrList *attr_list;
  PangoLayout *layout;
  PangoUnderline uline;

  if (celltext->extra_attrs)
    attr_list = pango_attr_list_copy (celltext->extra_attrs);
  else
    attr_list = pango_attr_list_new ();

  if (will_render)
    {
      /* Add options that affect appearance but not size */
      
      /* note that background doesn't go here, since it affects
       * background_area not the PangoLayout area
       */
      
      if (celltext->foreground_set)
        {
          PangoColor color;

          color = celltext->foreground;
          
          add_attr (attr_list,
                    pango_attr_foreground_new (color.red, color.green, color.blue));
        }

      if (celltext->strikethrough_set)
        add_attr (attr_list,
                  pango_attr_strikethrough_new (celltext->strikethrough));
    }

  add_attr (attr_list, pango_attr_font_desc_new (celltext->font));

  if (celltext->scale_set &&
      celltext->font_scale != 1.0)
    add_attr (attr_list, pango_attr_scale_new (celltext->font_scale));
  
  if (celltext->underline_set)
    uline = celltext->underline_style;
  else
    uline = PANGO_UNDERLINE_NONE;
  
  if ((flags & GTK_CELL_RENDERER_PRELIT) == GTK_CELL_RENDERER_PRELIT)
    {
      switch (uline)
        {
        case PANGO_UNDERLINE_NONE:
          uline = PANGO_UNDERLINE_SINGLE;
          break;

        case PANGO_UNDERLINE_SINGLE:
          uline = PANGO_UNDERLINE_DOUBLE;
          break;

        default:
          break;
        }
    }

  if (uline != PANGO_UNDERLINE_NONE)
    add_attr (attr_list, pango_attr_underline_new (celltext->underline_style));

  if (celltext->rise_set)
    add_attr (attr_list, pango_attr_rise_new (celltext->rise));
  
  layout = gtk_widget_create_pango_layout (widget, NULL);

  pango_layout_set_attributes (layout, attr_list);
  pango_layout_set_width (layout, -1);

  pango_attr_list_unref (attr_list);
  
  if (!ellipsize)
    {
      pango_layout_set_text (layout, celltext->text, -1);
    }
  else
    {
      eel_pango_layout_set_text_ellipsized (layout,
					    celltext->text,
					    alloc_width,
					    EEL_ELLIPSIZE_END);
    }

  return layout;
}

static void
egg_cell_renderer_text_get_size (GtkCellRenderer *cell,
				 GtkWidget       *widget,
				 GdkRectangle    *cell_area,
				 gint            *x_offset,
				 gint            *y_offset,
				 gint            *width,
				 gint            *height)
{
  EggCellRendererText *celltext = (EggCellRendererText *) cell;
  PangoRectangle rect;
  PangoLayout *layout;

  if (celltext->calc_fixed_height)
    {
      PangoContext *context;
      PangoFontMetrics *metrics;
      PangoFontDescription *font_desc;
      gint row_height;

      font_desc = pango_font_description_copy (widget->style->font_desc);
      pango_font_description_merge (font_desc, celltext->font, TRUE);

      if (celltext->scale_set)
	pango_font_description_set_size (font_desc,
					 celltext->font_scale * pango_font_description_get_size (font_desc));

      context = gtk_widget_get_pango_context (widget);

      metrics = pango_context_get_metrics (context,
					   font_desc,
					   pango_context_get_language (context));
      row_height = (pango_font_metrics_get_ascent (metrics) +
		    pango_font_metrics_get_descent (metrics));
      pango_font_metrics_unref (metrics);
      
      gtk_cell_renderer_set_fixed_size (cell,
					cell->width, 2*cell->ypad +
					celltext->fixed_height_rows * PANGO_PIXELS (row_height));
      
      if (height)
	{
	  *height = cell->height;
	  height = NULL;
	}
      celltext->calc_fixed_height = FALSE;
      if (width == NULL)
	return;
    }
  layout = get_layout (celltext, widget, FALSE, FALSE, -1, 0);
  pango_layout_get_pixel_extents (layout, NULL, &rect);

  if (width)
    *width = GTK_CELL_RENDERER (celltext)->xpad * 2 + rect.width;

  if (height)
    *height = GTK_CELL_RENDERER (celltext)->ypad * 2 + rect.height;

  if (cell_area)
    {
      if (x_offset)
	{
	  *x_offset = ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ?
		       (1.0 - cell->xalign) : cell->xalign) * (cell_area->width - rect.width - (2 * cell->xpad));
	  *x_offset = MAX (*x_offset, 0);
	}
      if (y_offset)
	{
	  *y_offset = cell->yalign * (cell_area->height - rect.height - (2 * cell->ypad));
	  *y_offset = MAX (*y_offset, 0);
	}
    }

  g_object_unref (layout);
}

static void
egg_cell_renderer_text_render (GtkCellRenderer      *cell,
			       GdkDrawable          *window,
			       GtkWidget            *widget,
			       GdkRectangle         *background_area,
			       GdkRectangle         *cell_area,
			       GdkRectangle         *expose_area,
			       GtkCellRendererState  flags)

{
  EggCellRendererText *celltext = (EggCellRendererText *) cell;
  PangoLayout *layout;
  GtkStateType state;
  gint x_offset;
  gint y_offset;
  gboolean ellipsize;
  gint alloc_width;

  g_assert (GTK_IS_TREE_VIEW (widget));

  /* FIXME: here's the hacky bit - don't think this is guaranteed to work */
  alloc_width = widget->allocation.width - cell_area->x;
  ellipsize = celltext->ellipsize && (alloc_width < cell_area->width);

  layout = get_layout (celltext, widget, TRUE, ellipsize, alloc_width, flags);

  egg_cell_renderer_text_get_size (cell, widget, cell_area, &x_offset, &y_offset, NULL, NULL);

  if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED)
    {
      if (GTK_WIDGET_HAS_FOCUS (widget))
	state = GTK_STATE_SELECTED;
      else
	state = GTK_STATE_ACTIVE;
    }
  else
    {
      if (GTK_WIDGET_STATE (widget) == GTK_STATE_INSENSITIVE)
	state = GTK_STATE_INSENSITIVE;
      else
	state = GTK_STATE_NORMAL;
    }

  if (celltext->background_set && state != GTK_STATE_SELECTED)
    {
      GdkColor color;
      GdkGC *gc;
      
      color.red = celltext->background.red;
      color.green = celltext->background.green;
      color.blue = celltext->background.blue;

      gc = gdk_gc_new (window);

      gdk_gc_set_rgb_fg_color (gc, &color);
      
      gdk_draw_rectangle (window,
                          gc,
                          TRUE,
                          background_area->x,
                          background_area->y,
                          background_area->width,
                          background_area->height);

      g_object_unref (gc);
    }

  gtk_paint_layout (widget->style,
                    window,
                    state,
		    TRUE,
                    cell_area,
                    widget,
                    "cellrenderertext",
                    cell_area->x + x_offset + cell->xpad,
                    cell_area->y + y_offset + cell->ypad,
                    layout);

  g_object_unref (layout);
}

#ifndef DISABLE_EDITING
static void
egg_cell_renderer_text_editing_done (GtkCellEditable *entry,
				     gpointer         data)
{
  const gchar *path;
  const gchar *new_text;
  EggCellRendererInfo *info;

  info = g_object_get_data (G_OBJECT (data),
		            EGG_CELL_RENDERER_INFO_KEY);

  if (info->focus_out_id > 0)
    {
      g_signal_handler_disconnect (entry, info->focus_out_id);
      info->focus_out_id = 0;
    }

  if (GTK_ENTRY (entry)->editing_canceled)
    return;

  path = g_object_get_data (G_OBJECT (entry), EGG_CELL_RENDERER_TEXT_PATH);
  new_text = gtk_entry_get_text (GTK_ENTRY (entry));

  g_signal_emit (data, text_cell_renderer_signals[EDITED], 0, path, new_text);
}

static gboolean
egg_cell_renderer_text_focus_out_event (GtkWidget *entry,
		                        GdkEvent  *event,
					gpointer   data)
{
  egg_cell_renderer_text_editing_done (GTK_CELL_EDITABLE (entry), data);

  /* entry needs focus-out-event */
  return FALSE;
}

static GtkCellEditable *
egg_cell_renderer_text_start_editing (GtkCellRenderer      *cell,
				      GdkEvent             *event,
				      GtkWidget            *widget,
				      const gchar          *path,
				      GdkRectangle         *background_area,
				      GdkRectangle         *cell_area,
				      GtkCellRendererState  flags)
{
  EggCellRendererText *celltext;
  GtkWidget *entry;
  EggCellRendererInfo *info;
  
  celltext = EGG_CELL_RENDERER_TEXT (cell);

  /* If the cell isn't editable we return NULL. */
  if (celltext->editable == FALSE)
    return NULL;

  entry = g_object_new (GTK_TYPE_ENTRY,
			"has_frame", FALSE,
			NULL);

  if (celltext->text)
    gtk_entry_set_text (GTK_ENTRY (entry), celltext->text);
  g_object_set_data_full (G_OBJECT (entry), EGG_CELL_RENDERER_TEXT_PATH, g_strdup (path), g_free);
  
  gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
  
  info = g_object_get_data (G_OBJECT (cell),
			    EGG_CELL_RENDERER_INFO_KEY);
  
  gtk_widget_show (entry);
  g_signal_connect (entry,
		    "editing_done",
		    G_CALLBACK (egg_cell_renderer_text_editing_done),
		    celltext);
  info->focus_out_id = g_signal_connect (entry, "focus_out_event",
		         G_CALLBACK (egg_cell_renderer_text_focus_out_event),
		         celltext);

  return GTK_CELL_EDITABLE (entry);

}
#endif /* DISABLE_EDITING */

/**
 * egg_cell_renderer_text_set_fixed_height_from_font:
 * @renderer: A #EggCellRendererText
 * @number_of_rows: Number of rows of text each cell renderer is allocated, or -1
 * 
 * Sets the height of a renderer to explicitly be determined by the "font" and
 * "y_pad" property set on it.  Further changes in these properties do not
 * affect the height, so they must be accompanied by a subsequent call to this
 * function.  Using this function is unflexible, and should really only be used
 * if calculating the size of a cell is too slow (ie, a massive number of cells
 * displayed).  If @number_of_rows is -1, then the fixed height is unset, and
 * the height is determined by the properties again.
 **/
void
egg_cell_renderer_text_set_fixed_height_from_font (EggCellRendererText *renderer,
						   gint                 number_of_rows)
{
  g_return_if_fail (EGG_IS_CELL_RENDERER_TEXT (renderer));
  g_return_if_fail (number_of_rows == -1 || number_of_rows > 0);

  if (number_of_rows == -1)
    {
      gtk_cell_renderer_set_fixed_size (GTK_CELL_RENDERER (renderer),
					GTK_CELL_RENDERER (renderer)->width,
					-1);
    }
  else
    {
      renderer->fixed_height_rows = number_of_rows;
      renderer->calc_fixed_height = TRUE;
    }
}

void
egg_cell_renderer_text_set_ellipsize (EggCellRendererText *renderer,
				      gboolean             ellipsize)
{
  g_return_if_fail (EGG_IS_CELL_RENDERER_TEXT (renderer));

  ellipsize = ellipsize != FALSE;

  if (renderer->ellipsize != ellipsize)
    {
      renderer->ellipsize = ellipsize;

      /* FIXME: update */

      g_object_notify (G_OBJECT (renderer), "ellipsize");
    }
}

gboolean
egg_cell_renderer_text_get_ellipsize (EggCellRendererText *renderer)
{
  g_return_val_if_fail (EGG_IS_CELL_RENDERER_TEXT (renderer), FALSE);

  return renderer->ellipsize;
}
