/**
 * Copyright © 2010 Canonical Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the licence, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 **/

#include <dconf-dbus-1.h>

#include <stdbool.h>
#include <unistd.h>
#include <string.h>

static DConfDBusClient *backend;

static void
free_variant (gpointer data)
{
  if (data != NULL)
    g_variant_unref (data);
}

static GVariant *
do_read (const gchar *key)
{
  return dconf_dbus_client_read (backend, key);
}

static gboolean
do_write (const gchar *key,
          GVariant    *value)
{
  return dconf_dbus_client_write (backend, key, value);
}

static gboolean
do_write_tree (GTree *tree)
{
  g_assert_not_reached ();
}

static void
do_sync (void)
{
/*  g_assert_not_reached (); */
}

#define RANDOM_ELEMENT(array) \
  array[g_test_rand_int_range(0, G_N_ELEMENTS(array))]

static gchar *
random_key (void)
{
  const gchar * const words[] = {
    "alpha", "bravo", "charlie", "delta", "echo", "foxtrot", "golf",
    "hotel", "india", "juliet", "kilo", "lima", "mike", "november",
    "oscar", "papa", "quebec", "romeo", "sierra", "tango", "uniform",
    "victor", "whiskey", "xray", "yankee", "zulu"
  };
  const gchar *parts[8];
  gint n, i;

  n = g_test_rand_int_range (2, 8);
  parts[0] = "";
  for (i = 1; i < n; i++)
    parts[i] = RANDOM_ELEMENT (words);
  parts[n] = NULL;

  return g_strjoinv ("/", (gchar **) parts);
}

static GVariant *
random_value (void)
{
  switch (g_test_rand_int_range (0, 3))
    {
    case 0:
      return g_variant_new_int32 (g_test_rand_int ());

    case 1:
      return g_variant_new_boolean (g_test_rand_bit ());

    case 2:
      {
        gint length = g_test_rand_int_range (0, 24);
        gchar buffer[24];
        gint i;

        for (i = 0; i < length; i++)
          buffer[i] = 'a' + g_test_rand_int_range (0, 26);
        buffer[i] = '\0';

        return g_variant_new_string (buffer);
      }

    default:
      g_assert_not_reached ();
    }
}

static GTree *
random_tree (void)
{
  GTree *tree;
  gint n;

  tree = g_tree_new_full ((GCompareDataFunc) strcmp, NULL,
                          g_free, free_variant);
  n = g_test_rand_int_range (1, 20);

  while (n--)
    g_tree_insert (tree, random_key (), g_variant_ref_sink (random_value ()));

  return tree;
}

static void
apply_change (GHashTable  *table,
              const gchar *key,
              GVariant    *value)
{
  if (value)
    g_hash_table_insert (table, g_strdup (key), g_variant_ref_sink (value));
  else
    g_hash_table_insert (table, g_strdup (key), NULL);
}

static gboolean
apply_one_change (gpointer key,
                  gpointer value,
                  gpointer user_data)
{
  apply_change (user_data, key, value);
  return FALSE;
}

static void
apply_change_tree (GHashTable *table,
                   GTree      *tree)
{
  g_tree_foreach (tree, apply_one_change, table);
}

static GHashTable *implicit;
static GHashTable *explicit;

static void
watch_func (DConfDBusClient *client,
            const gchar     *key,
            gpointer         user_data)
{
  GVariant *value;

  /* ensure that we see no dupes from the bus */
/*  g_assert (origin_tag == do_write); */
  g_assert (client == backend);

  value = do_read (key);
  apply_change (implicit, key, value);
  g_variant_unref (value);
}

static void
setup (void)
{
  gchar *file;

  file = g_build_filename (g_get_user_config_dir (),
                           "dconf/test", NULL);
  unlink (file);
  g_free (file);

  g_setenv ("DCONF_PROFILE", "test", false);

  backend = dconf_dbus_client_new ("test", NULL, NULL);
  dconf_dbus_client_subscribe (backend, "/", watch_func, NULL);

  implicit = g_hash_table_new_full (g_str_hash, g_str_equal,
                                    g_free, free_variant);
  explicit = g_hash_table_new_full (g_str_hash, g_str_equal,
                                    g_free, free_variant);

  sleep(1);
}

static void
make_random_change (void)
{
  if (1)
    {
      GVariant *value;
      gchar *key;

      key = random_key ();
      value = random_value ();
      apply_change (explicit, key, value);
      do_write (key, value);

      g_free (key);
    }
  else
    {
      GTree *tree;

      tree = random_tree ();
      apply_change_tree (explicit, tree);
      do_write_tree (tree);

      g_tree_unref (tree);
    }
}

guint64 dconf_time;
guint64 ghash_time;
guint64 lookups;
gboolean dots;

static void
verify_consistency (void)
{
  GHashTableIter iter;
  gpointer key, value;

  if (dots)
    g_print (".");
  else
    g_print ("(%d)", g_hash_table_size (explicit));

  g_assert (g_hash_table_size (explicit) == g_hash_table_size (implicit));
  g_hash_table_iter_init (&iter, explicit);
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      if (value)
        {
          GVariant *other;

          ghash_time -= g_get_monotonic_time ();
          other = g_hash_table_lookup (implicit, key);
          ghash_time += g_get_monotonic_time ();
          g_assert (g_variant_equal (value, other));


          dconf_time -= g_get_monotonic_time ();
          other = do_read (key);
          dconf_time += g_get_monotonic_time ();
          g_assert (g_variant_equal (value, other));
          g_variant_unref (other);
        }
      else
        {
          g_assert (g_hash_table_lookup (implicit, key) == NULL);
          g_assert (do_read (key) == NULL);
        }

      lookups++;
    }
}

#if 0
static void
dump_table (void)
{
  GHashTableIter iter;
  gpointer key, value;

  g_print ("{");
  g_hash_table_iter_init (&iter, explicit);
  while (g_hash_table_iter_next (&iter, &key, &value))
    if (value)
      {
        gchar *printed;

        if (value)
          printed = g_variant_print (value, FALSE);
        else
          printed = g_strdup ("None");

        g_print ("'%s': %s, ", (gchar *) key, printed);
        g_free (printed);
      }
  g_print ("}");
}
#endif

static void
test (void)
{
  int i;

  g_print ("Testing dconf...");
  for (i = 0; i < 1000; i++)
    {
      g_print (" %d", i);
      make_random_change ();
      verify_consistency ();
    }

  g_print ("\n");
  g_print ("GSettings lookup time:  %f µs/lookup\n",
           ((double) dconf_time / lookups));
  g_print ("GHashTable lookup time: %f µs/lookup\n",
           ((double) ghash_time / lookups));

  dconf_time = 0;
  ghash_time = 0;
  lookups = 0;

  g_print ("\nWaiting for dconf-service to catch up...");
  do_sync ();
  g_print (" done.\n");

  g_print ("Measuring dconf read performance...");
  dots = TRUE;
  for (i = 0; i < 1000; i++)
    verify_consistency ();
  g_print ("\n");

  g_print ("dconf lookup time:      %f µs/lookup\n",
           ((double) dconf_time / lookups));
  g_print ("GHashTable lookup time: %f µs/lookup\n",
           ((double) ghash_time / lookups));

  g_hash_table_unref (explicit);
  g_hash_table_unref (implicit);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);

  setup ();

  test ();

  return g_test_run ();
}
