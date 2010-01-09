#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include "utils.h"
#include "pam_auth.h"

#define MAX_PASSWD_LEN 256

#define trace(format, args...) \
  fprintf (stderr, PACKAGE_NAME ": " format "\n", ##args);

static Display *display = NULL;
static Window window;

static int doomsday = 1, pindex = 0;
static char password[MAX_PASSWD_LEN + 1] = "";

static void
x11_init (int argc, char **argv);

static void
x11_cleanup (void);

static void
x11_do_main_loop (void);

static void
x11_break_main_loop (void);

static void
x11_handle_keypressed (XKeyPressedEvent *ev);

int
main (int argc, char **argv)
{
  set_signal_handlers ();
  pam_auth_init ();

  x11_init (argc, argv);
  x11_do_main_loop ();
  x11_cleanup ();

  return EXIT_SUCCESS;
}

static void
x11_init (int argc, char **argv)
{
  int screen, depth;
  unsigned int width, height;

  XSetWindowAttributes xswa;
  XClassHint xch = {"xlockmost", "xlockmost"};

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
}

static void
x11_cleanup (void)
{
  XUngrabKeyboard (display, CurrentTime);
  XDestroyWindow (display, window);
  XCloseDisplay (display);
}

static void
x11_handle_keypressed (XKeyPressedEvent *ev)
{
  KeySym keysym;
  char keystr[25];
  int len, i;

  len = XLookupString (ev, keystr, sizeof (keystr), &keysym, NULL);
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
          XBell (ev->display, 100);
        }
      break;

    case XK_Linefeed:
    case XK_KP_Enter:
    case XK_Execute:
    case XK_Return:
      password[pindex] = '\0';
      if (pam_auth_validate (password) == AUTH_OK)
        {
          x11_break_main_loop();
          return;
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
              XBell (ev->display, 100);
            }

          return;
        }

      for (i = 0; i < len; ++i)
        {
          password[pindex++] = keystr[i];
          if (pindex == MAX_PASSWD_LEN)
            {
              pindex = 0;
            }
        }
      break;
    }
}

static void
x11_do_main_loop (void)
{
  XEvent ev;
  while (doomsday)
    {
      XNextEvent (display, &ev);

      switch (ev.type)
        {
        case KeyPress:
          x11_handle_keypressed (&ev.xkey);
          break;
        case KeyRelease:
          break;
        case Expose:
          break;
        default:
          trace ("unknown event type %d", ev.type);
          break;
        }
    }
}

static void
x11_break_main_loop (void)
{
  doomsday = 0;
}

