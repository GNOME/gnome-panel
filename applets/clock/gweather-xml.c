/* gweather-xml.c - Locations.xml parsing code
 *
 * Copyright (C) 2005 Ryan Lortie, 2004 Gareth Owen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


/* There is very little error checking in the parsing code below, it relies 
 * heavily on the locations file being in the correct format.  If you have
 * <name> elements within a parent element, they must come first and be
 * grouped together.
 * The format is as follows: 
 * 
 * <gweather format="1.0">
 * <region>
 *  <name>Name of the region</name>
 *  <name xml:lang="xx">Translated Name</name>
 *  <name xml:lang="zz">Another Translated Name</name>
 *  <country>
 *   <name>Name of the country</name>
 *   <location>
 *    <name>Name of the location</name>
 *    <code>IWIN code</code>
 *    <zone>Forecast code (North America, Australia, UK only)</zone>
 *    <radar>Weather.com radar map code (North America only)</radar>
 *    <coordinates>Latitude and longitude as DD-MM[-SS][H] pair</coordinates>
 *   </location>
 *   <state>
 *     <location>
 *       ....
 *     </location>
 *     <city>
 *      <name>Name of city with multiple locations</city>
 *      <zone>Forecast code</zone>
 *      <radar>Radar Map code</radar>
 *      <location>
 *        ...
 *      </location>
 *     </city>
 *   </state>
 *  </country>
 * </region>
 * <gweather>
 *
 * The thing to note is that each country can either contain different locations
 * or be split into "states" which in turn contain a list of locations.
 *
 */

#include <string.h>
#include <math.h>
#include <locale.h>
#include <gtk/gtk.h>
#include <libxml/xmlreader.h>

#include "gweather-xml.h"

static gint
gweather_xml_location_sort_func( GtkTreeModel *model, GtkTreeIter *a,
                                 GtkTreeIter *b, gpointer user_data )
{
    gint res;
    gchar *name_a, *name_b;
    gchar *fold_a, *fold_b;
        
    gtk_tree_model_get (model, a, GWEATHER_XML_COL_LOC, &name_a, -1);
    gtk_tree_model_get (model, b, GWEATHER_XML_COL_LOC, &name_b, -1);
        
    fold_a = g_utf8_casefold(name_a, -1);
    fold_b = g_utf8_casefold(name_b, -1);
        
    res = g_utf8_collate(fold_a, fold_b);
    
    g_free(name_a);
    g_free(name_b);
    g_free(fold_a);
    g_free(fold_b);
    
    return res;
}
 
static char*
gweather_xml_get_value( xmlTextReaderPtr xml )
{
  char* value;

  /* check for null node */
  if ( xmlTextReaderIsEmptyElement( xml ) )
    return NULL;
    
  /* the next "node" is the text node containing the value we want to get */
  if( xmlTextReaderRead( xml ) != 1 )
    return NULL;

  value = (char *) xmlTextReaderValue( xml );

  /* move on to the end of this node */
  while( xmlTextReaderNodeType( xml ) != XML_READER_TYPE_END_ELEMENT )
    if( xmlTextReaderRead( xml ) != 1 )
    {
      xmlFree( value );
      return NULL;
    }

  /* consume the end element too */
  if( xmlTextReaderRead( xml ) != 1 )
  {
    xmlFree( value );
    return NULL;
  }
    
  return value;
}

static char *
gweather_xml_parse_name( xmlTextReaderPtr xml )
{
  const char * const *locales;
  const char *this_language;
  int best_match = INT_MAX;
  char *lang, *tagname;
  gboolean keep_going;
  char *name = NULL;
  int i;

  locales = g_get_language_names();

  do
  {
    /* First let's get the language */
    lang = (char *) xmlTextReaderXmlLang( xml );

    if( lang == NULL )
      this_language = "C";
    else
      this_language = lang;

    /* the next "node" is text node containing the actual name */
    if( xmlTextReaderRead( xml ) != 1 )
    {
      xmlFree( lang );
      return NULL;
    }

    for( i = 0; locales[i] && i < best_match; i++ )
      if( !strcmp( locales[i], this_language ) )
      {
        /* if we've already encounted a less accurate
           translation, then free it */
        xmlFree( name );

        name = (char *) xmlTextReaderValue( xml );
        best_match = i;

        break;
      }

    xmlFree( lang );

    while( xmlTextReaderNodeType( xml ) != XML_READER_TYPE_ELEMENT )
      if( xmlTextReaderRead( xml ) != 1 )
      {
        xmlFree( name );
        return NULL;
      }

    /* if the next tag is another <name> then keep going */
    tagname = (char *) xmlTextReaderName( xml );
    keep_going = !strcmp( tagname, "name" );
    xmlFree( tagname );

  } while( keep_going );

  return name;
}

