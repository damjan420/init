#include <sys/types.h>

#define MAX_SV_NAME 256

enum {
  A_ENABLE,
  A_DISABLE,
  A_START,
  A_STOP,
};

enum {
  ERR_OK,
  ERR_NOT_AV = -1,
  ERR_ALR_EN = -2,
  ERR_NOT_EN = -3,
  ERR_PERM = -4,
  ERR_NO_MEM = -5,
  ERR_UNSUPORTED= -6,
};

typedef struct  {
  int action;
  uid_t euid;
  char sv_name[MAX_SV_NAME];
} ctl_payload;
