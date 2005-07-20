/********************************************************************\
 * gnc-gconf-utils.c -- utility functions for storing/retrieving    *
 *              data in the GConf database for GnuCash              *
 * Copyright (C) 2005 David Hampton <hampton@employees.org>         *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/

#include "config.h"

#include <stdio.h>
#include <string.h>
#include "gnc-gconf-utils.h"

#define APP_GNUCASH "/apps/gnucash/%s"
#define CLIENT_TAG  "%s-client"
#define NOTIFY_TAG  "%s-notify_id"

static GConfClient *our_client = NULL;

/************************************************************/
/*                      Enum Utilities                      */
/************************************************************/

const gchar *
gnc_enum_to_nick(GType type,
		 gint value)
{
  GEnumClass   	    *enum_class;
  GEnumValue   	    *enum_value;

  /* Lookup the enum in the glib type system */
  enum_class = g_type_class_ref (type);
  if (!enum_class) {
    /* g_type_class_ref has already printed a warning. */
    return NULL;
  }

  enum_value = g_enum_get_value (enum_class, value);
  if (!enum_value) {
    /* Use the first item in the enum */
    enum_value = g_enum_get_value (enum_class, 0);
  }
  return enum_value->value_nick;
}

gint
gnc_enum_from_nick(GType type,
		   const gchar *name,
		   gint default_value)
{
  GEnumClass   *enum_class;
  GEnumValue   *enum_value;
  gchar        *alt_name, *ptr;

  /* Lookup the enum class in the glib type system */
  enum_class = g_type_class_ref (type);
  if (!enum_class) {
    /* g_type_class_ref has already printed a warning. */
    return default_value;
  }

  /* Lookup the specified enum in the class */
  enum_value = g_enum_get_value_by_nick(enum_class, name);
  if (enum_value)
    return enum_value->value;

  /* Flip '-' and '_' and try again */
  alt_name = g_strdup(name);
  if ((ptr = index(alt_name, '-')) != NULL) {
    do {
      *ptr++ = '_';
    } while ((ptr = index(ptr, '-')) != NULL);
  } else  if ((ptr = index(alt_name, '_')) != NULL) {
    do {
      *ptr++ = '-';
    } while ((ptr = index(ptr, '_')) != NULL);
  } else {
    g_free(alt_name);
    return default_value;
  }

  /* Lookup the specified enum in the class */
  enum_value = g_enum_get_value_by_nick(enum_class, alt_name);
  g_free(alt_name);
  if (enum_value)
    return enum_value->value;
  return default_value;
}

/************************************************************/
/*                      Gconf Utilities                     */
/************************************************************/

char *
gnc_gconf_section_name (const char *name)
{
  if (*name == '/') {
    /* Need to return a newly allocated string */
    return g_strdup(name);
  }

  /* This could (should?) be accomplished with a call to
   * gnome_gconf_get_app_settings_relative(), but that would introduce
   * a new library dependancy, even though its not a gui library.  In
   * order to keep this file completely "gnome-free" this approach was
   * used.
   */
  return g_strdup_printf(APP_GNUCASH, name);
}

char *
gnc_gconf_schema_section_name (const char *name)
{
  if (strncmp(name, "/schemas", sizeof("/schemas")) == 0) {
    /* Need to return a newly allocated string */
    return g_strdup(name);
  }

  /* This could (should?) be accomplished with a call to
   * gnome_gconf_get_app_settings_relative(), but that would introduce
   * a new library dependancy, even though its not a gui library.  In
   * order to keep this file completely "gnome-free" this approach was
   * used.
   */
  return g_strdup_printf("/schemas" APP_GNUCASH, name);
}

static gchar *
gnc_gconf_make_key (const gchar *section, const gchar *name)
{
  gchar *section_path, *key;

  g_assert ((section != NULL) || (name != NULL));

  if (section == NULL) {
    if (*name == '/')
      return g_strdup(name);
    return gnc_gconf_section_name(name);
  }
    
  if (name == NULL) {
    if (*section == '/')
      return g_strdup(section);
    return gnc_gconf_section_name(section);
  }

  if (*section == '/') {
    if (*name == '/')
      return g_strjoin(NULL, section, name, NULL);
    return g_strjoin("/", section, name, NULL);
  }

  section_path = gnc_gconf_section_name(section);
  key = g_strjoin("/", section_path, name, NULL);
  g_free(section_path);
  return key;
}