static int
gweather_xml_parse_node (GtkTreeStore *store, GtkTreeIter *parent,
                         xmlTextReaderPtr xml, 
                         const char *dflt_radar, const char *dflt_zone,
                         const char *cityname)
{
  char *name, *code, *zone, *radar, *coordinates;
  char **city, *nocity = NULL;
  GtkTreeIter iter, *self;
  gboolean is_location;
  char *tagname;
  int ret = -1;
  int tagtype;

  if( (tagname = (char *) xmlTextReaderName( xml )) == NULL )
    return -1;

  if( !strcmp( tagname, "city" ) )
    city = &name;
  else
    city = &nocity;

  is_location = !strcmp( tagname, "location" );

  /* if we're processing the top-level, then don't create a new iter */
  if( !strcmp( tagname, "gweather" ) )
    self = NULL;
  else
  {
    self = &iter;
    /* insert this node into the tree */
    gtk_tree_store_append( store, self, parent );
  }

  xmlFree( tagname );

  coordinates = NULL;
  radar = NULL;
  zone = NULL;
  code = NULL;
  name = NULL;

  /* absorb the start tag */
  if( xmlTextReaderRead( xml ) != 1 )
    goto error_out;

  /* start parsing the actual contents of the node */
  while( (tagtype = xmlTextReaderNodeType( xml )) !=
         XML_READER_TYPE_END_ELEMENT )
  {

    /* skip non-element types */
    if( tagtype != XML_READER_TYPE_ELEMENT )
    {
      if( xmlTextReaderRead( xml ) != 1 )
        goto error_out;

      continue;
    }
    
    tagname = (char *) xmlTextReaderName( xml );

    if( !strcmp( tagname, "region" ) || !strcmp( tagname, "country" ) ||
        !strcmp( tagname, "state" ) || !strcmp( tagname, "city" ) ||
        !strcmp( tagname, "location" ) )
    {
      /* recursively handle sub-sections */
      if( gweather_xml_parse_node( store, self, xml, 
                                   radar, zone, *city ) )
        goto error_out;
    }
    else if ( !strcmp( tagname, "name" ) )
    {
      xmlFree( name );
      if( (name = gweather_xml_parse_name( xml )) == NULL )
        goto error_out;
    }
    else if ( !strcmp( tagname, "code" ) )
    {
      xmlFree( code );
      if( (code = gweather_xml_get_value( xml )) == NULL )
        goto error_out;
    }
    else if ( !strcmp( tagname, "zone" ) )
    {
      xmlFree( zone );
      if( (zone = gweather_xml_get_value( xml )) == NULL )
        goto error_out;
    }
    else if ( !strcmp( tagname, "radar" ) )
    {
      xmlFree( radar );
      if( (radar = gweather_xml_get_value( xml )) == NULL )
        goto error_out;
    }
    else if ( !strcmp( tagname, "coordinates" ) )
    {
      xmlFree( coordinates );
      if( (coordinates = gweather_xml_get_value( xml )) == NULL )
        goto error_out;
    }
    else /* some strange tag */
    {
      /* skip past it */
      if( xmlTextReaderRead( xml ) != 1 )
        goto error_out;
    }

    xmlFree( tagname );
  }

  if( self )
    gtk_tree_store_set( store, self, GWEATHER_XML_COL_LOC, name, -1 );

  /* absorb the end tag.  in the case of processing a <gweather> then 'self'
     is NULL.  In this case, we let this fail since we might be at EOF */
  if( xmlTextReaderRead( xml ) != 1 && self )
    goto error_out;

  /* if this is an actual location, setup the WeatherLocation for it */
  if( is_location )
  {
    WeatherLocation *new_loc;

    if( cityname == NULL )
      cityname = name;

    if( radar != NULL )
      dflt_radar = radar;

    if( zone != NULL )
      dflt_zone = zone;

    new_loc =  weather_location_new( cityname, code, dflt_zone,
                                     dflt_radar, coordinates );

    gtk_tree_store_set( store, &iter, GWEATHER_XML_COL_POINTER, new_loc, -1 );
  }

  ret = 0;

error_out:
  xmlFree( name );
  xmlFree( code );
  xmlFree( zone );
  xmlFree( radar );
  xmlFree( coordinates );

  return ret;
}

