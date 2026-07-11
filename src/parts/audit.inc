/* Development audit stubs — all no-ops in production builds */
#ifndef AUDIT_H
#define AUDIT_H

static void audit_init(void) {}
static void audit_report(void) {}

#define AUDIT_OPCODE()      do {} while(0)
#define AUDIT_USER_FUNC()   do {} while(0)
#define AUDIT_FRAME_PUSH()  do {} while(0)
#define AUDIT_FRAME_POP()   do {} while(0)
#define AUDIT_FREE_VALUE()  do {} while(0)
#define AUDIT_CLONE()       do {} while(0)
#define AUDIT_FIND_VAR()    do {} while(0)
#define AUDIT_ENV_SET()     do {} while(0)

#endif