static gchar *
gnc_gconf_make_schema_key (const gchar *section, const gchar *name)
{
  gchar *intermediate, *key;

  g_assert ((section != NULL) || (name != NULL));

  intermediate = gnc_gconf_make_key(section, name);
  key = g_strconcat("/schemas", intermediate, NULL);
  g_free(intermediate);
  return key;
}


gboolean
gnc_gconf_get_bool (const gchar *section,
		    const gchar *name,
		    GError **caller_error)
{
  GError *error = NULL;
  gboolean value;
  gchar *key;

  if (our_client == NULL)
    our_client = gconf_client_get_default();

  key = gnc_gconf_make_key(section, name);
  value = gconf_client_get_bool(our_client, key, &error);
  if (error) {
    if (caller_error) {
      g_propagate_error(caller_error, error);
    } else {
      printf("Failed to load key %s: %s", key, error->message);
      g_error_free(error);
    }
  }
  g_free(key);
  return value;
}

gboolean
gnc_gconf_get_bool_no_error (const gchar *section,
			     const gchar *name)
{
  return gnc_gconf_get_bool(section, name, NULL);
}

void
gnc_gconf_set_bool (const gchar *section,
		    const gchar *name,
		    const gboolean value,
		    GError **caller_error)
{
  GError *error = NULL;
  gchar *key;

  if (our_client == NULL)
    our_client = gconf_client_get_default();

  /* Remember whether the column width */
  key = gnc_gconf_make_key(section, name);
  if (!gconf_client_set_bool(our_client, key, value, &error)) {
    if (caller_error) {
      g_propagate_error(caller_error, error);
    } else {
      printf("Failed to save key %s: %s", key, error->message);
      g_error_free(error);
    }
  }
  g_free(key);
}

gint
gnc_gconf_get_int (const gchar *section,
		   const gchar *name,
		   GError **caller_error)
{
  GError *error = NULL;
  gint value;
  gchar *key;

  if (our_client == NULL)
    our_client = gconf_client_get_default();

  key = gnc_gconf_make_key(section, name);
  value = gconf_client_get_int(our_client, key, &error);
  if (error) {
    if (caller_error) {
      g_propagate_error(caller_error, error);
    } else {
      printf("Failed to load key %s: %s", key, error->message);
      g_error_free(error);
    }
  }
  g_free(key);
  return value;
}

void
gnc_gconf_set_int (const gchar *section,
		   const gchar *name,
		   const gint value,
		   GError **caller_error)
{
  GError *error = NULL;
  gchar *key;

  if (our_client == NULL)
    our_client = gconf_client_get_default();

  /* Remember whether the column width */
  key = gnc_gconf_make_key(section, name);
  if (!gconf_client_set_int(our_client, key, value, &error)) {
    if (caller_error) {
      g_propagate_error(caller_error, error);
    } else {
      printf("Failed to save key %s: %s", key, error->message);
      g_error_free(error);
    }
  }
  g_free(key);
}

gchar *
gnc_gconf_get_string (const gchar *section,
		      const gchar *name,
		      GError **caller_error)
{
  GError *error = NULL;
  gchar *value;
  gchar *key;

  if (our_client == NULL)
    our_client = gconf_client_get_default();

  key = gnc_gconf_make_key(section, name);
  value = gconf_client_get_string(our_client, key, &error);
  if (error) {
    if (caller_error) {
      g_propagate_error(caller_error, error);
    } else {
      printf("Failed to load key %s: %s", key, error->message);
      g_error_free(error);
    }
  }
  g_free(key);
  return value;
}

void
gnc_gconf_set_string (const gchar *section,
		      const gchar *name,
		      const gchar *value,
		      GError **caller_error)
{
  GError *error = NULL;
  gchar *key;

  if (our_client == NULL)
    our_client = gconf_client_get_default();

  key = gnc_gconf_make_key(section, name);
  if (!gconf_client_set_string(our_client, key, value, &error)) {
    if (caller_error) {
      g_propagate_error(caller_error, error);
    } else {
      printf("Failed to save key %s: %s", key, error->message);
      g_error_free(error);
    }
  }
  g_free(key);
}

GSList *
gnc_gconf_get_list (const gchar *section,
		    const gchar *name,
		    GConfValueType list_type,
		    GError **caller_error)
{
  GError *error = NULL;
  GSList *value;
  gchar *key;

  if (our_client == NULL)
    our_client = gconf_client_get_default();

  key = gnc_gconf_make_key(section, name);
  value = gconf_client_get_list(our_client, key, list_type, &error);
  if (error) {
    if (caller_error) {
      g_propagate_error(caller_error, error);
    } else {
      printf("Failed to load key %s: %s", key, error->message);
      g_error_free(error);
    }
  }
  g_free(key);
  return value;
}

