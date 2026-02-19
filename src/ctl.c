#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/uio.h>

#include "klog.h"
#include "ctl.h"
#include "sv.h"


char* init_strerror(int err) {
  // TODO: improve error messages
  const char* msg;
  switch (err){
  case STATE_UP:
    msg = "Service up\n";
    break;
  case STATE_RESTART_PENDING:
    msg = "Service has a restart pending\n";
    break;
  case STATE_FAILED:
    msg = "Service failed\n";
    break;
  case STATE_STOPPED:
    msg = "Service stopped\n";
    break;
  case STATE_UNKNOWN:
    msg = "State unknown\n";
    break;
  case ERR_OK:
    msg = "Sucess\n";
    break;
  case ERR_NOT_AV:
    msg = "Service file not found in /etc/init.d/av\n";
    break;
  case ERR_ALR_EN:
    msg = "Service already enabled\n";
    break;
  case ERR_NOT_EN:
    msg = "Service not enabled\n";
    break;
 case ERR_PERM:
   msg = "Action not permitted\n";
   break;
 case ERR_NO_MEM:
   msg = "Insufficient memmory to complete the request\n";
   break;

 case  ERR_ALR_UP:
   msg = "Service is already running\n";
   break;
 case ERR_NOT_UP:
   msg = "Service is not running\n";
   break;
 case  ERR_CANT_PARSE:
   msg = "Cannnot parse service file\n";
   break;
  case ERR_UNSUPORTED:
    msg = "Requested action not supported\n";
    break;
  }
  char* ret = strdup(msg);
  return ret;
}

void display_res_payload(res_payload* payload, int action) {
  if(action == A_ENABLE || action == A_DISABLE) {
    char* msg = init_strerror(payload->err);
      if(!msg) {
        fprintf(stdout, "Insufficient memory to display response");
        return;
      }
      fprintf(stdout, msg);
      free(msg);
  }
  else {
    char* err_msg = init_strerror(payload->err);
    if(!err_msg) {
      fprintf(stdout, "Insufficient memory to display response");
      return;
    }
    fprintf(stdout, err_msg);
    free(err_msg);

    if(payload->err != ERR_OK) return;

    char* state_msg = init_strerror(payload->state);
    if(!state_msg) {
      fprintf(stdout, "Insufficient memory to display response");
      return;
    }

    fprintf(stdout, state_msg);
    free(state_msg);

    fprintf(stdout, "PID: %d\n", payload->pid);
  }
}

int main(int argc, const char* argv[]) {

  if(argc < 3) {
    fprintf(stdout, "Usage: <action> <service>\nactions: enable, disable, start, stop, status\n");
    return -1;
  }

  req_hdr header;
  size_t payload_size = (sizeof(int)) + strlen(argv[2]) + 1;
  req_payload* payload = malloc(payload_size);

  if(payload == NULL) {
    klog_ctl(FAIL, "failed to allocate memory for ctl payload: %s", strerror(errno));
    return -1;
  }

  if(strcmp(argv[1], "enable") == 0) {
    payload->action = A_ENABLE;
  }

  else if(strcmp(argv[1], "disable") == 0) {
    payload->action = A_DISABLE;
  }

  else if(strcmp(argv[1], "start") == 0) {
    payload->action = A_START;
  }


  else if(strcmp(argv[1], "stop") == 0) {
    payload->action = A_STOP;
  }

  else if(strcmp(argv[1], "state") == 0) {
    payload->action = A_STATE;
  }

  else {
    fprintf(stderr, "Unknow action : %s\n", argv[1]);
    free(payload);
    return -1;
  }

  header.euid = geteuid();
  header.payload_size = payload_size;

  struct iovec iov[2];

  iov[0].iov_base = &header;
  iov[0].iov_len = sizeof(header);

  strcpy(payload->sv_name, argv[2]);

  iov[1].iov_base = payload;
  iov[1].iov_len  = header.payload_size;

  int lfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  struct sockaddr_un addr  = { .sun_family = AF_UNIX, .sun_path = "/run/init.sock" };

  if (connect(lfd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {

    if( writev(lfd, iov , 2) < 0) {
      klog_ctl(FAIL, "failed to write to listener socket(fd=%d): %s\n", lfd, strerror(errno));
      free(payload);
      return -1;
    }
    res_payload rspayload;
    if( read(lfd, &rspayload, sizeof(res_payload)) < 0) {
      klog_ctl(FAIL, "failed to read from listener socket(fd=%d): strerror(errno)", lfd);
    }
    display_res_payload(&rspayload, payload->action);
  } else klog_ctl(FAIL, "failed to connect to listener socket(fd=%d): %s\n", lfd, strerror(errno));

  free(payload);
  close(lfd);
}
