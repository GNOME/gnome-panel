/* GwmThumbNail - DeskGuide ThumbNail maintenance
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include	"gwmthumbnail.h"


#define	USE_SHM	// FIXME


/* --- defines --- */
#define	SHM_IMAGE_HEIGHT	(32)


/* --- Gdk & GdkPixbuf compat prototypes --- */
GdkImage*       gdk_image_get                    (GdkWindow *window,
						  gint       x,
						  gint       y,
						  gint       width,
						  gint       height);
GdkColormap*	gdk_window_get_colormap		 (GdkWindow *window);
GdkVisual*	gdk_window_get_visual		 (GdkWindow *window);
GdkImage*       gdk_image_new_shared_with_pixmap (GdkWindow  *window,
						  gint        width,
						  gint        height,
						  GdkPixmap **pixmap_p);
GdkPixbuf*      gdk_pixbuf_from_image            (GdkImage    *image,
						  guint        x,
						  guint        y,
						  guint        width,
						  guint        height,
						  GdkColormap *cmap);
void            gdk_pixbuf_copy_from_image       (GdkPixbuf   *pixbuf,
						  guint        dest_x,
						  guint        dest_y,
						  GdkImage    *image,
						  guint        src_x,
						  guint        src_y,
						  guint        width,
						  guint        height,
						  GdkColormap *cmap);


/* --- structures --- */
typedef struct _ThumbSize ThumbSize;
struct _ThumbSize
{
  ThumbSize *next;
  glong      request_id;
  guint      width;
  guint      height;
};


/* --- variables --- */
static gboolean gwm_thumb_nails_enabled = FALSE;
static GSList  *gwm_thumb_nails = NULL;


/* --- functions --- */
void
gwm_thumb_nails_set_active (gboolean thumb_nails_enabled)
{
  gwm_thumb_nails_enabled = thumb_nails_enabled != FALSE;
  if (!gwm_thumb_nails_enabled)
    while (gwm_thumb_nails)
      gwm_thumb_nail_destroy (gwm_thumb_nails->data);
}

GwmThumbNail*
gwm_thumb_nail_new (guint          default_color,
		    guint          width,
		    guint          height,
		    glong          grow_request_id,
		    gpointer       user_data,
		    GDestroyNotify dtor)
{
  GwmThumbNail *nail;

  g_return_val_if_fail (grow_request_id != 0, NULL);

  nail = g_new (GwmThumbNail, 1);
  gwm_thumb_nails = g_slist_prepend (gwm_thumb_nails, nail);
  nail->pixbuf = NULL;
  nail->width = 0;
  nail->height = 0;
  nail->thumb_row = 0;
  nail->default_color = default_color & 0x00ffffff;
  nail->size_list = NULL;
  nail->user_data = user_data;
  nail->dtor = dtor;
  gwm_thumb_nail_grow (nail, width, height, grow_request_id);

  return nail;
}

void
gwm_thumb_nail_destroy (GwmThumbNail *nail)
{
  GDestroyNotify dtor;
  ThumbSize *node;

  g_return_if_fail (nail != NULL);
  g_return_if_fail (nail->pixbuf != NULL);

  gwm_thumb_nails = g_slist_remove (gwm_thumb_nails, nail);
  dtor = nail->dtor;
  nail->dtor = NULL;
  gdk_pixbuf_unref (nail->pixbuf);
  nail->pixbuf = NULL;
  node = nail->size_list;
  while (node)
    {
      ThumbSize *next = node->next;

      g_free (node);
      node = next;
    }
  dtor (nail->user_data);
  g_free (nail);
}

void
gwm_thumb_nail_flag_reload (GwmThumbNail *nail)
{
  g_return_if_fail (nail != NULL);

  nail->thumb_row = -1;
}

