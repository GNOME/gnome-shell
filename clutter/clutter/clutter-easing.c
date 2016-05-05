#include "clutter-build-config.h"

#include "clutter-easing.h"

#include <math.h>

double
clutter_linear (double t,
                double d)
{
  return t / d;
}

double
clutter_ease_in_quad (double t,
                      double d)
{
  double p = t / d;

  return p * p;
}

double
clutter_ease_out_quad (double t,
                       double d)
{
  double p = t / d;

  return -1.0 * p * (p - 2);
}

double
clutter_ease_in_out_quad (double t,
                          double d)
{
  double p = t / (d / 2);

  if (p < 1)
    return 0.5 * p * p;

  p -= 1;

  return -0.5 * (p * (p - 2) - 1);
}

double
clutter_ease_in_cubic (double t,
                       double d)
{
  double p = t / d;

  return p * p * p;
}

double
clutter_ease_out_cubic (double t,
                        double d)
{
  double p = t / d - 1;

  return p * p * p + 1;
}

double
clutter_ease_in_out_cubic (double t,
                           double d)
{
  double p = t / (d / 2);

  if (p < 1)
    return 0.5 * p * p * p;

  p -= 2;

  return 0.5 * (p * p * p + 2);
}

double
clutter_ease_in_quart (double t,
                       double d)
{
  double p = t / d;

  return p * p * p * p;
}

double
clutter_ease_out_quart (double t,
                        double d)
{
  double p = t / d - 1;

  return -1.0 * (p * p * p * p - 1);
}

double
clutter_ease_in_out_quart (double t,
                           double d)
{
  double p = t / (d / 2);

  if (p < 1)
    return 0.5 * p * p * p * p;

  p -= 2;

  return -0.5 * (p * p * p * p - 2);
}

double
clutter_ease_in_quint (double t,
                       double d)
 {
  double p = t / d;

  return p * p * p * p * p;
}

double
clutter_ease_out_quint (double t,
                        double d)
{
  double p = t / d - 1;

  return p * p * p * p * p + 1;
}

double
clutter_ease_in_out_quint (double t,
                           double d)
{
  double p = t / (d / 2);

  if (p < 1)
    return 0.5 * p * p * p * p * p;

  p -= 2;

  return 0.5 * (p * p * p * p * p + 2);
}

double
clutter_ease_in_sine (double t,
                      double d)
{
  return -1.0 * cos (t / d * G_PI_2) + 1.0;
}

double
clutter_ease_out_sine (double t,
                       double d)
{
  return sin (t / d * G_PI_2);
}

double
clutter_ease_in_out_sine (double t,
                          double d)
{
  return -0.5 * (cos (G_PI * t / d) - 1);
}

double
clutter_ease_in_expo (double t,
                      double d)
{
  return (t == 0) ? 0.0 : pow (2, 10 * (t / d - 1));
}

double
clutter_ease_out_expo (double t,
                       double d)
{
  return (t == d) ? 1.0 : -pow (2, -10 * t / d) + 1;
}

double
clutter_ease_in_out_expo (double t,
                          double d)
{
  double p;

  if (t == 0)
    return 0.0;

  if (t == d)
    return 1.0;

  p = t / (d / 2);

  if (p < 1)
    return 0.5 * pow (2, 10 * (p - 1));

  p -= 1;

  return 0.5 * (-pow (2, -10 * p) + 2);
}

double
clutter_ease_in_circ (double t,
                      double d)
{
  double p = t / d;

  return -1.0 * (sqrt (1 - p * p) - 1);
}

double
clutter_ease_out_circ (double t,
                       double d)
{
  double p = t / d - 1;

  return sqrt (1 - p * p);
}

double
clutter_ease_in_out_circ (double t,
                          double d)
{
  double p = t / (d / 2);

  if (p < 1)
    return -0.5 * (sqrt (1 - p * p) - 1);

  p -= 2;

  return 0.5 * (sqrt (1 - p * p) + 1);
}

double
clutter_ease_in_elastic (double t,
                         double d)
{
  double p = d * .3;
  double s = p / 4;
  double q = t / d;

  if (q == 1)
    return 1.0;

  q -= 1;

  return -(pow (2, 10 * q) * sin ((q * d - s) * (2 * G_PI) / p));
}

double
clutter_ease_out_elastic (double t,
                          double d)
{
  double p = d * .3;
  double s = p / 4;
  double q = t / d;

  if (q == 1)
    return 1.0;

  return pow (2, -10 * q) * sin ((q * d - s) * (2 * G_PI) / p) + 1.0;
}

double
clutter_ease_in_out_elastic (double t,
                             double d)
{
  double p = d * (.3 * 1.5);
  double s = p / 4;
  double q = t / (d / 2);

  if (q == 2)
    return 1.0;

  if (q < 1)
    {
      q -= 1;

      return -.5 * (pow (2, 10 * q) * sin ((q * d - s) * (2 * G_PI) / p));
    }
  else
    {
      q -= 1;

      return pow (2, -10 * q)
           * sin ((q * d - s) * (2 * G_PI) / p)
           * .5 + 1.0;
    }
}

