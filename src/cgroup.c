#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <linux/limits.h>

#include "klog.h"
#include "sv.h"

#define CGROUP_PATH "/sys/fs/cgroup/init/services"

void cg_start(service* sv) {

  char path[PATH_MAX];
  int offset = snprintf(path, sizeof(path), "%s/%s", CGROUP_PATH, sv->name);

  mode_t mode = S_IRWXU | S_IRWXG | S_IROTH;
  if(mkdir(path, mode) < 0 && errno != EEXIST) {
    klog(FAIL, "failed to mkdir %s: %s", path, strerror(errno));
  }

  snprintf(path + offset, sizeof(path) - offset, "/cgroup.procs");
  int procs_fd = open(path, O_WRONLY);

  if(procs_fd < 0) {
    klog(FAIL, "failed to open %s: %s", path, strerror(errno));
  }

  if(dprintf(procs_fd, "%d", sv->pid) < 0) {
    klog(FAIL, "failed to write to %s: %s", path, strerror(errno));
  }

}

void cg_kill(const service* sv) {
  char path[PATH_MAX];
  snprintf(path, sizeof(path), "%s/%s/cgroup.kill", CGROUP_PATH, sv->name);

  int kill_fd = open(path, O_WRONLY);
  if(kill_fd < 0) {
    klog(FAIL, "failed toopen %s: %s", path, strerror(errno));
  }
  if(dprintf(kill_fd, "1") < 0) {
    klog(FAIL, "failed to write to %s: %s", path, strerror(errno));
  }
}