static inline void
_thumb_nail_resize (GwmThumbNail *nail,
		    gint          width,
		    gint          height)
{
  gint d = (nail->width - width + nail->height - height) / 2;

  nail->width = width;
  nail->height = height;

  if (!nail->pixbuf)
    {
      guint x, y, rowstride, color;
      guint8 *pixels, red, green, blue;

      nail->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
      rowstride = gdk_pixbuf_get_rowstride (nail->pixbuf);
      pixels = gdk_pixbuf_get_pixels (nail->pixbuf);
      color = nail->default_color & 0xffffff;
      red = (nail->default_color >> 16) & 0xff;
      green = (nail->default_color >> 8) & 0xff;
      blue = nail->default_color & 0xff;
      for (y = 0; y < height; y++)
	{
	  guint8 *d = pixels + y * rowstride;

	  for (x = 0; x < width; x++)
	    {
	      *d++ = red;
	      *d++ = green;
	      *d++ = blue;
	    }
	}
    }
  else
    {
      GdkPixbuf *pixbuf = gdk_pixbuf_scale_simple (nail->pixbuf, width, height, d <= 8 ? GDK_INTERP_NEAREST : GDK_INTERP_BILINEAR);

      gdk_pixbuf_unref (nail->pixbuf);
      nail->pixbuf = pixbuf;
    }
}

void
gwm_thumb_nail_grow (GwmThumbNail *nail,
		     guint         width,
		     guint         height,
		     glong         request_id)
{
  ThumbSize *node;
  guint max_width = 1, max_height = 1;

  g_return_if_fail (nail != NULL);
  g_return_if_fail (request_id != 0);

  /* update/add size request, collect max size on the way */
  for (node = nail->size_list; node; node = node->next)
    {
      if (node->request_id == request_id)
	{
	  node->width = width;
	  node->height = height;
	  request_id = 0;
	}
      max_width = MAX (max_width, node->width);
      max_height = MAX (max_height, node->height);
    }
  if (request_id)
    {
      node = g_new (ThumbSize, 1);
      node->next = nail->size_list;
      nail->size_list = node;
      node->request_id = request_id;
      node->width = width;
      node->height = height;
      max_width = MAX (max_width, node->width);
      max_height = MAX (max_height, node->height);
    }

  /* ok, adapt thumb nail to new size */
  if (max_width != nail->width || max_height != nail->height)
    _thumb_nail_resize (nail, max_width, max_height);
}

void
gwm_thumb_nail_ungrow (GwmThumbNail *nail,
		       glong         request_id)
{
  ThumbSize *node, *last;
  guint max_width = 1, max_height = 1;

  g_return_if_fail (nail != NULL);
  g_return_if_fail (request_id != 0);

  /* remove size request, collect max size on the way */
  last = NULL;
  node = nail->size_list;
  while (node)
    {
      if (node->request_id == request_id)
	{
	  if (last)
	    last->next = node->next;
	  else
	    nail->size_list = node->next;
	  g_free (node);
	  continue;
	}
      max_width = MAX (max_width, node->width);
      max_height = MAX (max_height, node->height);
      last = node;
      node = last->next;
    }

  /* ok, adapt thumb nail to new size */
  if (max_width != nail->width || max_height != nail->height)
    _thumb_nail_resize (nail, max_width, max_height);
}

