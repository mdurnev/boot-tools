/*
 * Start LTTng session before systemd
 *
 * "presd" means "started before systemd"
 *
 * 1. Cross-compile the tool
 * 2. Place the executable somewere on the target (e.g. /home/root/presd)
 * 3. Add init option to your kernel command line (e.g. init=/home/root/presd)
 *
 * Some parts of code were taken from systemd mount-setup.c, which is LGPL2.1
 * Copyright 2010 Lennart Poettering
 *
 * Copyright 2014 Mentor Graphics Corp.
 * Copyright 2014 Mikhail Durnev
 *
 * mikhail_durnev@mentor.com
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <linux/fs.h>
#include <time.h>

#include "../common/spawn.h"
#include "../common/mkdir.h"

/* init */
char arg_init_path[PATH_MAX] = "/lib/systemd/systemd";


/* file systems to be mounted */
typedef struct MountPoint {
        const char *what;
        const char *where;
        const char *type;
        const char *options;
        unsigned long flags;
} MountPoint;

static const MountPoint mount_table[] = {
        { "proc",       "/proc",                     "proc",       NULL, MS_NOSUID|MS_NOEXEC|MS_NODEV },
        { "sysfs",      "/sys",                      "sysfs",      NULL, MS_NOSUID|MS_NOEXEC|MS_NODEV },
        { "devtmpfs",   "/dev",                      "devtmpfs",   "mode=755", MS_NOSUID|MS_STRICTATIME },
        { "tmpfs",      "/dev/shm",                  "tmpfs",      "mode=1777", MS_NOSUID|MS_NODEV|MS_STRICTATIME },
        { "tmpfs",      "/run",                      "tmpfs",      "mode=755", MS_NOSUID|MS_NODEV|MS_STRICTATIME }
};
#define MOUNT_TABLE_SIZE (sizeof(mount_table) / sizeof(MountPoint))


/* kernel modules to be loaded */
static const char* module_table[] = {
};
#define MODULE_TABLE_SIZE (sizeof(module_table) / sizeof(char*))


/* mounts all file systems from mount_table */
void mount_all()
{
        int i;

        for (i = 0; i < MOUNT_TABLE_SIZE; i++)
        {
                MountPoint* p = mount_table + i;

                mkdir_p(p->where, 0755);

                if (mount(p->what, p->where, p->type, p->flags, p->options) < 0)
                {
                    fprintf(stderr, "WARNING: Cannot mount %s\n", p->where);
                }
        }
}


/* loads all kernel modules from module_table */
void load_all()
{
        int i;

        for (i = 0; i < MODULE_TABLE_SIZE; i++)
        {
                char* p = module_table[i];

                char *const argv[] = {"/sbin/modprobe", p, NULL};

                pid_t pid = start_process(argv);

                if (pid > 0)
                {
                        /* wait for the child */
                        int status;
                        if (waitpid(pid, &status, 0) != pid ||
                            !WIFEXITED(status) || WEXITSTATUS(status) != 0)
                        {
                                fprintf(stderr, "WARNING: Cannot load kernel module %s\n", p);
                        }
                }
                else
                {
                        exit(2);
                }
        }
}




/* program entry point */
int main()
{
        pid_t child;

        printf("\nNOTE: Prepare for systemd launch\n");

        if (getpid() != 1)
        {
                fprintf(stderr, "ERROR: It must be a PID 1 process\n");
                exit(2);
        }

        child = fork();
        if (child < 0)
        {
                fprintf(stderr, "ERROR: Cannot fork\n");
                exit(2);
        }

        if (child > 0)
        {
                /* parent */

                /* wait for child */
                int status;

                while(1)
                {
                        if (waitpid(child, &status, 0) != child ||
                            (WIFEXITED(status) && WEXITSTATUS(status) != 0))
                        {
                                fprintf(stderr, "NOTE: %s was not launched\n", arg_init_path);
                                break;
                        }

                        if (!WIFEXITED(status))
                        {
                                continue;
                        }

                        /* give some time to perf */
                        //sleep(1);
                        struct timespec sleeptime = {0, 600000000}; // 600ms
                        nanosleep(&sleeptime, NULL);

                        /* start init as PID 1 */

                        printf("NOTE: Launching systemd...\n");
                        char *const argv_init[] = {arg_init_path, NULL};
                        char *const envp[] = {NULL};

                        execve(argv_init[0], argv_init, envp);

                        fprintf(stderr, "ERROR: Launch failed\n");
                        break;
                }

                exit(2);
        }

        /* prepare file systems */
        printf("NOTE: Mounting file systems...\n");
        mount_all();

        /* load kernel modules */
        printf("NOTE: Loading kernel modules...\n");
        load_all();

        /* let the kernel to run all deferred initcalls (to enable tracepoint events) */
        printf("NOTE: Execute kernel deferred initcalls...\n");
        char *const argv_init[] = {"/bin/cat", "/proc/deferred_initcalls", NULL};
        if (run_app(argv_init) != 0)
        {
                fprintf(stderr, "ERROR: Cannot run kernel deferred initcalls\n");
                //exit(2);
        }

        /* start perf */
        char *const argv_perf[] = {"/usr/bin/perf", "record", 
                                   //"-e", "cycles",
                                    //"-e", "sched:sched_stat*", 
                                    //"-e", "sched:sched_switch",  
                                    //"-e", "sched:sched_process_exit", 
                                   "-e", "sched:sched*",
                                   "-e", "module:module*",
                                   "-g", /* call backtrace */
                                   "-a", /* all processes */
                                   "-c", "1", /* per event sampling */
                                   "-o", "/home/root/perf.data.raw", "sleep", "3", NULL};

        printf("NOTE: Starting perf...\n");
        if (start_process(argv_perf) <= 0)
        {
                fprintf(stderr, "ERROR: Cannot start perf\n");
                exit(2);
        }

        exit(0);
}

