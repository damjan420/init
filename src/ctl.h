#include <sys/types.h>

#define MAX_PAYLOAD_SIZE 8192

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
  ERR_TO_LARGE = -9,
  ERR_UNSUPORTED = -10,
};

typedef struct {
  uid_t euid;
  size_t payload_size;
} req_hdr;

typedef struct {
  int action;
  char sv_name[];
} req_payload;

typedef struct {
  int err;
  size_t size;
  char data[256];
} res_payload;

/* typedef struct{ */
/*   pid_t pid; */
/*   int state; */
/* } sv_state; */