double
clutter_ease_in_back (double t,
                      double d)
{
  double p = t / d;

  return p * p * ((1.70158 + 1) * p - 1.70158);
}

double
clutter_ease_out_back (double t,
                       double d)
{
  double p = t / d - 1;

  return p * p * ((1.70158 + 1) * p + 1.70158) + 1;
}

double
clutter_ease_in_out_back (double t,
                          double d)
{
  double p = t / (d / 2);
  double s = 1.70158 * 1.525;

  if (p < 1)
    return 0.5 * (p * p * ((s + 1) * p - s));

  p -= 2;

  return 0.5 * (p * p * ((s + 1) * p + s) + 2);
}

static inline double
ease_out_bounce_internal (double t,
                          double d)
{
  double p = t / d;

  if (p < (1 / 2.75))
    {
      return 7.5625 * p * p;
    }
  else if (p < (2 / 2.75))
    {
      p -= (1.5 / 2.75);

      return 7.5625 * p * p + .75;
    }
  else if (p < (2.5 / 2.75))
    {
      p -= (2.25 / 2.75);

      return 7.5625 * p * p + .9375;
    }
  else
    {
      p -= (2.625 / 2.75);

      return 7.5625 * p * p + .984375;
    }
}

static inline double
ease_in_bounce_internal (double t,
                         double d)
{
  return 1.0 - ease_out_bounce_internal (d - t, d);
}

double
clutter_ease_in_bounce (double t,
                        double d)
{
  return ease_in_bounce_internal (t, d);
}

double
clutter_ease_out_bounce (double t,
                         double d)
{
  return ease_out_bounce_internal (t, d);
}

double
clutter_ease_in_out_bounce (double t,
                            double d)
{
  if (t < d / 2)
    return ease_in_bounce_internal (t * 2, d) * 0.5;
  else
    return ease_out_bounce_internal (t * 2 - d, d) * 0.5 + 1.0 * 0.5;
}

static inline double
ease_steps_end (double p,
                int    n_steps)
{
  return floor (p * (double) n_steps) / (double) n_steps;
}

double
clutter_ease_steps_start (double t,
                          double d,
                          int    n_steps)
{
  return 1.0 - ease_steps_end (1.0 - (t / d), n_steps);
}

double
clutter_ease_steps_end (double t,
                        double d,
                        int    n_steps)
{
  return ease_steps_end ((t / d), n_steps);
}

static inline double
x_for_t (double t,
         double x_1,
         double x_2)
{
  double omt = 1.0 - t;

  return 3.0 * omt * omt * t * x_1
       + 3.0 * omt * t * t * x_2
       + t * t * t;
}

static inline double
y_for_t (double t,
         double y_1,
         double y_2)
{
  double omt = 1.0 - t;

  return 3.0 * omt * omt * t * y_1
       + 3.0 * omt * t * t * y_2
       + t * t * t;
}

static inline double
t_for_x (double x,
         double x_1,
         double x_2)
{
  double min_t = 0, max_t = 1;
  int i;

  for (i = 0; i < 30; ++i)
    {
      double guess_t = (min_t + max_t) / 2.0;
      double guess_x = x_for_t (guess_t, x_1, x_2);

      if (x < guess_x)
        max_t = guess_t;
      else
        min_t = guess_t;
    }

  return (min_t + max_t) / 2.0;
}

double
clutter_ease_cubic_bezier (double t,
                           double d,
                           double x_1,
                           double y_1,
                           double x_2,
                           double y_2)
{
  double p = t / d;

  if (p == 0.0)
    return 0.0;

  if (p == 1.0)
    return 1.0;

  return y_for_t (t_for_x (p, x_1, x_2), y_1, y_2);
}

/*< private >
 * _clutter_animation_modes:
 *
 * A mapping of animation modes and easing functions.
 */
