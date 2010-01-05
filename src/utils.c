#include "config.h"
#include "utils.h"

#include <stdio.h>
#include <signal.h>

#define info(format, ...) \
  fprintf (stderr, PACKAGE_NAME ": " format "\n", __VA_ARGS__);

RETSIGTYPE
signal_handler (int signal)
{
  info ("caught signal %d", signal);
}

void
set_signal_handlers (void)
{
  static const int SIGNALS[] = { SIGQUIT, SIGTERM, SIGABRT, SIGSEGV, SIGINT },
                   NUM_SIGNALS = sizeof (SIGNALS) / sizeof (SIGNALS[0]);
  int i;

  for (i = 0; i < NUM_SIGNALS; ++i)
    {
      signal (SIGNALS[i], signal_handler);
    }
}