/*****************************************************************************
 * Func:    gweather_xml_load_locations()
 * Desc:    Main entry point for loading the locations from the XML file
 * Parm:
 *      *tree:      tree to view locations
 *      *current:   currently selected location
 */
int
gweather_xml_load_locations( GtkTreeView *tree)
{
  char *tagname, *format;
  GtkTreeSortable *sortable;
  GtkTreeStore *store;
  xmlTextReaderPtr xml;
  int keep_going;
  int ret = -1;

  /* Open the xml file containing the different locations */
#ifdef GWEATHER_XML_LOCATION
  xml = xmlNewTextReaderFilename (GWEATHER_XML_LOCATION);
#else
  xml = xmlNewTextReaderFilename ("/usr/share/gnome-applets/gweather/Locations.xml");
#endif
  if( xml == NULL )
    goto error_out;

  /* fast forward to the first element */
  do
  {
    /* if we encounter a problem here, exit right away */
    if( xmlTextReaderRead( xml ) != 1 )
      goto error_out;
  } while( xmlTextReaderNodeType( xml ) != XML_READER_TYPE_ELEMENT );

  /* check the name and format */
  tagname = (char *) xmlTextReaderName( xml );
  keep_going = tagname && !strcmp( tagname, "gweather" );
  xmlFree( tagname );

  if( !keep_going )
    goto error_out;

  format = (char *) xmlTextReaderGetAttribute( xml, (xmlChar *) "format" );
  keep_going = format && !strcmp( format, "1.0" );
  xmlFree( format );

  if( !keep_going )
    goto error_out;

  store = GTK_TREE_STORE (gtk_tree_view_get_model (tree));
  /* temporarily disconnect the model to speed things up */
  g_object_ref (store);
  gtk_tree_view_set_model (tree, NULL);

  ret = gweather_xml_parse_node( store, NULL, xml, NULL, NULL, NULL );

  gtk_tree_view_set_model (tree, GTK_TREE_MODEL (store));
  g_object_unref (store);

  if( ret )
    goto error_out;

  /* Sort the tree */
  sortable = GTK_TREE_SORTABLE (gtk_tree_view_get_model( tree ));
  gtk_tree_sortable_set_default_sort_func( sortable,
                                           &gweather_xml_location_sort_func,
                                           NULL, NULL);
  gtk_tree_sortable_set_sort_column_id( sortable, 
                                     GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
                                        GTK_SORT_ASCENDING );
error_out:
  xmlFreeTextReader( xml );

  return ret;
}

typedef struct {
  const gchar *name;
  gdouble latitude;
  gdouble longitude;
  gdouble distance;
  WeatherLocation *location;
} SearchData;
  

static gdouble
distance (gdouble lat1, gdouble lon1, 
          gdouble lat2, gdouble lon2)
{
  gdouble radius = 6372.795;

  return acos (cos (lat1) * cos (lat2) * cos (lon1 - lon2) + sin (lat1) * sin (lat2)) * radius;
}

static gboolean
compare_location (GtkTreeModel *model,
                  GtkTreePath  *path,
                  GtkTreeIter  *iter,
                  gpointer      user_data)
{
  SearchData *data = user_data;
  WeatherLocation *loc;
  gdouble d;
   
  gtk_tree_model_get (model, iter, GWEATHER_XML_COL_POINTER, &loc, -1);

  if (!loc)
    return FALSE;

  d = distance (data->latitude, data->longitude, loc->latitude, loc->longitude);

  if (d < data->distance) {
    data->distance = d;
    data->location = loc;
  }

  return FALSE;
}

gchar *
find_weather_code (GtkTreeModel *model,
		   const gchar  *name,
                   gdouble       lat, 
                   gdouble       lon)
{
  SearchData data;
  gchar *code;

  data.name = name;
  data.latitude = lat;
  data.longitude = lon;
  data.distance = 1e6;
  data.location = NULL;

  gtk_tree_model_foreach (GTK_TREE_MODEL (model), compare_location, &data);

  if (data.distance < 50)
    code = g_strdup (data.location->code);
  else
    code = g_strdup ("-");

  return code;
}