static GdkPixbuf*
pixbuf_from_drawable (GdkDrawable *drawable,
		      gint         x,
		      gint         y,
		      gint         width,
		      gint         height)
{
  static GdkImage *shm_image = NULL;
  static GdkPixmap *shm_pixmap = NULL;
  gint swidth = gdk_screen_width ();
  gint sheight = gdk_screen_height ();
  GdkColormap *cmap = gdk_window_get_colormap (drawable);
  GdkPixbuf *shot_pixbuf = NULL;

  /* the drawable relative rectangle (x, y, width, height) must be
   * confined to the root window rectangle (0, 0, swidth, sheight)
   * we don't check that here, since that involves server roundtrips
   */

  if (!shm_image)
    shm_image = gdk_image_new_shared_with_pixmap (NULL, swidth, SHM_IMAGE_HEIGHT, &shm_pixmap);

  /* fallback to root window's colormap */
  if (!cmap)
    {
      GdkVisual *visual = gdk_window_get_visual (drawable);

      if (visual && visual->depth == gdk_window_get_visual (NULL)->depth)
	cmap = gdk_window_get_colormap (NULL);

      if (!cmap)	/* guess we're screwed */
	return NULL;
    }

  /* for depth matches, we first try to grab the image using our global
   * shared memory pixmap image
   */
  if (shm_image && shm_image->depth == gdk_window_get_visual (drawable)->depth)
    {
      GdkGCValues gc_values;
      GdkGC *gc;
      gint pix_y;

      gc_values.subwindow_mode = GDK_INCLUDE_INFERIORS;
      gc = gdk_gc_new_with_values (drawable, &gc_values, GDK_GC_SUBWINDOW);

      shot_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);

      /* copy the region incrementally, since the shared image is height-limited
       */
      pix_y = 0;
      while (height)
	{
	  gint n = MIN (height, SHM_IMAGE_HEIGHT);

	  gdk_error_trap_push ();
	  gdk_draw_pixmap (shm_pixmap, gc, drawable, x, y, 0, 0, width, n);
	  gdk_flush ();
	  if (gdk_error_trap_pop ())
	    {
	      gdk_pixbuf_unref (shot_pixbuf);
	      shot_pixbuf = NULL;
	      break;
	    }
	  gdk_pixbuf_copy_from_image (shot_pixbuf, 0, pix_y, shm_image, 0, 0, width, n, cmap);
	  height -= n;
	  y += n;
	  pix_y += n;
	}

      gdk_gc_unref (gc);
    }

  /* ok, we might as well just try to create a shared image on the fly */
  if (!shot_pixbuf && cmap)
    {
      GdkImage *image;
      GdkPixmap *pixmap;

      image = gdk_image_new_shared_with_pixmap (drawable, swidth, sheight, &pixmap);
      if (image)
	{
	  GdkGCValues gc_values;
	  GdkGC *gc;

	  gc_values.subwindow_mode = GDK_INCLUDE_INFERIORS;
	  gc = gdk_gc_new_with_values (drawable, &gc_values, GDK_GC_SUBWINDOW);

          gdk_error_trap_push ();
	  gdk_draw_pixmap (pixmap, gc, drawable, x, y, 0, 0, width, height);
	  gdk_flush ();
          if (!gdk_error_trap_pop ())
	    shot_pixbuf = gdk_pixbuf_from_image (image, 0, 0, width, height, cmap);
	  gdk_gc_unref (gc);

	  gdk_image_destroy (image);
	  gdk_pixmap_unref (pixmap);
	}
    }

  /* last resort, do it slowly ;) */
  if (!shot_pixbuf)
    {
      GdkImage *image = gdk_image_get (drawable, x, y, width, height);

      if (image)
	{
	  shot_pixbuf = gdk_pixbuf_from_image (image, 0, 0, width, height, cmap);
	  gdk_image_destroy (image);
	}
    }

  return shot_pixbuf;
}

