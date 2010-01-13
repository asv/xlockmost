#include "config.h"

#include <stdio.h>
#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/dpms.h>

#include <getopt.h>

#include "utils.h"
#include "pam_auth.h"

#define MAX_PASSWD_LEN 256
#define POINTER_ATTRIBUTES \
  (ButtonPressMask | ButtonReleaseMask | EnterWindowMask \
   | LeaveWindowMask | PointerMotionMask | PointerMotionHintMask \
   | Button1MotionMask | Button2MotionMask | Button3MotionMask \
   | Button4MotionMask | Button5MotionMask | ButtonMotionMask)

#define trace(format, args...) \
  fprintf (stderr, PACKAGE_NAME ": " format "\n", ##args);

typedef struct {
    CARD16 standby, suspend, off;
} dpms_timeout_t;

static Display *display = NULL;
static Window window;

static dpms_timeout_t server, user = {0, 0, 0};
static int dpms_manual = 0, dpms_capable = 0;
static BOOL dpms_state = 0;

static int doomsday = 0, pindex = 0;
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

static void
x11_hide_cursor (void);

static void
main_cleanup (void);

static void
parse_arguments (int argc, char **argv);

static void
parse_dpms_arguments (const char *options);

int
main (int argc, char **argv)
{
  parse_arguments (argc, argv);

  if (atexit (main_cleanup) != 0)
    {
      trace ("cannot set exit function");
      exit (EXIT_FAILURE);
    }

  set_signal_handlers ();
  pam_auth_init ();

  x11_init (argc, argv);
  x11_do_main_loop ();

  return EXIT_SUCCESS;
}

static void
x11_init (int argc, char **argv)
{
  int screen, depth;
  unsigned int width, height;

  XSetWindowAttributes xswa;
  XClassHint xch = {"xlockmost", "xlockmost"};

  int dummy;
  CARD16 power_lvl;

  display = XOpenDisplay (NULL);
  if (display == NULL)
    {
      trace ("unable to open display");
      exit (EXIT_FAILURE);
    }

  if (dpms_manual)
    {
      if (DPMSQueryExtension (display, &dummy, &dummy) && DPMSCapable (display))
        {
          dpms_capable = 1;

          if (DPMSInfo (display, &power_lvl, &dpms_state) && dpms_state != True)
            {
              DPMSEnable (display);
            }

          if (DPMSGetTimeouts (display, &server.standby,
                               &server.suspend, &server.off) != True)
            {
              dpms_capable = 0;

              if (dpms_state != True)
                {
                  DPMSDisable (display);
                }

              trace ("DPMSGetTimeouts failed");
            }
          else
            {
              DPMSSetTimeouts (display, user.standby, user.suspend, user.off);
            }
        }
      else
        {
          trace ("XDPMS extension not supported");
        }
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

  x11_hide_cursor ();

  XMapRaised (display, window);

  if (XGrabKeyboard (display, window, False, GrabModeAsync,
                     GrabModeAsync, CurrentTime) != GrabSuccess)
    {
      XUngrabKeyboard (display, CurrentTime);

      trace ("couldn't grab keyboard");
      exit (EXIT_FAILURE);
    }

  if (XGrabPointer (display, window, False, POINTER_ATTRIBUTES, GrabModeAsync,
                    GrabModeAsync, None, None, CurrentTime) != GrabSuccess)
    {
      XUngrabPointer (display, CurrentTime);

      trace ("couldn't grab pointer");
      exit (EXIT_FAILURE);
    }

  XFlush (display);
}

static void
x11_cleanup (void)
{
  if (display != NULL)
    {
      XUngrabKeyboard (display, CurrentTime);
      XUngrabPointer (display, CurrentTime);

      XUndefineCursor (display, window);

      if (dpms_manual && dpms_capable)
        {
          if (dpms_state != True)
            {
              DPMSDisable (display);
            }

          DPMSSetTimeouts (display, server.standby,
                           server.suspend, server.off);
        }

      XDestroyWindow (display, window);
      XCloseDisplay (display);
    }
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
  while (!doomsday)
    {
      XNextEvent (display, &ev);

      switch (ev.type)
        {
        case KeyPress:
          x11_handle_keypressed (&ev.xkey);
          break;

        case Expose:
          break;

        case KeyRelease:
        case ButtonRelease:
        case ButtonPress:
        case KeymapNotify:
        case MotionNotify:
        case LeaveNotify:
        case EnterNotify:
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
  doomsday = 1;
}

static void
x11_hide_cursor (void)
{
  Cursor invisible_cursor;
  Pixmap nodata_pixmap;
  XColor black;

  const char nodata[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  black.red = black.green = black.blue = 0;

  nodata_pixmap = XCreateBitmapFromData (display, window, nodata, 8, 8);
  invisible_cursor = XCreatePixmapCursor (display,nodata_pixmap, nodata_pixmap,
                                          &black, &black, 0, 0);

  XDefineCursor (display, window, invisible_cursor);

  XFreePixmap (display, nodata_pixmap);
  XFreeCursor (display, invisible_cursor);
}

static void
main_cleanup (void)
{
  pam_auth_cleanup ();
  x11_cleanup ();

  trace ("good bye");
}

static void
parse_arguments (int argc, char **argv)
{
  const char *short_options = "vhd:";
  const struct option long_options[] = {
    {"dpms",    required_argument,  0, 'd'},
    {"version", no_argument,        0, 'v'},
    {"help",    no_argument,        0, 'h'},

    {0, 0, 0, 0}
  };

  const char *usage =
    "Usage: " PACKAGE_NAME " [options]\n"
    "Options:\n"
    "   -h, --help                                show this help screen\n"
    "   -v, --version                             print version number\n"
    "   -d, --dpms MODE:TIMEOUT[,MODE:TIMEOUT...] set dpms mode and timeout";

  int opt, optind;
  while ((opt = getopt_long (argc, argv, short_options,
                             long_options, &optind)) != -1)
    {
      switch (opt)
        {
        case 'd':
          dpms_manual = 1;
          parse_dpms_arguments (optarg);
          break;
        case 'v':
          printf ("%s %s Copyright (C) 2010 Alexey Smirnov\n",
                  PACKAGE_NAME, VERSION);
          exit (EXIT_SUCCESS);
          break;
        case 'h':
          puts (usage);
          exit (EXIT_SUCCESS);
          break;
        default: /* unrecognized option */
          exit (EXIT_FAILURE);
        }
    }
}

static void
parse_dpms_arguments (const char *options)
{
  const char *p = options;
  char ent[16], *c = ent;

  CARD16 *val = NULL;
  int end = 1;

  while (end)
    {
      switch (*p)
        {
        case '\0':
          end = 0;
        case ',':
          if (val != NULL)
            {
              *c = '\0';
              *val = atoi (ent);

              val = NULL;
            }

          c = ent;
          break;
        case ':':
          if (strncmp (ent, "standby", c - ent) == 0)
            {
              val = &user.standby;
            }
          else if (strncmp (ent, "suspend", c - ent) == 0)
            {
              val = &user.suspend;
            }
          else if (strncmp (ent, "off", c - ent) == 0)
            {
              val = &user.off;
            }
          else
            {
              *c = '\0';
              trace ("invalid dpms mode `%s'", ent);
            }

          c = ent;
          break;
        default:
          if ((c - ent + 1) < sizeof (ent))
            {
              *c = *p;
              ++c;
            }
          else
            {
              c = ent;
            }
        }

      ++p;
    }

  if ((user.suspend != 0) && (user.standby > user.suspend))
    {
      trace ("illegal combination of values\n"
             "    standby time of %d is greater than suspend time of %d",
             user.standby, user.suspend);
      dpms_manual = 0;
    }
  else if ((user.off != 0) && (user.suspend > user.off))
    {
      trace ("illegal combination of values\n"
             "    suspend time of %d is greater than off time of %d",
             user.standby, user.suspend);
      dpms_manual = 0;
    }
  else if ((user.suspend == 0) && (user.off != 0) && (user.standby > user.off))
    {
      trace ("illegal combination of values\n"
             "    standby time of %d is greater than off time of %d",
             user.standby, user.suspend);
      dpms_manual = 0;
    }

  trace ("standby: %d suspend: %d off: %d",
         user.standby, user.suspend, user.off);
}
