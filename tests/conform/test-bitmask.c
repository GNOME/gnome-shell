#include <cogl/cogl.h>

#include <string.h>
#include <stdarg.h>

#include "test-utils.h"

/* This is testing CoglBitmask which is an internal data structure
   within Cogl. Cogl doesn't export the symbols for this data type so
   we just directly include the source instead */

#define _COGL_IN_TEST_BITMASK
#include <cogl/cogl-bitmask.h>
#include <cogl/cogl-bitmask.c>
#include <cogl/cogl-util.c>

typedef struct
{
  int n_bits;
  int *bits;
} CheckData;

static CoglBool
check_bit (int bit_num, void *user_data)
{
  CheckData *data = user_data;
  int i;

  for (i = 0; i < data->n_bits; i++)
    if (data->bits[i] == bit_num)
      {
        data->bits[i] = -1;
        return TRUE;
      }

  g_assert_not_reached ();

  return TRUE;
}

static void
verify_bits (const CoglBitmask *bitmask,
             ...)
{
  CheckData data;
  va_list ap, ap_copy;
  int i;

  va_start (ap, bitmask);
  G_VA_COPY (ap_copy, ap);

  for (data.n_bits = 0; va_arg (ap, int) != -1; data.n_bits++);

  data.bits = alloca (data.n_bits * (sizeof (int)));

  G_VA_COPY (ap, ap_copy);

  for (i = 0; i < data.n_bits; i++)
    data.bits[i] = va_arg (ap, int);

  _cogl_bitmask_foreach (bitmask, check_bit, &data);

  for (i = 0; i < data.n_bits; i++)
    g_assert_cmpint (data.bits[i], ==, -1);

  g_assert_cmpint (_cogl_bitmask_popcount (bitmask), ==, data.n_bits);

  for (i = 0; i < 1024; i++)
    {
      int upto_popcount = 0;
      int j;

      G_VA_COPY (ap, ap_copy);

      for (j = 0; j < data.n_bits; j++)
        if (va_arg (ap, int) < i)
          upto_popcount++;

      g_assert_cmpint (_cogl_bitmask_popcount_upto (bitmask, i),
                       ==,
                       upto_popcount);

      G_VA_COPY (ap, ap_copy);

      for (j = 0; j < data.n_bits; j++)
        if (va_arg (ap, int) == i)
          break;

      g_assert_cmpint (_cogl_bitmask_get (bitmask, i), ==, (j < data.n_bits));
    }
}

void
test_bitmask (void)
{
  CoglBitmask bitmask;
  CoglBitmask other_bitmask;
  /* A dummy bit to make it use arrays sometimes */
  int dummy_bit;
  int i;

  for (dummy_bit = -1; dummy_bit < 256; dummy_bit += 40)
    {
      _cogl_bitmask_init (&bitmask);
      _cogl_bitmask_init (&other_bitmask);

      if (dummy_bit != -1)
        _cogl_bitmask_set (&bitmask, dummy_bit, TRUE);

      verify_bits (&bitmask, dummy_bit, -1);

      _cogl_bitmask_set (&bitmask, 1, TRUE);
      _cogl_bitmask_set (&bitmask, 4, TRUE);
      _cogl_bitmask_set (&bitmask, 5, TRUE);

      verify_bits (&bitmask, 1, 4, 5, dummy_bit, -1);

      _cogl_bitmask_set (&bitmask, 4, FALSE);

      verify_bits (&bitmask, 1, 5, dummy_bit, -1);

      _cogl_bitmask_clear_all (&bitmask);

      verify_bits (&bitmask, -1);

      if (dummy_bit != -1)
        _cogl_bitmask_set (&bitmask, dummy_bit, TRUE);

      verify_bits (&bitmask, dummy_bit, -1);

      _cogl_bitmask_set (&bitmask, 1, TRUE);
      _cogl_bitmask_set (&bitmask, 4, TRUE);
      _cogl_bitmask_set (&bitmask, 5, TRUE);
      _cogl_bitmask_set (&other_bitmask, 5, TRUE);
      _cogl_bitmask_set (&other_bitmask, 6, TRUE);

      _cogl_bitmask_set_bits (&bitmask, &other_bitmask);

      verify_bits (&bitmask, 1, 4, 5, 6, dummy_bit, -1);
      verify_bits (&other_bitmask, 5, 6, -1);

      _cogl_bitmask_set (&bitmask, 6, FALSE);

      verify_bits (&bitmask, 1, 4, 5, dummy_bit, -1);

      _cogl_bitmask_xor_bits (&bitmask, &other_bitmask);

      verify_bits (&bitmask, 1, 4, 6, dummy_bit, -1);
      verify_bits (&other_bitmask, 5, 6, -1);

      _cogl_bitmask_set_range (&bitmask, 5, TRUE);

      verify_bits (&bitmask, 0, 1, 2, 3, 4, 6, dummy_bit, -1);

      _cogl_bitmask_set_range (&bitmask, 4, FALSE);

      verify_bits (&bitmask, 4, 6, dummy_bit, -1);

      _cogl_bitmask_destroy (&other_bitmask);
      _cogl_bitmask_destroy (&bitmask);
    }

  /* Extra tests for really long bitmasks */
  _cogl_bitmask_init (&bitmask);
  _cogl_bitmask_set_range (&bitmask, 400, TRUE);
  _cogl_bitmask_init (&other_bitmask);
  _cogl_bitmask_set (&other_bitmask, 5, TRUE);
  _cogl_bitmask_xor_bits (&bitmask, &other_bitmask);

  for (i = 0; i < 1024; i++)
    g_assert_cmpint (_cogl_bitmask_get (&bitmask, i),
                     ==,
                     (i == 5 ? FALSE :
                      i < 400 ? TRUE :
                      FALSE));

  _cogl_bitmask_set_range (&other_bitmask, 500, TRUE);
  _cogl_bitmask_set_bits (&bitmask, &other_bitmask);

  for (i = 0; i < 1024; i++)
    g_assert_cmpint (_cogl_bitmask_get (&bitmask, i), ==, (i < 500));

  if (cogl_test_verbose ())
    g_print ("OK\n");
}