gboolean
gwm_thumb_nail_update_drawable (GwmThumbNail *nail,
				GdkDrawable  *drawable,
				gint	      drawable_x,
				gint	      drawable_y)
{
  GdkPixbuf *pixbuf;
  gint swidth = gdk_screen_width ();
  gint sheight = gdk_screen_height ();
  gdouble xscale, yscale;
  gint thumb_x0, thumb_y0, thumb_x1, thumb_y1;
  guint8 *dest_pixels, *src_pixels;
  gint drowstride, srowstride, xx, yy;
  gdouble drawable_width, drawable_height;

  g_return_val_if_fail (nail != NULL, FALSE);
  g_return_val_if_fail (drawable != NULL, FALSE);

  gdk_window_get_size (drawable, &xx, &yy);
  drawable_width = xx;
  drawable_height = yy;

  /* check out of screen bounds */
  if (drawable_x >= swidth || drawable_y >= sheight ||
      drawable_x + drawable_width <= 0 || drawable_y + drawable_height <= 0)
    return FALSE;

  xscale = drawable_width / nail->width;
  yscale = drawable_height / nail->height;

  /* left bound */
  thumb_x0 = 0;
  if (drawable_x < 0)
    {
      thumb_x0 = (- drawable_x) / xscale + 0.5;
      /* confine to tiles within swidth */
      if (0 > (gint) (thumb_x0 * xscale))
	thumb_x0 += 1;
    }
  /* right bound */
  thumb_x1 = MIN (drawable_width, swidth - drawable_x) / xscale;
  /* g_print ("drawable_x=%d drawable_width=%d xscale=%g thumb_x0=%d thumb_x1=%d swidth=%d\n", drawable_x, (gint) drawable_width, xscale, thumb_x0, thumb_x1, swidth); */
  g_assert (thumb_x0 <= nail->width && thumb_x1 <= nail->width);
  /* upper bound */
  thumb_y0 = 0;
  if (drawable_y < 0)
    {
      thumb_y0 = (- drawable_y) / yscale + 0.5;
      /* confine to tiles within sheight */
      if (0 > (gint) (thumb_y0 * yscale))
	thumb_y0 += 1;
    }
  /* lower bound */
  thumb_y1 = MIN (drawable_height, sheight - drawable_y) / yscale;
  /* g_print ("drawable_y=%d drawable_height=%d yscale=%g thumb_y0=%d thumb_y1=%d sheight=%d\n", drawable_y, (gint) drawable_height, yscale, thumb_y0, thumb_y1, sheight); */
  g_assert (thumb_y0 <= nail->height && thumb_y1 <= nail->height);

  /* check for valid tiles */
  if (thumb_x1 <= thumb_x0 || thumb_y1 <= thumb_y0)
    return FALSE;

  /* confine to incremental mark */
  if (nail->thumb_row >= 0)
    {
      if (nail->thumb_row >= thumb_y1 || nail->thumb_row < thumb_y0)
	nail->thumb_row = thumb_y0;
      thumb_y0 = nail->thumb_row++;
      thumb_y1 = thumb_y0 + 1;
    }
  else
    nail->thumb_row = 0;

  /* grab screen portion */
  pixbuf = pixbuf_from_drawable (drawable,
				 thumb_x0 * xscale, thumb_y0 * yscale,
				 (thumb_x1 - thumb_x0) * xscale,
				 (thumb_y1 - thumb_y0) * yscale);
  if (!pixbuf)
    return FALSE;
  
  /* compute thumb nail pixels from tiles */
  dest_pixels = gdk_pixbuf_get_pixels (nail->pixbuf);
  drowstride = gdk_pixbuf_get_rowstride (nail->pixbuf);
  src_pixels = gdk_pixbuf_get_pixels (pixbuf);
  srowstride = gdk_pixbuf_get_rowstride (pixbuf);
#if 1
  for (yy = thumb_y0; yy < thumb_y1; yy++)
    {
      guint8 *dest_row = dest_pixels + drowstride * yy + thumb_x0 * 3;
      
      for (xx = thumb_x0; xx < thumb_x1; xx++)
	{
	  gint x0 = (xx - thumb_x0) * xscale, x1 = x0 + xscale;
	  gint y0 = (yy - thumb_y0) * yscale, y1 = y0 + yscale;
	  gint i = x1 - x0, j = y1 - y0, n = i * j;
	  guint8 *src_row = src_pixels + y0 * srowstride + x0 * 3;
	  guint red = 0, green = 0, blue = 0;

	  do
	    {
	      guint t = i;
	      guint8 *s = src_row;
	      
	      do
		{
		  red += *s++;
		  green += *s++;
		  blue += *s++;
		}
	      while (--t);
	      src_row += srowstride;
	    }
	  while (--j);

	  *dest_row++ = red / n;
	  *dest_row++ = green / n;
	  *dest_row++ = blue / n;
	}
    }
#else
  gdk_pixbuf_scale (pixbuf, nail->pixbuf,
		    thumb_x0, thumb_y0,
		    thumb_x1 - thumb_x0, thumb_y1 - thumb_y0,
		    0, 0,
		    1.0 / xscale, 1.0 / yscale,
		    GDK_INTERP_TILES);
#endif
  gdk_pixbuf_unref (pixbuf);

  return TRUE;
}


/* --- Gdk & GdkPixbuf compat functions --- */
#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>
#ifdef	USE_SHM
#include <X11/extensions/XShm.h>
#include <sys/types.h>
#include <sys/shm.h>
#endif	/* USE_SHM */

