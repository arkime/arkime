#include <stdlib.h>
#include <unistd.h>
#include <locale.h>
#include <libintl.h>
#include <gio/gio.h>
#include <gstdio.h>

typedef struct {
  const gchar *name;
  const gchar *opt;
  const gchar *err;
} SchemaTest;

/* Meson build defines this, autotools build does not */
#ifndef GLIB_COMPILE_SCHEMAS
#define GLIB_COMPILE_SCHEMAS "../glib-compile-schemas"
#endif

static void
test_schema_do_compile (gpointer data)
{
  SchemaTest *test = (SchemaTest *) data;
  gchar *filename = g_strconcat (test->name, ".gschema.xml", NULL);
  gchar *path = g_test_build_filename (G_TEST_DIST, "schema-tests", filename, NULL);
  gchar *argv[] = {
    GLIB_COMPILE_SCHEMAS,
    "--strict",
    "--dry-run",
    "--schema-file", path,
    (gchar *)test->opt,
    NULL
  };
  gchar *envp[] = { NULL };

  execve (argv[0], argv, envp);
  g_assert_not_reached ();
}

static void
test_schema (gpointer data)
{
  SchemaTest *test = (SchemaTest *) data;
  gchar *child_name;

  child_name = g_strdup_printf ("/gschema/%s%s/subprocess/do_compile", test->name, test->opt ? "/opt" : "");
  g_test_trap_subprocess (child_name, 0, 0);
  g_free (child_name);

  if (test->err)
    {
      g_test_trap_assert_failed ();
      g_test_trap_assert_stderr_unmatched ("*CRITICAL*");
      g_test_trap_assert_stderr_unmatched ("*WARNING*");
      g_test_trap_assert_stderr (test->err);
    }
  else
    g_test_trap_assert_passed();
}

