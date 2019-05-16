#include "nd-daemon.h"

int
main (void)
{
  g_autoptr(NdDaemon) daemon = NULL;

  daemon = nd_daemon_new ();

  return nd_daemon_run (daemon);
}
