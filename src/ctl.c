#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include "ctl.h"
#include "sv.h"

char* init_strerror(int err) {
  // TODO: improve error messages
  const char* msg;
  switch (err){
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
  case ERR_UNSUPORTED:
    msg = "Requested action not supported\n";
    break;
  }
  char* ret = strdup(msg);
  return ret;
}
int main(int argc, const char* argv[]) {

  if(argc < 3) {
    fprintf(stdout, "Usage: <action> <service>\n actions: enable, disable\n");
    return -1;
  }

  ctl_payload payload;
  if(strcmp(argv[1], "enable") == 0) {
    payload.action = A_ENABLE;
  }

  else if(strcmp(argv[1], "disable") == 0) {
    payload.action = A_DISABLE;
  }

  else if(strcmp(argv[1], "start") == 0) {
    payload.action = A_START;
  }


  else if(strcmp(argv[1], "stop") == 0) {
    payload.action = A_STOP;
  }


  else {
    fprintf(stderr, "Unknow action : %s\n", argv[1]);
    return -1;
  }

  payload.euid = geteuid();
  strcpy(payload.sv_name, argv[2]);

  int cfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  struct sockaddr_un addr  = { .sun_family = AF_UNIX, .sun_path = "/run/init.sock" };
  if (connect(cfd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {

    if( write(cfd, &payload, sizeof(payload)) < 0) fprintf(stderr, "[ FAIL ] Unable to write to fd: %s\n", strerror(errno));
    int err;
    read(cfd, &err, sizeof(int));
    char* msg = init_strerror(err);
    fprintf(stdout, "%s", msg);
    free(msg);
  } else fprintf(stderr, "[ FAIL ] unable to connect to the socket: %s\n", strerror(errno));
  close(cfd);
}
