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
  int services_dir_fd = open(CGROUP_PATH, O_DIRECTORY);
  if(services_dir_fd < 0) {
    klog(FAIL, "failed to open /sys/fs/cgroup/init/services: %s", strerror(errno));
    return;
  }

  mode_t mode = S_IRWXU | S_IRWXG | S_IROTH;
  if(mkdirat(services_dir_fd,sv->name, mode) < 0) {
    if(errno == EEXIST) {
      close(services_dir_fd);
      return;
    };
    klog(FAIL, "failed to mkdir %s/%s: %s", CGROUP_PATH,sv->name, strerror(errno));
  }

  int sv_dir_fd = openat(services_dir_fd,sv->name, O_DIRECTORY);

  if(sv_dir_fd < 0) {
    close(services_dir_fd);
    klog(FAIL, "failed to open dir %s/%s: %s", CGROUP_PATH, sv->name,strerror(errno));
    return;
  }

  int procs_fd = openat(sv_dir_fd, "cgroup.procs", O_WRONLY);
  if(procs_fd < 0) {
    klog(FAIL, "failed to open %s/%s/cgroup.procs: %s",CGROUP_PATH,sv->name, strerror(errno));
  }
  if(dprintf(procs_fd, "%d", sv->pid) < 0) {
    klog(FAIL, "failed to write to %s/%s/cgroup.procs: %s", CGROUP_PATH,sv->name, strerror(errno));
  }
  close(procs_fd);

  int pids_fd = openat(sv_dir_fd, "pids.max", O_WRONLY);
  if(pids_fd < 0) {
    klog(FAIL, "failed to open %s/%s/pids.max: %s",CGROUP_PATH,sv->name, strerror(errno));
  }
  if(sv->pids_max == -1) {
    if(dprintf(pids_fd, "max") < 0) {
      klog(FAIL, "failed to write to %s/%s/pids.max: %s", CGROUP_PATH,sv->name, strerror(errno));
    }
  } else {

    if(dprintf(pids_fd, "%ld", sv->pids_max) < 0) {
      klog(FAIL, "failed to write to %s/%s/pids.max: %s", CGROUP_PATH,sv->name, strerror(errno));
    }
  }
  close(pids_fd);

  int memory_fd = openat(sv_dir_fd, "memory.max", O_WRONLY);
  if(memory_fd < 0) {
    klog(FAIL, "failed to open %s/%s/memory.max: %s",CGROUP_PATH,sv->name, strerror(errno));
  }
  if(sv->memory_max == -1) {
    if(dprintf(memory_fd, "max") < 0) {
      klog(FAIL, "failed to write to %s/%s/memory.max: %s", CGROUP_PATH,sv->name, strerror(errno));
    }
  } else {

    if(dprintf(memory_fd, "%ld", sv->memory_max) < 0) {
      klog(FAIL, "failed to write to %s/%s/memory.max: %s", CGROUP_PATH,sv->name, strerror(errno));
    }
  }
  close(memory_fd);

  int cpu_fd = openat(sv_dir_fd, "cpu.max", O_WRONLY);
  if(cpu_fd < 0) {
    klog(FAIL, "failed to open %s/%s/cpu.max: %s",CGROUP_PATH,sv->name, strerror(errno));
  }
  if(sv->cpu_quota == -1) {
    if(dprintf(cpu_fd, "max %ld", sv->cpu_period) < 0) {
      klog(FAIL, "failed to write to %s/%s/cpu.max: %s", CGROUP_PATH,sv->name, strerror(errno));
    }
  } else {

    if(dprintf(cpu_fd, "%ld %ld", sv->cpu_quota, sv->cpu_period) < 0) {
      klog(FAIL, "failed to write to %s/%s/cpu.max: %s", CGROUP_PATH,sv->name, strerror(errno));
    }
  }
  close(services_dir_fd);
  close(sv_dir_fd);
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
