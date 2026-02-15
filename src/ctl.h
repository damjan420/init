#include <sys/types.h>

#define MAX_SV_NAME 256

enum {
  A_ENABLE,
  A_DISABLE,
  A_START,
  A_STOP,
  A_STATE,
};

enum {
  ERR_OK,
  STATE_UP,
  STATE_FAILED,
  STATE_STOPPED,
  STATE_RESTART_PENDING,
  STATE_UNKNOW,
  ERR_NOT_AV = -1,
  ERR_ALR_EN = -2,
  ERR_NOT_EN = -3,
  ERR_PERM = -4,
  ERR_NO_MEM = -5,
  ERR_ALR_UP = -6,
  ERR_NOT_UP = -7,
  ERR_CANT_PARSE = -8,
  ERR_UNSUPORTED = -9,
};

typedef struct  {
  int action;
  uid_t euid;
  int sv_name_len;
} ctl_payload;
