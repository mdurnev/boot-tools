/*
 * Copyright 2013 Mentor Graphics Corp.
 * Copyright 2013 Mikhail Durnev
 *
 * mikhail_durnev@mentor.com
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


/* start a process */
pid_t start_process(char *const argv[])
{
        pid_t pid = fork();

        if (pid < 0)
        {
                fprintf(stderr, "ERROR: Cannot fork\n");
                return pid;
        }

        if (pid == 0)
        {
                /* child */

                char *const envp[] = {"HOME=/home/root", NULL};

                execve(argv[0], argv, envp);

                fprintf(stderr, "WARNING: execve() failed\n");
                exit(2);
        }
        else
        {
                return pid;
        }
}


/* execute an application and return its exit code */
int run_app(char *const argv[])
{
        pid_t pid = start_process(argv);

        if (pid > 0)
        {
                /* wait for the child */
                int status;
                if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status))
                {
                        fprintf(stderr, "WARNING: Application %s did not return exit code\n", argv[0]);
                        return 0xff;
                }
                return WEXITSTATUS(status);
        }
        else
        {
                exit(2);
        }
}