GdkImage*
gdk_image_new_shared_with_pixmap (GdkWindow  *window,
				  gint        width,
				  gint        height,
				  GdkPixmap **pixmap_p)
{
#ifdef	USE_SHM
  GdkImage *image;
  GdkPixmap *pixmap;
  GdkWindowPrivate *private;
  GdkWindowPrivate *window_private;
  GdkImagePrivate *img_private;
  GdkVisual *visual;
  XShmSegmentInfo *x_shm_info;
  gint depth = -1;
  gint saved_gdk_use_xshm;

  g_return_val_if_fail (width != 0 && height != 0, NULL);
  g_return_val_if_fail (pixmap_p != NULL, NULL);

  if (!window)
    window = (GdkWindow*) &gdk_root_parent;
  window_private = (GdkWindowPrivate*) window;
  visual = gdk_window_get_visual (window);
  if (window_private->destroyed || !visual)
    return NULL;

  saved_gdk_use_xshm = gdk_use_xshm;
  image = gdk_image_new (GDK_IMAGE_SHARED, visual, width, height);
  gdk_use_xshm = saved_gdk_use_xshm;
  if (!image)
    return NULL;
  img_private = (GdkImagePrivate*) image;
  x_shm_info = img_private->x_shm_info;

  if (depth == -1)
    depth = visual->depth;

  private = g_new0 (GdkWindowPrivate, 1);
  pixmap = (GdkPixmap*) private;
  private->xdisplay = window_private->xdisplay;
  private->window_type = GDK_WINDOW_PIXMAP;
  gdk_error_trap_push ();
  private->xwindow = XShmCreatePixmap (private->xdisplay, window_private->xwindow,
				       x_shm_info->shmaddr,
				       x_shm_info,
				       width, height, depth);
  XSync (private->xdisplay, False);
  gdk_error_trap_pop ();
  if (!private->xwindow)
    {
      g_free (private);
      gdk_image_destroy (image);
      return NULL;
    }

  private->colormap = NULL;
  private->parent = NULL;
  private->x = 0;
  private->y = 0;
  private->width = width;
  private->height = height;
  private->resize_count = 0;
  private->ref_count = 1;
  private->destroyed = 0;

  gdk_xid_table_insert (&private->xwindow, pixmap);

  *pixmap_p = pixmap;

  return image;

#else	/* !USE_SHM */
  return NULL;
#endif	/* !USE_SHM */
}

/* fixed gdk_image_get(), so we don't crash if we get an X error
 */
GdkImage*
gdk_image_get (GdkWindow *window,
	       gint       x,
	       gint       y,
	       gint       width,
	       gint       height)
{
  static gpointer image_put_normal = NULL;
  GdkImage *image;
  GdkImagePrivate *private;
  GdkWindowPrivate *win_private;
  GdkVisual *visual;

  g_return_val_if_fail (window != NULL, NULL);

  if (!image_put_normal)
    {
      image = gdk_image_new (GDK_IMAGE_NORMAL, gdk_rgb_get_visual (), 1, 1);
      image_put_normal = ((GdkImagePrivate*) image)->image_put;
      gdk_image_destroy (image);
    }

  win_private = (GdkWindowPrivate *) window;
  visual = gdk_window_get_visual (window);
  if (win_private->destroyed || width < 1 || height < 1) /* || !visual) */
    return NULL;

  private = g_new (GdkImagePrivate, 1);
  image = (GdkImage*) private;

  private->xdisplay = gdk_display;
  private->image_put = image_put_normal;
  gdk_error_trap_push ();
  private->ximage = XGetImage (private->xdisplay,
			       win_private->xwindow,
			       x, y, width, height,
			       AllPlanes, ZPixmap);
  if (gdk_error_trap_pop ())
    {
      g_free (private);
      return NULL;
    }
  image->type = GDK_IMAGE_NORMAL;
  image->visual = visual;
  image->width = width;
  image->height = height;
  image->depth = private->ximage->depth;

  image->mem = private->ximage->data;
  image->bpl = private->ximage->bytes_per_line;
  image->bpp = private->ximage->bits_per_pixel;
  image->byte_order = private->ximage->byte_order;

  return image;
}

/* instead of bailing on NULL, return the root window's colormap,
 * also, guard against XErrors
 */
