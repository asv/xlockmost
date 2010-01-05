#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "utils.h"
#include "pam_auth.h"

#define trace(format, args...) \
  fprintf (stderr, PACKAGE_NAME ": " format "\n", ##args);

int
main (int argc, char **argv)
{
  Display *display;
  Window window;
  XSetWindowAttributes xswa;
  XClassHint xch = {"xlockmost", "xlockmost"};

  unsigned width, height, screen, depth;

  int pindex = 0;
  char password[1024];

  set_signal_handlers ();
  pam_auth_init ();

  display = XOpenDisplay (NULL);

  if (display == NULL)
    {
      trace ("unable to open display");
      exit (EXIT_FAILURE);
    }

  screen = DefaultScreen (display);
  depth = DefaultDepth (display, screen);

  width = XDisplayWidth (display, screen);
  height = XDisplayHeight (display, screen);

  xswa.override_redirect = 1;
  xswa.event_mask = KeyPressMask | KeyReleaseMask | ExposureMask;

  window = XCreateWindow (display, RootWindow (display, screen),
                          0, 0, width, height, 0, depth, CopyFromParent,
                          DefaultVisual (display, screen),
                          CWOverrideRedirect | CWEventMask, &xswa);

  Xutf8SetWMProperties (display, window, "xlockmost", "xlockmost",
                      argv, argc, NULL, NULL, &xch);

  XSetWindowBackground (display, window, BlackPixel (display, screen));
  XMapRaised (display, window);

  XGrabKeyboard (display, window, False,
                 GrabModeAsync, GrabModeAsync, CurrentTime);

  XFlush (display);

  FOREVER
    {
      XEvent event;
      XNextEvent (display, &event);

      switch (event.type)
        {
        case KeyPress:
            {
              KeySym keysym;
              int len, i;
              char keystr[25];

              len = XLookupString (&event.xkey, keystr,
                                   sizeof (keystr), &keysym, NULL);
              switch (keysym)
                {
                case XK_Delete:
                case XK_KP_Delete:
                case XK_BackSpace:
                  if (pindex > 0)
                    {
                      --pindex;
                    }
                  else
                    {
                      XBell (display, 100);
                    }
                  break;

                case XK_Linefeed:
                case XK_KP_Enter:
                case XK_Execute:
                case XK_Return:
                  password[pindex] = '\0';
                  if (pam_auth_validate (password) == AUTH_OK)
                    {
                      goto exit_success;
                    }

                  XBell (display, 100);
                case XK_Escape:
                case XK_Clear:
                  pindex = 0;
                  break;
                default:
                  if (len == 0)
                    {
                      if (IsModifierKey (keysym) != True)
                        {
                          XBell (display, 100);
                        }
                      continue;
                    }

                  for (i = 0; i < len; ++i)
                    {
                      password[pindex++] = keystr[i];
                      if (pindex == sizeof (password))
                        {
                          pindex = 0;
                        }
                    }
                  break;
                }
            }
          break;
        case KeyRelease:
          break;
        case Expose:
          break;
        default:
          trace ("unknown event type %d", event.type);
          break;
        }
    }

exit_success:
  XUngrabKeyboard (display, CurrentTime);
  XDestroyWindow (display, window);
  XCloseDisplay (display);

  return EXIT_SUCCESS;
}