void
gnc_gconf_set_list (const gchar *section,
		    const gchar *name,
		    GConfValueType list_type,
		    GSList *value,
		    GError **caller_error)
{
  GError *error = NULL;
  gchar *key;

  if (our_client == NULL)
    our_client = gconf_client_get_default();

  key = gnc_gconf_make_key(section, name);
  if (!gconf_client_set_list(our_client, key, list_type, value, &error)) {
    if (caller_error) {
      g_propagate_error(caller_error, error);
    } else {
      printf("Failed to save key %s: %s", key, error->message);
      g_error_free(error);
    }
  }
  g_free(key);
}

GConfSchema *
gnc_gconf_get_schema (const gchar *section,
		      const gchar *name,
		      GError **caller_error)
{
  GError *error = NULL;
  GConfSchema *value;
  gchar *key;

  if (our_client == NULL)
    our_client = gconf_client_get_default();

  key = gnc_gconf_make_key(section, name);
  value = gconf_client_get_schema(our_client, key, &error);
  if (error) {
    if (caller_error) {
      g_propagate_error(caller_error, error);
    } else {
      printf("Failed to load key %s: %s", key, error->message);
      g_error_free(error);
    }
  }
  g_free(key);
  return value;
}

GSList *
gnc_gconf_client_all_entries (const gchar *name)
{
  GError *error = NULL;
  GSList *value;
  gchar *section;

  if (our_client == NULL)
    our_client = gconf_client_get_default();

  section = gnc_gconf_section_name(name);
  value = gconf_client_all_entries(our_client, section, &error);
  if (error != NULL) {
    printf("Failed to get list of all gconf keys: %s", error->message);
    g_error_free(error);
  }

  return value;
}

void
gnc_gconf_unset (const gchar *section,
		 const gchar *name,
		 GError **caller_error)
{
  GError *error = NULL;
  gchar *key;

  if (our_client == NULL)
    our_client = gconf_client_get_default();

  key = gnc_gconf_make_key(section, name);
  if (!gconf_client_unset(our_client, key, &error)) {
    if (caller_error) {
      g_propagate_error(caller_error, error);
    } else {
      printf("Failed to unset key %s: %s", key, error->message);
      g_error_free(error);
    }
  }
  g_free(key);
}


void
gnc_gconf_unset_dir (const gchar *section,
		     GError **caller_error)
{
  GError *error = NULL;
  GSList *entries, *tmp;
  const gchar *key;
  gchar *dir_key;

  if (our_client == NULL)
    our_client = gconf_client_get_default();

  dir_key = gnc_gconf_make_key(section, NULL);
  entries = gconf_client_all_entries(our_client, dir_key, &error);
  g_free(dir_key);
  if (error) {
    if (caller_error) {
      g_propagate_error(caller_error, error);
    } else {
      printf("Failed to get directory entries for key %s: %s",
	     dir_key, error->message);
      g_error_free(error);
    }
    return;
  }

  for (tmp = entries; tmp; tmp = g_slist_next(tmp)) {
    key = gconf_entry_get_key(tmp->data);
    if (!gconf_client_unset(our_client, key, &error)) {
      if (caller_error) {
	g_propagate_error(caller_error, error);
      } else {
	printf("Failed to unset key %s: %s", key, error->message);
	g_error_free(error);
      }
      break;
    }
  }

  g_slist_foreach(entries, (GFunc)gconf_entry_free, NULL);
  g_slist_free(entries);
}


void
gnc_gconf_suggest_sync (void)
{
  GError *error = NULL;

  if (our_client == NULL)
    our_client = gconf_client_get_default();

  gconf_client_suggest_sync(our_client, &error);
  if (error != NULL) {
    printf("Failed to sync gconf: %s", error->message);
    g_error_free(error);
  }
}


