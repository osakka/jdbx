int main() {
#include "core/server.h"
server_config_t config;
printf("log_file offset: %lu\n", (unsigned long)&config.log_file - (unsigned long)&config);
printf("pid_file offset: %lu\n", (unsigned long)&config.pid_file - (unsigned long)&config);
printf("verbose_mode offset: %lu\n", (unsigned long)&config.verbose_mode - (unsigned long)&config);
return 0;
}
