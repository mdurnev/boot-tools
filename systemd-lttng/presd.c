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
        "lttng-tracer"
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

        
        /* start lttng daemon */
        char *const argv_lttng_sessiond[] = {"/usr/bin/lttng-sessiond", "--quiet", NULL};

        printf("NOTE: Starting LTTng daemon...\n");
        if (start_process(argv_lttng_sessiond) <= 0)
        {
                fprintf(stderr, "ERROR: Cannot start lttng-sessiond\n");
                exit(2);
        }

        /* wait for the daemon */
        sleep(1);


        /* create lttng session */
        printf("NOTE: Creating LTTng session...\n");
        char *const argv_create[] = {"/usr/bin/lttng", "-n", "create", "init-session", NULL};
        if (run_app(argv_create) != 0)
        {
                fprintf(stderr, "ERROR: Cannot create lnttg session\n");
                exit(2);
        }

        /* enable all kernel events */
        printf("NOTE: Configuring LTTng session...\n");
        char *const argv_events[] = {"/usr/bin/lttng", "-n", "enable-event", "-u", "-a", "-s", "init-session", NULL};
        if (run_app(argv_events) != 0)
        {
                fprintf(stderr, "ERROR: Cannot enable events\n");
                exit(2);
        }
        
        /* start log capture */
        printf("NOTE: Starting log capture...\n");
        char *const argv_start[] = {"/usr/bin/lttng", "-n", "start", "init-session", NULL};
        if (run_app(argv_start) != 0)
        {
                fprintf(stderr, "ERROR: Cannot start capturing logs\n");
                exit(2);
        }

        if (fork() != 0)
        {
                /* parent */

                /* stop this process to proceed with PID 1 */
                printf("NOTE: Ready to launch systemd\n");
                exit(0);
        }
        else
        {
                /* wait 1 minute */
                sleep(60);

                /* and stop the lttng session */
                printf("NOTE: Stopping log capture...\n");
                char *const argv_stop[] = {"/usr/bin/lttng", "-n", "stop", "init-session", NULL};
                exit(run_app(argv_stop));
        }
}

