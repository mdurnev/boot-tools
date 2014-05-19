#ifndef SPAWN_H
#define SPAWN_H

/* start a process */
pid_t start_process(char *const argv[]);

/* execute an application and return its exit code */
int run_app(char *const argv[]);

#endif