static const SchemaTest tests[] = {
  { "no-default",                   NULL, "*<default> is required in <key>*"                    },
  { "missing-quotes",               NULL, "*unknown keyword*"                                   },
  { "incomplete-list",              NULL, "*to follow array element*"                           },
  { "wrong-category",               NULL, "*unsupported l10n category*"                         },
  { "bad-type",                     NULL, "*Invalid GVariant type string*"                      },
  { "overflow",                     NULL, "*out of range*"                                      },
  { "range-wrong-type",             NULL, "*<range> not allowed for keys of type*"              },
  { "range-missing-min",            NULL, NULL                                                  },
  { "range-missing-max",            NULL, NULL                                                  },
  { "default-out-of-range",         NULL, "*<default> is not contained in the specified range*" },
  { "choices-wrong-type",           NULL, "*<choices> not allowed for keys of type*"            },
  { "choice-missing-value",         NULL, "*element 'choice' requires attribute 'value'*"       },
  { "default-not-in-choices",       NULL, "*<default> contains a string not in <choices>*"        },
  { "array-default-not-in-choices", NULL, "*<default> contains a string not in <choices>*"        },
  { "bad-key",                      NULL, "*Invalid name*"                                      },
  { "invalid-path",                 NULL, "*must begin and end with a slash*"                   },
  { "bad-key",                      "--allow-any-name", NULL                                    },
  { "bad-key2",                     NULL, "*Invalid name*"                                      },
  { "bad-key2",                     "--allow-any-name", NULL                                    },
  { "bad-key3",                     NULL, "*Invalid name*"                                      },
  { "bad-key3",                     "--allow-any-name", NULL                                    },
  { "bad-key4",                     NULL, "*Invalid name*"                                      },
  { "bad-key4",                     "--allow-any-name", NULL                                    },
  { "empty-key",                    NULL, "*Empty names*"                                       },
  { "empty-key",                    "--allow-any-name", "*Empty names*"                         },
  { "enum",                         NULL, NULL                                                  },
  { "enum-with-aliases",            NULL, NULL                                                  },
  { "enum-with-invalid-alias",      NULL, "*“banger” is not in enumerated type*"                },
  { "enum-with-invalid-value",      NULL, "*Invalid numeric value*"                             },
  { "enum-with-repeated-alias",     NULL, "*<alias value='sausages'/> already specified*"       },
  { "enum-with-repeated-nick",      NULL, "*<value nick='spam'/> already specified*"            },
  { "enum-with-repeated-value",     NULL, "*value='1' already specified*"                       },
  { "enum-with-chained-alias",      NULL, "*“sausages” is not in enumerated type*"              },
  { "enum-with-shadow-alias",       NULL, "*“mash” is already a member of the enum*"            },
  { "enum-with-choice",             NULL, "*<choices> cannot be specified*"                     },
  { "enum-with-bad-default",        NULL, "*<default> is not a valid member*"                   },
  { "choice",                       NULL, NULL                                                  },
  { "choice-upside-down",           NULL, NULL                                                  },
  { "bad-choice",                   NULL, "*<default> contains a string not in <choices>*"        },
  { "choice-bad",                   NULL, "*<default> contains a string not in <choices>*"        },
  { "choice-badtype",               NULL, "*<choices> not allowed for keys of type “i”*"        },
  { "bare-alias",                   NULL, "*enumerated or flags types or after <choices>*"      },
  { "choice-alias",                 NULL, NULL                                                  },
  { "default-in-aliases",           NULL, "*<default> contains a string not in <choices>*"        },
  { "choice-invalid-alias",         NULL, "*“befor” is not in <choices>*"                       },
  { "choice-shadowed-alias",        NULL, "*given when <choice value='before'/> was already*"   },
  { "range",                        NULL, NULL                                                  },
  { "range-badtype",                NULL, "*<range> not allowed for keys of type “s”*"          },
  { "range-low-default",            NULL, "*<default> is not contained in the specified range*" },
  { "range-high-default",           NULL, "*<default> is not contained in the specified range*" },
  { "range-default-low",            NULL, "*<default> is not contained in the specified range*" },
  { "range-default-high",           NULL, "*<default> is not contained in the specified range*" },
  { "range-parse-error",            NULL, "*invalid character in number*"                       },
  { "from-docs",                    NULL, NULL                                                  },
  { "extending",                    NULL, NULL                                                  },
  { "extend-missing",               NULL, "*extends not yet existing schema*"                   },
  { "extend-nonlist",               NULL, "*which is not a list*"                               },
  { "extend-self",                  NULL, "*not yet existing*"                                  },
  { "extend-wrong-list-indirect",   NULL, "*“y” does not extend “x”*"                           },
  { "extend-wrong-list",            NULL, "*“y” does not extend “x”*"                           },
  { "key-in-list-indirect",         NULL, "*Cannot add keys to a “list*"                        },
  { "key-in-list",                  NULL, "*Cannot add keys to a “list*"                        },
  { "list-of-missing",              NULL, "*is list of not yet existing schema*"                },
  { "extend-and-shadow",            NULL, "*shadows*use <override>*"                            },
  { "extend-and-shadow-indirect",   NULL, "*shadows*use <override>*"                            },
  { "override",                     NULL, NULL                                                  },
  { "override-missing",             NULL, "*No <key name='bar'> to override*"                   },
  { "override-range-error",         NULL, "*<override> is not contained in the specified range*"},
  { "override-then-key",            NULL, "*shadows <key name='foo'> in <schema id='base'>*"    },
  { "override-twice",               NULL, "*<override name='foo'> already specified*"           },
  { "override-type-error",          NULL, "*invalid character in number*"                       },
  { "flags-aliased-default",        NULL, "*<default> * not in the specified flags type*"       },
  { "flags-bad-default",            NULL, "*<default> * not in the specified flags type*"       },
  { "flags-more-than-one-bit",      NULL, "*flags values must have at most 1 bit set*"          },
  { "flags-with-enum-attr",         NULL, "*<enum id='flags'> not (yet) defined*"               },
  { "flags-with-enum-tag",          NULL, "*<flags id='flags'> not (yet) defined*"              },
  { "summary-xmllang",              NULL, "*Only one <summary> element allowed*"                },
  { "description-xmllang",          NULL, "*Only one <description> element allowed*"            },
  { "summary-xmllang-and-attrs",    NULL, "*attribute 'lang' invalid for element 'summary'*"    },
  { "inherit-gettext-domain",       NULL, NULL                                                  },
  { "range-type-test",              NULL, NULL                                                  },
  { "cdata",                        NULL, NULL                                                  }
};


int
main (int argc, char *argv[])
{
  guint i;

  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);

  for (i = 0; i < G_N_ELEMENTS (tests); ++i)
    {
      gchar *name;

      name = g_strdup_printf ("/gschema/%s%s", tests[i].name, tests[i].opt ? "/opt" : "");
      g_test_add_data_func (name, &tests[i], (gpointer) test_schema);
      g_free (name);

      name = g_strdup_printf ("/gschema/%s%s/subprocess/do_compile", tests[i].name, tests[i].opt ? "/opt" : "");
      g_test_add_data_func (name, &tests[i], (gpointer) test_schema_do_compile);
      g_free (name);
    }

  return g_test_run ();
}
