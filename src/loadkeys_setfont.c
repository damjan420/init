#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

char *parse_vconsole(const char *target_key) {
    int fd = open("/etc/vconsole.conf", O_RDONLY);
    if (fd < 0) return NULL;

    char buf[1024];
    ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (bytes <= 0) return NULL;
    buf[bytes] = '\0';

    const char *line = strstr(buf, target_key);
    if (!line) return NULL;

    const char *value = strchr(line, '=');
    if (!value) return NULL;
    value++;

    if (*value == '"') value++;

    size_t len = strcspn(value, "\n\r\" ");

    char *result = malloc(len + 1);
    if (!result) return NULL;

    strncpy(result, value, len);
    result[len] = '\0';

    return result;
}

int main() {
  char* keymap = parse_vconsole("KEYMAP");
  char* font = parse_vconsole("FONT");

  if(keymap == NULL && font == NULL) return 0;

  if(keymap != NULL) {
    pid_t pid = fork();
    if(pid == 0) {
      char* args[] = {"loadkeys", keymap, "2>/dev/null"};
      execv(args[0], args);
    }
  }
  if(font != NULL) {
    pid_t pid = fork();
    if(pid == 0) {
      char* args[] = {"setfont", font, "2>/dev/null"};
      execv(args[0], args);
    }
  }
}
