
struct service;
typedef struct service service;

void cg_start(service* sv);
void cg_kill(const service* sv);
