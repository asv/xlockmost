#include "config.h"

#include <security/pam_appl.h>

#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_PWD_H
#include <pwd.h>
#endif

#include "pam_auth.h"

#define die(format, args...) \
  do { \
    fprintf (stderr, PACKAGE_NAME ": " format "\n", ##args); \
    exit (EXIT_FAILURE); \
  } while (0)

#define COPY_STRING(s) ((s) ? strdup ((s)) : NULL)

static char *pam_password = NULL;
static pam_handle_t *pamh = NULL;

static int
_pam_conv (int num_msg, const struct pam_message **msg,
           struct pam_response **resp, void *appdata_ptr);

void
pam_auth_init (void)
{
  struct pam_conv conv = { _pam_conv, NULL };
  struct passwd *pwd;
  uid_t uid;

  int retval;

  uid = getuid ();
  pwd = getpwuid (uid);

  if (pwd == NULL)
    {
      die ("pam_auth: %s", strerror (errno));
    }

  retval = pam_start ("common-auth", pwd->pw_name, &conv, &pamh);
  if (retval != PAM_SUCCESS)
    {
      die ("pam_auth: `%s'", pam_strerror (pamh, retval));
    }
}

int
pam_auth_validate (char *password)
{
  int retval;

  pam_password = password;
  retval = pam_authenticate (pamh, 0);

  if (retval != PAM_SUCCESS)
    {
      return AUTH_FAILURE;
    }

  retval = pam_acct_mgmt (pamh, 0);

  if (retval != PAM_SUCCESS)
    {
      return AUTH_FAILURE;
    }

  pam_end (pamh, PAM_SUCCESS);
  return AUTH_OK;
}

static int
_pam_conv (int num_msg, const struct pam_message **msg,
           struct pam_response **resp, void *appdata_ptr)
{
  int i;
  struct pam_response *reply = NULL;
  reply = malloc (sizeof (struct pam_response) * num_msg);

  for (i = 0; i < num_msg; ++i)
    {
      if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF)
        {
          reply[i].resp = COPY_STRING (pam_password);
          reply[i].resp_retcode = PAM_SUCCESS;
        }
    }

  *resp = reply;
  return PAM_SUCCESS;
}
