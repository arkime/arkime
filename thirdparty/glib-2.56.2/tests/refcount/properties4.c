#include <glib.h>
#include <glib-object.h>

#define MY_TYPE_BADGER              (my_badger_get_type ())
#define MY_BADGER(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), MY_TYPE_BADGER, MyBadger))
#define MY_IS_BADGER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MY_TYPE_BADGER))
#define MY_BADGER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), MY_TYPE_BADGER, MyBadgerClass))
#define MY_IS_BADGER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), MY_TYPE_BADGER))
#define MY_BADGER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), MY_TYPE_BADGER, MyBadgerClass))

enum {
  PROP_0,
  PROP_MAMA
};

typedef struct _MyBadger MyBadger;
typedef struct _MyBadgerClass MyBadgerClass;

struct _MyBadger
{
  GObject parent_instance;

  MyBadger * mama;
  guint mama_notify_count;
};

struct _MyBadgerClass
{
  GObjectClass parent_class;
};

static GType my_badger_get_type (void);
G_DEFINE_TYPE (MyBadger, my_badger, G_TYPE_OBJECT)

static void my_badger_dispose (GObject * object);

static void my_badger_get_property (GObject    *object,
				    guint       prop_id,
				    GValue     *value,
				    GParamSpec *pspec);
static void my_badger_set_property (GObject      *object,
				    guint         prop_id,
				    const GValue *value,
				    GParamSpec   *pspec);

static void my_badger_mama_notify (GObject    *object,
				   GParamSpec *pspec);

static void
my_badger_class_init (MyBadgerClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;

  gobject_class->dispose = my_badger_dispose;

  gobject_class->get_property = my_badger_get_property;
  gobject_class->set_property = my_badger_set_property;

  g_object_class_install_property (gobject_class,
				   PROP_MAMA,
				   g_param_spec_object ("mama",
							NULL,
							NULL,
							MY_TYPE_BADGER,
							G_PARAM_READWRITE));
}

static void
my_badger_init (MyBadger * self)
{
  g_signal_connect (self, "notify::mama", G_CALLBACK (my_badger_mama_notify),
      NULL);
}

static void
my_badger_dispose (GObject * object)
{
  MyBadger * self;

  self = MY_BADGER (object);

  if (self->mama != NULL)
    {
      g_object_unref (self->mama);
      self->mama = NULL;
    }

  G_OBJECT_CLASS (my_badger_parent_class)->dispose (object);
}

static void
my_badger_get_property (GObject    *object,
			guint        prop_id,
			GValue     *value,
			GParamSpec *pspec)
{
  MyBadger *self;

  self = MY_BADGER (object);

  switch (prop_id)
    {
    case PROP_MAMA:
      g_value_set_object (value, self->mama);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
my_badger_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
  MyBadger *self;

  self = MY_BADGER (object);

  switch (prop_id)
    {
    case PROP_MAMA:
      if (self->mama)
	g_object_unref (self->mama);
      self->mama = g_value_dup_object (value);
      if (self->mama)
	g_object_set (self->mama, "mama", NULL, NULL); /* another notify */
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
my_badger_mama_notify (GObject    *object,
                       GParamSpec *pspec)
{
  MyBadger *self;

  self = MY_BADGER (object);

  self->mama_notify_count++;
}

int
main (int argc, char **argv)
{
  MyBadger * badger1, * badger2;
  gpointer test;

  g_print ("START: %s\n", argv[0]);
  g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL | g_log_set_always_fatal (G_LOG_FATAL_MASK));

  badger1 = g_object_new (MY_TYPE_BADGER, NULL);
  badger2 = g_object_new (MY_TYPE_BADGER, NULL);

  g_object_set (badger1, "mama", badger2, NULL);
  g_assert_cmpuint (badger1->mama_notify_count, ==, 1);
  g_assert_cmpuint (badger2->mama_notify_count, ==, 1);
  g_object_get (badger1, "mama", &test, NULL);
  g_assert (test == badger2);
  g_object_unref (test);

  g_object_unref (badger1);
  g_object_unref (badger2);

  return 0;
}
