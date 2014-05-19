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
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <linux/fs.h>


/* recursive directory creation */
int mkdir_p(const char* path, mode_t mode)
{
        int result = 0;
        char* path_copy = NULL;
        int path_len = strlen(path);
        char* p1 = NULL;
        char* p2 = NULL;

        if (path == NULL || path[0] != '/')
        {
                fprintf(stderr, "ERROR: Absolute path is required\n");
                return -1;
        }

        path_copy = malloc(path_len + 1);
        strcpy(path_copy, path);

        p1 = p2 = path_copy + 1;
        while (p2 != path_copy + path_len)
        {
                struct stat st;

                p2 = strchr(p1, '/');
                if (p2 == 0)
                {
                        p2 = path_copy + path_len;
                }

                p2[0] = 0; /* replace '/' with end-of-line */

                if (stat(path_copy, &st) != 0)
                {
                        if (mkdir(path_copy, mode) != 0 && errno != EEXIST)
                        {
                                result = -1;
                                fprintf(stderr, "ERROR: Cannot create directory %s\n", path_copy);
                                break;
                        }
                }
                else if (!S_ISDIR(st.st_mode))
                {
                        result = -1;
                        fprintf(stderr, "ERROR: File %s exists\n", path_copy);
                        break;
                }

                p2[0] = '/'; /* return '/' */
                p1 = p2 + 1;
        }

        return result;
}