GdkColormap*
gdk_window_get_colormap (GdkWindow *window)
{
  GdkWindowPrivate *window_private;
  XWindowAttributes window_attributes = { 0, };

  if (!window)
    window = GDK_ROOT_PARENT ();
  window_private = (GdkWindowPrivate*) window;

  g_return_val_if_fail (window_private->window_type != GDK_WINDOW_PIXMAP, NULL);
  if (!window_private->destroyed)
    {
      if (window_private->colormap == NULL)
	{
	  gdk_error_trap_push ();
	  XGetWindowAttributes (window_private->xdisplay,
				window_private->xwindow,
				&window_attributes);
	  return gdk_error_trap_pop () ? NULL : gdk_colormap_lookup (window_attributes.colormap);
	}
      else
	return window_private->colormap;
    }

  return NULL;
}

/* instead of bailing on NULL, return the root window's visual, also
 * guard against XErrors
 */
GdkVisual*
gdk_window_get_visual (GdkWindow *window)
{
  GdkWindowPrivate *window_private;
  XWindowAttributes window_attributes = { 0, };

  if (!window)
    window = GDK_ROOT_PARENT ();
  window_private = (GdkWindowPrivate*) window;

  /* Huh? ->parent is never set for a pixmap. We should just return
   * null immeditately
   */
  while (window_private && (window_private->window_type == GDK_WINDOW_PIXMAP))
    window_private = (GdkWindowPrivate*) window_private->parent;

  if (window_private && !window_private->destroyed)
    {
      if (window_private->colormap == NULL)
	{
	  gdk_error_trap_push ();
	  XGetWindowAttributes (window_private->xdisplay,
				window_private->xwindow,
				&window_attributes);
	  return gdk_error_trap_pop () ? NULL : gdk_visual_lookup (window_attributes.visual);
	}
      else
	return ((GdkColormapPrivate *) window_private->colormap)->visual;
    }

  return NULL;
}

/* self hacked gdk_pixbuf_from_image(), lacking in gdk-pixbuf.
 * mostly ripped from the gdk_pixbuf_get_from_drawable() implementation
 */
static void
_pixbuf_convert_real_slow (GdkImage    *image,
			   guint8      *pixels,
			   gint         rowstride,
			   gboolean     has_alpha,
			   gint         x_off,
			   gint         y_off,
			   gint         width,
			   gint         height,
			   GdkColormap *cmap)
{
  guint8 *orow = pixels;
  gint xx, yy;
  guint32 pixel;
  GdkVisual *visual;
  guint8 component;
  gint i;
  
  visual = gdk_colormap_get_visual (cmap);
  
  width += x_off;
  height += y_off;
  for (yy = y_off; yy < height; yy++)
    {
      guint8 *o = orow;

      for (xx = x_off; xx < width; xx++)
	{
	  pixel = gdk_image_get_pixel (image, xx, yy);
	  switch (visual->type)
	    {
	      /* I assume this is right for static & greyscale's too? */
	    case GDK_VISUAL_STATIC_GRAY:
	    case GDK_VISUAL_GRAYSCALE:
	    case GDK_VISUAL_STATIC_COLOR:
	    case GDK_VISUAL_PSEUDO_COLOR:
	      *o++ = cmap->colors[pixel].red;
	      *o++ = cmap->colors[pixel].green;
	      *o++ = cmap->colors[pixel].blue;
	      break;
	    case GDK_VISUAL_TRUE_COLOR:
	      /* This is odd because it must sometimes shift left (otherwise
	       * I'd just shift >> (*_shift - 8 + *_prec + <0-7>). This logic
	       * should work for all bit sizes/shifts/etc.
	       */
	      component = 0;
	      for (i = 24; i < 32; i += visual->red_prec)
		component |= ((pixel & visual->red_mask) << (32 - visual->red_shift - visual->red_prec)) >> i;
	      *o++ = component;
	      component = 0;
	      for (i = 24; i < 32; i += visual->green_prec)
		component |= ((pixel & visual->green_mask) << (32 - visual->green_shift - visual->green_prec)) >> i;
	      *o++ = component;
	      component = 0;
	      for (i = 24; i < 32; i += visual->blue_prec)
		component |= ((pixel & visual->blue_mask) << (32 - visual->blue_shift - visual->blue_prec)) >> i;
	      *o++ = component;
	      break;
	    case GDK_VISUAL_DIRECT_COLOR:
	      *o++ = cmap->colors[((pixel & visual->red_mask) << (32 - visual->red_shift - visual->red_prec)) >> 24].red;
	      *o++ = cmap->colors[((pixel & visual->green_mask) << (32 - visual->green_shift - visual->green_prec)) >> 24].green;
	      *o++ = cmap->colors[((pixel & visual->blue_mask) << (32 - visual->blue_shift - visual->blue_prec)) >> 24].blue;
	      break;
	    }
	  if (has_alpha)
	    *o++ = 0xff;
	}
      orow += rowstride;
    }
}