static const struct {
  ClutterAnimationMode mode;
  ClutterEasingFunc func;
  const char *name;
} _clutter_animation_modes[] = {
  { CLUTTER_CUSTOM_MODE,         NULL, "custom" },

  { CLUTTER_LINEAR,              clutter_linear, "linear" },
  { CLUTTER_EASE_IN_QUAD,        clutter_ease_in_quad, "easeInQuad" },
  { CLUTTER_EASE_OUT_QUAD,       clutter_ease_out_quad, "easeOutQuad" },
  { CLUTTER_EASE_IN_OUT_QUAD,    clutter_ease_in_out_quad, "easeInOutQuad" },
  { CLUTTER_EASE_IN_CUBIC,       clutter_ease_in_cubic, "easeInCubic" },
  { CLUTTER_EASE_OUT_CUBIC,      clutter_ease_out_cubic, "easeOutCubic" },
  { CLUTTER_EASE_IN_OUT_CUBIC,   clutter_ease_in_out_cubic, "easeInOutCubic" },
  { CLUTTER_EASE_IN_QUART,       clutter_ease_in_quart, "easeInQuart" },
  { CLUTTER_EASE_OUT_QUART,      clutter_ease_out_quart, "easeOutQuart" },
  { CLUTTER_EASE_IN_OUT_QUART,   clutter_ease_in_out_quart, "easeInOutQuart" },
  { CLUTTER_EASE_IN_QUINT,       clutter_ease_in_quint, "easeInQuint" },
  { CLUTTER_EASE_OUT_QUINT,      clutter_ease_out_quint, "easeOutQuint" },
  { CLUTTER_EASE_IN_OUT_QUINT,   clutter_ease_in_out_quint, "easeInOutQuint" },
  { CLUTTER_EASE_IN_SINE,        clutter_ease_in_sine, "easeInSine" },
  { CLUTTER_EASE_OUT_SINE,       clutter_ease_out_sine, "easeOutSine" },
  { CLUTTER_EASE_IN_OUT_SINE,    clutter_ease_in_out_sine, "easeInOutSine" },
  { CLUTTER_EASE_IN_EXPO,        clutter_ease_in_expo, "easeInExpo" },
  { CLUTTER_EASE_OUT_EXPO,       clutter_ease_out_expo, "easeOutExpo" },
  { CLUTTER_EASE_IN_OUT_EXPO,    clutter_ease_in_out_expo, "easeInOutExpo" },
  { CLUTTER_EASE_IN_CIRC,        clutter_ease_in_circ, "easeInCirc" },
  { CLUTTER_EASE_OUT_CIRC,       clutter_ease_out_circ, "easeOutCirc" },
  { CLUTTER_EASE_IN_OUT_CIRC,    clutter_ease_in_out_circ, "easeInOutCirc" },
  { CLUTTER_EASE_IN_ELASTIC,     clutter_ease_in_elastic, "easeInElastic" },
  { CLUTTER_EASE_OUT_ELASTIC,    clutter_ease_out_elastic, "easeOutElastic" },
  { CLUTTER_EASE_IN_OUT_ELASTIC, clutter_ease_in_out_elastic, "easeInOutElastic" },
  { CLUTTER_EASE_IN_BACK,        clutter_ease_in_back, "easeInBack" },
  { CLUTTER_EASE_OUT_BACK,       clutter_ease_out_back, "easeOutBack" },
  { CLUTTER_EASE_IN_OUT_BACK,    clutter_ease_in_out_back, "easeInOutBack" },
  { CLUTTER_EASE_IN_BOUNCE,      clutter_ease_in_bounce, "easeInBounce" },
  { CLUTTER_EASE_OUT_BOUNCE,     clutter_ease_out_bounce, "easeOutBounce" },
  { CLUTTER_EASE_IN_OUT_BOUNCE,  clutter_ease_in_out_bounce, "easeInOutBounce" },

  /* the parametrized functions need a cast */
  { CLUTTER_STEPS,               (ClutterEasingFunc) clutter_ease_steps_end, "steps" },
  { CLUTTER_STEP_START,          (ClutterEasingFunc) clutter_ease_steps_start, "stepStart" },
  { CLUTTER_STEP_END,            (ClutterEasingFunc) clutter_ease_steps_end, "stepEnd" },

  { CLUTTER_CUBIC_BEZIER,        (ClutterEasingFunc) clutter_ease_cubic_bezier, "cubicBezier" },
  { CLUTTER_EASE,                (ClutterEasingFunc) clutter_ease_cubic_bezier, "ease" },
  { CLUTTER_EASE_IN,             (ClutterEasingFunc) clutter_ease_cubic_bezier, "easeIn" },
  { CLUTTER_EASE_OUT,            (ClutterEasingFunc) clutter_ease_cubic_bezier, "easeOut" },
  { CLUTTER_EASE_IN_OUT,         (ClutterEasingFunc) clutter_ease_cubic_bezier, "easeInOut" },

  { CLUTTER_ANIMATION_LAST,      NULL, "sentinel" },
};

ClutterEasingFunc
clutter_get_easing_func_for_mode (ClutterAnimationMode mode)
{
  g_assert (_clutter_animation_modes[mode].mode == mode);
  g_assert (_clutter_animation_modes[mode].func != NULL);

  return _clutter_animation_modes[mode].func;
}

const char *
clutter_get_easing_name_for_mode (ClutterAnimationMode mode)
{
  g_assert (_clutter_animation_modes[mode].mode == mode);
  g_assert (_clutter_animation_modes[mode].func != NULL);

  return _clutter_animation_modes[mode].name;
}

double
clutter_easing_for_mode (ClutterAnimationMode mode,
                         double               t,
                         double               d)
{
  g_assert (_clutter_animation_modes[mode].mode == mode);
  g_assert (_clutter_animation_modes[mode].func != NULL);

  return _clutter_animation_modes[mode].func (t, d);
}