void
gnc_gconf_add_notification (GObject *object,
			    const gchar *section,
			    GConfClientNotifyFunc callback)
{
	GConfClient *client;
	GError *error = NULL;
	gchar *path, *client_tag, *notify_tag;
	guint id;

	g_return_if_fail(G_IS_OBJECT(object));
	g_return_if_fail(section != NULL);
	g_return_if_fail(callback != NULL);

	client = gconf_client_get_default();
	path = gnc_gconf_section_name(section);

	/*
	 * First we have to add the directory...
	 */
	gconf_client_add_dir(client, path, GCONF_CLIENT_PRELOAD_ONELEVEL, &error);
	if (error != NULL) {
	    printf("Failed to add history section to watched directories in gconf: %s", error->message);
	    g_error_free(error);
	    g_object_unref(client);
	    g_free(path);
	    return;
	}

	/*
	 * Then we can add the notification callback.
	 */
	id = gconf_client_notify_add(client, path, callback,
				     object, NULL, &error);
	if (error != NULL) {
	      printf("Failed to set gconf notify for history section: %s", error->message);
	      gconf_client_remove_dir(client, path, NULL);
	      g_error_free(error);
	      g_object_unref(client);
	      g_free(path);
	      return;
	}
	
	/*
	 * Save the values needed to undo this later.
	 */
	client_tag = g_strdup_printf(CLIENT_TAG, section);
	notify_tag = g_strdup_printf(NOTIFY_TAG, section);
	g_object_set_data(object, client_tag, client);
	g_object_set_data(object, notify_tag, GUINT_TO_POINTER(id));
	g_free(notify_tag);
	g_free(client_tag);
	g_free(path);
}


guint
gnc_gconf_add_anon_notification (const gchar *section,
				 GConfClientNotifyFunc callback,
				 gpointer data)
{
	GConfClient *client;
	GError *error = NULL;
	gchar *path;
	guint id;

	g_return_val_if_fail(section != NULL, 0);
	g_return_val_if_fail(callback != NULL, 0);

	client = gconf_client_get_default();
	path = gnc_gconf_section_name(section);


	/*
	 * First we have to add the directory...
	 */
	gconf_client_add_dir(client, path, GCONF_CLIENT_PRELOAD_ONELEVEL, &error);
	if (error != NULL) {
	    printf("Failed to add history section to watched directories in gconf: %s", error->message);
	    g_error_free(error);
	    g_object_unref(client);
	    g_free(path);
	    return 0;
	}

	/*
	 * Then we can add the notification callback.
	 */
	id = gconf_client_notify_add(client, path, callback,
				     data, NULL, &error);
	if (error != NULL) {
	      printf("Failed to set gconf notify for history section: %s", error->message);
	      gconf_client_remove_dir(client, path, NULL);
	      g_error_free(error);
	      g_object_unref(client);
	      g_free(path);
	      return 0;
	}
	g_free(path);
	return id;
}


void
gnc_gconf_remove_notification (GObject *object,
			       const gchar *section)
{
	GConfClient *client;
	gchar *path, *client_tag, *notify_tag;
	guint id;

	g_return_if_fail(G_IS_OBJECT(object));
	g_return_if_fail(section != NULL);

	/*
	 * Remove any gconf notifications
	 */
	client_tag = g_strdup_printf(CLIENT_TAG, section);
	client = g_object_get_data(object, client_tag);
	path = gnc_gconf_section_name(section);
	if (client) {
	  notify_tag = g_strdup_printf(NOTIFY_TAG, section);
	  id = GPOINTER_TO_UINT(g_object_get_data(object, notify_tag));
	  gconf_client_notify_remove(client, id);
	  gconf_client_remove_dir(client, path, NULL);
	  g_object_unref(client);
	  g_free(notify_tag);
	}
	g_free(path);
	g_free(client_tag);
}


void
gnc_gconf_remove_anon_notification (const gchar *section,
				    guint cnxn_id)
{
	GConfClient *client;
	gchar *path;

	g_return_if_fail(section != NULL);

	/*
	 * Remove any gconf notifications
	 */
	path = gnc_gconf_section_name(section);
	client = gconf_client_get_default();
	if (client) {
	  gconf_client_notify_remove(client, cnxn_id);
	  gconf_client_remove_dir(client, path, NULL);
	  g_object_unref(client);
	}
	g_free(path);
}

gboolean
gnc_gconf_schemas_found (void)
{
  GConfSchema* schema;
  GError *err = NULL;
  gboolean found = FALSE;
  gchar *key;

  if (our_client == NULL)
    our_client = gconf_client_get_default();

  key = gnc_gconf_make_schema_key(GCONF_GENERAL_REGISTER, "use_theme_colors");
  schema = gconf_client_get_schema(our_client, key, &err);
  if (schema != NULL) {
    gconf_schema_free(schema);
    found = TRUE;
  }
  g_free(key);
  return found;
}
