#include "config.h"

#include "shell-square-bin.h"

struct _ShellSquareBin
{
  /*< private >*/
  StBin parent_instance;
};

G_DEFINE_TYPE (ShellSquareBin, shell_square_bin, ST_TYPE_BIN);

static void
shell_square_bin_get_preferred_width (ClutterActor *actor,
                                      float         for_height,
                                      float        *min_width_p,
                                      float        *natural_width_p)
{
  float min_width, nat_width;

  /* Return the actual height to keep the squared aspect */
  clutter_actor_get_preferred_height (actor, -1,
                                      &min_width, &nat_width);

  if (min_width_p)
    *min_width_p = min_width;

  if (natural_width_p)
    *natural_width_p = nat_width;
}

static void
shell_square_bin_init (ShellSquareBin *self)
{
}

static void
shell_square_bin_class_init (ShellSquareBinClass *klass)
{
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  actor_class->get_preferred_width = shell_square_bin_get_preferred_width;
}
