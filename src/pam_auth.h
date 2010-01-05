#ifndef __PAM_AUTH_H__
#define __PAM_AUTH_H__

enum { AUTH_FAILURE, AUTH_OK };

void
pam_auth_init (void);

int
pam_auth_validate (char *password);

#endif /* __PAM_AUTH_H__ */