static void
_pixbuf_rgbconvert (GdkImage    *image,
		    guint8      *pixels,
		    gint         rowstride,
		    gboolean     has_alpha,
		    gint	 x,
		    gint	 y,
		    gint	 width,
		    gint	 height,
		    GdkColormap *cmap)
{
  gint bank = 5;  /* default fallback converter */
  GdkVisual *visual = gdk_colormap_get_visual (cmap);
  
  switch (visual->type)
    {
      /* I assume this is right for static & greyscale's too? */
    case GDK_VISUAL_STATIC_GRAY:
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_STATIC_COLOR:
    case GDK_VISUAL_PSEUDO_COLOR:
      switch (image->bpp)
	{
	case 1:
	  bank = 0;
	  break;
	case 8:
	  bank = 1;
	  break;
	}
      break;
    case GDK_VISUAL_TRUE_COLOR:
      switch (image->depth)
	{
	case 15:
	  if (visual->red_mask == 0x7c00 && visual->green_mask == 0x3e0 && visual->blue_mask == 0x1f
	      && image->bpp == 16)
	    bank = 2;
	  break;
	case 16:
	  if (visual->red_mask == 0xf800 && visual->green_mask == 0x7e0 && visual->blue_mask == 0x1f
	      && image->bpp == 16)
	    bank = 3;
	  break;
	case 24:
	case 32:
	  if (visual->red_mask == 0xff0000 && visual->green_mask == 0xff00 && visual->blue_mask == 0xff
	      && image->bpp == 32)
	    bank = 4;
	  break;
	}
      break;
    case GDK_VISUAL_DIRECT_COLOR:
      /* always use the slow version */
      break;
    }
  
  _pixbuf_convert_real_slow (image, pixels, rowstride, has_alpha,
			     x, y, width, height, cmap);
}

void
gdk_pixbuf_copy_from_image (GdkPixbuf   *pixbuf,
			    guint	 dest_x,
			    guint        dest_y,
			    GdkImage    *image,
			    guint        src_x,
			    guint        src_y,
			    guint        width,
			    guint        height,
			    GdkColormap *cmap)
{
  gint rowstride;
  guint8 *pixels;
  
  g_return_if_fail (pixbuf != NULL);
  g_return_if_fail (image != NULL);
  g_return_if_fail (cmap != NULL);
  
  /* TODO: check coords and that pixbuf is RGB !alpha */
  
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  
  /* offset into pixbuf */
  pixels += dest_y * rowstride;
  pixels += dest_x * 3;
  
  _pixbuf_rgbconvert (image, pixels, rowstride,
		      gdk_pixbuf_get_has_alpha (pixbuf),
		      src_x, src_y, width, height,
		      cmap);
}

GdkPixbuf*
gdk_pixbuf_from_image (GdkImage    *image,
		       guint        x,
		       guint        y,
		       guint        width,
		       guint        height,
		       GdkColormap *cmap)
{
  GdkPixbuf *pixbuf;
  
  g_return_val_if_fail (image != NULL, NULL);
  
  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
  
  _pixbuf_rgbconvert (image,
		      gdk_pixbuf_get_pixels (pixbuf),
		      gdk_pixbuf_get_rowstride (pixbuf),
		      gdk_pixbuf_get_has_alpha (pixbuf),
		      x, y,
		      gdk_pixbuf_get_width (pixbuf),
		      gdk_pixbuf_get_height (pixbuf),
		      cmap);
  
  return pixbuf;
}
