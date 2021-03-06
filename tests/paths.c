#include <dconf-paths.h>

static void
test_paths (void)
{
  struct test_case {
    const gchar *string;
    guint flags;
  } cases[] = {

#define invalid 0
#define path    001
#define key     002 | path
#define dir     004 | path
#define rel     010
#define relkey  020 | rel
#define reldir  040 | rel

    { "",               reldir  },
    { "/",              dir     },

    { "/key",           key     },
    { "/path/",         dir     },
    { "/path/key",      key     },
    { "/path/path/",    dir     },
    { "/a/b",           key     },
    { "/a/b/",          dir     },

    { "//key",          invalid },
    { "//path/",        invalid },
    { "//path/key",     invalid },
    { "//path/path/",   invalid },
    { "//a/b",          invalid },
    { "//a/b/",         invalid },

    { "/key",           key     },
    { "/path//",        invalid },
    { "/path/key",      key     },
    { "/path/path//",   invalid },
    { "/a/b",           key     },
    { "/a/b//",         invalid },

    { "/key",           key     },
    { "/path/",         dir     },
    { "/path//key",     invalid },
    { "/path//path/",   invalid },
    { "/a//b",          invalid },
    { "/a//b/",         invalid },

    { "key",            relkey  },
    { "path/",          reldir  },
    { "path/key",       relkey  },
    { "path/path/",     reldir  },
    { "a/b",            relkey  },
    { "a/b/",           reldir  },

    { "key",            relkey  },
    { "path//",         invalid },
    { "path/key",       relkey  },
    { "path/path//",    invalid },
    { "a/b",            relkey  },
    { "a/b//",          invalid },

    { "key",            relkey  },
    { "path/",          reldir  },
    { "path//key",      invalid },
    { "path//path/",    invalid },
    { "a//b",           invalid },
    { "a//b/",          invalid }
  };
  gint i;

  for (i = 0; i < G_N_ELEMENTS (cases); i++)
    {
      const gchar *string = cases[i].string;
      guint flags;

      flags = (dconf_is_path    (string, NULL) ? 001 : 000) |
              (dconf_is_key     (string, NULL) ? 002 : 000) |
              (dconf_is_dir     (string, NULL) ? 004 : 000) |
              (dconf_is_rel     (string, NULL) ? 010 : 000) |
              (dconf_is_rel_key (string, NULL) ? 020 : 000) |
              (dconf_is_rel_dir (string, NULL) ? 040 : 000);

      if (flags != cases[i].flags)
        {
          g_print ("case %i: string '%s' should be %o but is %o",
                   i, string, cases[i].flags, flags);
          g_assert_not_reached ();
        }
    }
}

int
main (void)
{
  test_paths ();

  return 0;
}
