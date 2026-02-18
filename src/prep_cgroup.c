#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mount.h>

#include "klog.h"

int main() {
  mode_t mode = S_IRWXU | S_IRWXG | S_IROTH;

  if(mkdir("/sys/fs/cgroup", mode) < 0) {
    klog(FAIL, "failed to mkdir /sys/fs/cgroup: %s", strerror(errno));
  } else klog(OK, "made dir /sys/fs/cgroup");

  if(mount("cgroup2", "/sys/fs/cgroup", "cgroup2", MS_NOSUID | MS_NOEXEC | MS_NODEV, NULL) == -1) {
    klog(FAIL, "failed to mount /sys/fs/cgroup: %s", strerror(errno));
  } else klog(OK, "mounted /sys/fs/cgroup");


  int root_ctrls_fd = open("/sys/fs/cgroup/cgroup.subtree_control", O_WRONLY);
  char controllers[] = "+pids +memory +cpu"; //TODO: maybe parse controllers?

  if(write(root_ctrls_fd, controllers, sizeof(controllers)) < 0) {
    klog(FAIL, "failed to write to /sys/fs/cgroup/cgroup.subtree_control: %s\n", strerror(errno));
  } else klog(OK, "enabled cgroup controllers at root: %s", controllers);
  close(root_ctrls_fd);

  if(mkdir("/sys/fs/cgroup/init", mode) < 0) {
    klog(FAIL, "failed to crate init cgroup: %s", strerror(errno));
  } else klog(OK, "created init cgroup");

  int init_ctrls_fd = open("/sys/fs/cgroup/init/cgroup.subtree_control", O_WRONLY);
  if (write(init_ctrls_fd, controllers, sizeof(controllers)) < 0) {
    klog(FAIL, "failed to write to /sys/fs/cgroup/init/cgroup.subtree_control: %s", strerror(errno));
  } else klog(OK, "enabled cgroup controllers at init: %s", controllers);
  close(init_ctrls_fd);

  if (mkdir("/sys/fs/cgroup/init/self", mode) < 0) {
    klog(FAIL, "failed to mkdir /sys/fs/cgroup/init/self: %s", strerror(errno));
  } else klog(OK, "made dir /sys/fs/cgroup/init/self");

  int init_self_fd = open("/sys/fs/cgroup/init/self/cgroup.procs", O_WRONLY);
  if( dprintf(init_self_fd, "1") < 0) {
   klog(FAIL, "failed to write to /sys/fs/cgroup/init/selfcgroup.procs: %s", strerror);
  } else klog(OK, "set init/self/cgroup.procs to 1");
  close(init_self_fd);

  if(mkdir("/sys/fs/cgroup/init/services/", mode) < 0) {
    klog(FAIL, "failed to mkdir /sys/fs/cgroup/init/services: %s", strerror(errno));
  } else klog(OK, "made dir /sys/fs/init/cgroup/services");


  int services_ctrls_fd = open("/sys/fs/cgroup/init/services/cgroup.subtree_control", O_WRONLY);
  if (write(services_ctrls_fd, controllers, sizeof(controllers)) < 0) {
    klog(FAIL, "failed to write to /sys/fs/cgroup/init/cgroup.subtree_control: %s", strerror(errno));
  } else klog(OK, "enabled cgroup controllers at init/services: %s", controllers);
  close(services_ctrls_fd);

  return 0;
}
