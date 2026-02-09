#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <wait.h>

void phase_one() {
  pid_t pid = getpid();
  if(pid != 1) return;

  pid = fork();

  if(pid == 0) {
    char *args[] = { "/bin/init.d/core_services/mount_pfs",  NULL };
    execv(args[0], args);
    fprintf(stderr, "[ FAIL ] Could not launch core service mount_pfs: %s\n", strerror(errno));
    _exit(-1);
  }

  pid = fork();
  if(pid == 0) {
    char *args[] = { "/bin/init.d/core_services/up_lo",  NULL };
    execv(args[0], args);
    fprintf(stderr, "[ FAIL ] Could not launch core service up_lo: %s\n", strerror(errno));
    _exit(-1);
  }

  pid = fork();
  if(pid == 0) {
    char *args[] = { "/bin/init.d/core_services/set_hostname",  NULL };
    execv(args[0], args);
    fprintf(stderr, "[ FAIL ] Could not launch core service set_hostname: %s\n", strerror(errno));
    _exit(-1);
  }


  pid = fork();
  if(pid == 0) { ///bin/init.d/core_services
    char *args[] = { "/bin/init.d/core_services/seed_entropy",  NULL };
    execv(args[0], args);
    fprintf(stderr, "[ FAIL ] Could not launch core service seed_entropy: %s\n", strerror(errno));
    _exit(-1);
  }
  while (wait(NULL) > 0);
}
