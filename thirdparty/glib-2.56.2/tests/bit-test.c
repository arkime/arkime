#include <glib.h>

#if defined(__GNUC__) && (__GNUC__ >= 4)
# define TEST_BUILTINS 1
#else
# define TEST_BUILTINS 0
#endif

#if TEST_BUILTINS
static gint
builtin_bit_nth_lsf1 (gulong mask, gint nth_bit)
{
  if (nth_bit >= 0)
    {
      if (G_LIKELY (nth_bit < GLIB_SIZEOF_LONG * 8 - 1))
	mask &= -(1UL<<(nth_bit+1));
      else
	mask = 0;
    }
  return __builtin_ffsl(mask) - 1;
}

static gint
builtin_bit_nth_lsf2 (gulong mask, gint nth_bit)
{
  if (nth_bit >= 0)
    {
      if (G_LIKELY (nth_bit < GLIB_SIZEOF_LONG * 8 - 1))
	mask &= -(1UL<<(nth_bit+1));
      else
	mask = 0;
    }
  return mask ? __builtin_ctzl(mask) : -1;
}

static gint
builtin_bit_nth_msf (gulong mask, gint nth_bit)
{
  if (nth_bit >= 0 && nth_bit < GLIB_SIZEOF_LONG * 8)
    mask &= (1UL<<nth_bit)-1;
  return mask ? GLIB_SIZEOF_LONG * 8 - 1 - __builtin_clzl(mask) : -1;
}


static guint
builtin_bit_storage (gulong number)
{
  return number ? GLIB_SIZEOF_LONG * 8 - __builtin_clzl(number) : 1;
}
#endif


static gint
naive_bit_nth_lsf (gulong mask, gint   nth_bit)
{
  if (G_UNLIKELY (nth_bit < -1))
    nth_bit = -1;
  while (nth_bit < ((GLIB_SIZEOF_LONG * 8) - 1))
    {
      nth_bit++;
      if (mask & (1UL << nth_bit))
	return nth_bit;
    }
  return -1;
}

static gint
naive_bit_nth_msf (gulong mask, gint   nth_bit)
{
  if (nth_bit < 0 || G_UNLIKELY (nth_bit > GLIB_SIZEOF_LONG * 8))
    nth_bit = GLIB_SIZEOF_LONG * 8;
  while (nth_bit > 0)
    {
      nth_bit--;
      if (mask & (1UL << nth_bit))
	return nth_bit;
    }
  return -1;
}

static guint
naive_bit_storage (gulong number)
{
  guint n_bits = 0;
  
  do
    {
      n_bits++;
      number >>= 1;
    }
  while (number);
  return n_bits;
}



#define TEST(f1, f2, i) \
	if (f1 (i) != f2 (i)) { \
		g_error (G_STRINGIFY (f1) " (%lu) = %d; " \
			 G_STRINGIFY (f2) " (%lu) = %d; ", \
			 i, f1 (i), \
			 i, f2 (i)); \
		return 1; \
	}
#define TEST2(f1, f2, i, n) \
	if (f1 (i, n) != f2 (i, n)) { \
		g_error (G_STRINGIFY (f1) " (%lu, %d) = %d; " \
			 G_STRINGIFY (f2) " (%lu, %d) = %d; ", \
			 i, n, f1 (i, n), \
			 i, n, f2 (i, n)); \
		return 1; \
	}

int
main (void)
{
  gulong i;
  gint nth_bit;

  /* we loop like this: 0, -1, 1, -2, 2, -3, 3, ... */
  for (i = 0; (glong)i < 1500 ; i = -(i+((glong)i>=0))) {

#if TEST_BUILTINS
    TEST (naive_bit_storage, builtin_bit_storage, i);
#endif
    TEST (naive_bit_storage, g_bit_storage, i);

    for (nth_bit = -3; nth_bit <= 2 + GLIB_SIZEOF_LONG * 8; nth_bit++) {

#if TEST_BUILTINS
      TEST2 (naive_bit_nth_lsf, builtin_bit_nth_lsf1, i, nth_bit);
      TEST2 (naive_bit_nth_lsf, builtin_bit_nth_lsf2, i, nth_bit);
#endif
      TEST2 (naive_bit_nth_lsf, g_bit_nth_lsf, i, nth_bit);

#if TEST_BUILTINS
      TEST2 (naive_bit_nth_msf, builtin_bit_nth_msf, i, nth_bit);
#endif
      TEST2 (naive_bit_nth_msf, g_bit_nth_msf, i, nth_bit);

    }
  }

  return 0;
}
