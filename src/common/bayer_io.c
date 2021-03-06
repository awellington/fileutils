/*
 * Copyright (c) 2013-2015, Lawrence Livermore National Security, LLC.
 *   Produced at the Lawrence Livermore National Laboratory
 *   CODE-673838
 *
 * Copyright (c) 2006-2007,2011-2015, Los Alamos National Security, LLC.
 *   (LA-CC-06-077, LA-CC-10-066, LA-CC-14-046)
 *
 * Copyright (2013-2015) UT-Battelle, LLC under Contract No.
 *   DE-AC05-00OR22725 with the Department of Energy.
 *
 * Copyright (c) 2015, DataDirect Networks, Inc.
 * 
 * All rights reserved.
 *
 * This file is part of mpiFileUtils.
 * For details, see https://github.com/hpc/fileutils.
 * Please also read the LICENSE file.
*/

#include "bayer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BAYER_IO_TRIES  (5)
#define BAYER_IO_USLEEP (100)

/* calls access, and retries a few times if we get EIO or EINTR */
int bayer_access(const char* path, int amode)
{
    int rc;
    int tries = BAYER_IO_TRIES;
retry:
    rc = access(path, amode);
    if (rc != 0) {
        if (errno == EINTR || errno == EIO) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return rc;
}

/* calls lchown, and retries a few times if we get EIO or EINTR */
int bayer_lchown(const char* path, uid_t owner, gid_t group)
{
    int rc;
    int tries = BAYER_IO_TRIES;
retry:
    rc = lchown(path, owner, group);
    if (rc != 0) {
        if (errno == EINTR || errno == EIO) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return rc;
}

/* calls chmod, and retries a few times if we get EIO or EINTR */
int bayer_chmod(const char* path, mode_t mode)
{
    int rc;
    int tries = BAYER_IO_TRIES;
retry:
    rc = chmod(path, mode);
    if (rc != 0) {
        if (errno == EINTR || errno == EIO) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return rc;
}

/* calls lstat, and retries a few times if we get EIO or EINTR */
int bayer_lstat(const char* path, struct stat* buf)
{
    int rc;
    int tries = BAYER_IO_TRIES;
retry:
    rc = lstat(path, buf);
    if (rc != 0) {
        if (errno == EINTR || errno == EIO) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return rc;
}

/* calls lstat64, and retries a few times if we get EIO or EINTR */
int bayer_lstat64(const char* path, struct stat64* buf)
{
    int rc;
    int tries = BAYER_IO_TRIES;
retry:
    rc = lstat64(path, buf);
    if (rc != 0) {
        if (errno == EINTR || errno == EIO) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return rc;
}

/* call mknod, retry a few times on EINTR or EIO */
int bayer_mknod(const char* path, mode_t mode, dev_t dev)
{
    int rc;
    int tries = BAYER_IO_TRIES;
retry:
    rc = mknod(path, mode, dev);
    if (rc < 0) {
        if (errno == EINTR || errno == EIO) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return rc;
}

/* call remove, retry a few times on EINTR or EIO */
int bayer_remove(const char* path)
{
    int rc;
    int tries = BAYER_IO_TRIES;
retry:
    rc = remove(path);
    if (rc < 0) {
        if (errno == EINTR || errno == EIO) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return rc;
}

/*****************************
 * Links
 ****************************/

/* call readlink, retry a few times on EINTR or EIO */
ssize_t bayer_readlink(const char* path, char* buf, size_t bufsize)
{
    ssize_t rc;
    int tries = BAYER_IO_TRIES;
retry:
    rc = readlink(path, buf, bufsize);
    if (rc < 0) {
        if (errno == EINTR || errno == EIO) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return rc;
}

/* call symlink, retry a few times on EINTR or EIO */
int bayer_symlink(const char* oldpath, const char* newpath)
{
    int rc;
    int tries = BAYER_IO_TRIES;
retry:
    rc = symlink(oldpath, newpath);
    if (rc < 0) {
        if (errno == EINTR || errno == EIO) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return rc;
}

/*****************************
 * Files
 ****************************/

/* open file with specified flags and mode, retry open a few times on failure */
int bayer_open(const char* file, int flags, ...)
{
    /* extract the mode (see man 2 open) */
    int mode_set = 0;
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
        mode_set = 1;
    }

    /* attempt to open file */
    int fd = -1;
    errno = 0;
    if (mode_set) {
        fd = open(file, flags, mode);
    }
    else {
        fd = open(file, flags);
    }

    /* if open failed, try a few more times */
    if (fd < 0) {
        /* try again */
        int tries = BAYER_IO_TRIES;
        while (tries && fd < 0) {
            /* sleep a bit before consecutive tries */
            usleep(BAYER_IO_USLEEP);

            /* open again */
            errno = 0;
            if (mode_set) {
                fd = open(file, flags, mode);
            }
            else {
                fd = open(file, flags);
            }
            tries--;
        }

        /* if we still don't have a valid file, consider it an error */
        if (fd < 0) {
            /* we could abort, but probably don't want to here */
        }
    }

    return fd;
}

/* close file */
int bayer_close(const char* file, int fd)
{
    int tries = BAYER_IO_TRIES;
retry:
    errno = 0;
    int rc = close(fd);
    if (rc != 0) {
        if (errno == EINTR || errno == EIO) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return rc;
}

/* seek file descriptor to specified position */
off_t bayer_lseek(const char* file, int fd, off_t pos, int whence)
{
    int tries = BAYER_IO_TRIES;
retry:
    errno = 0;
    off_t rc = lseek(fd, pos, whence);
    if (rc == (off_t)-1) {
        if (errno == EINTR || errno == EIO) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return rc;
}

/* reliable read from file descriptor (retries, if necessary, until hard error) */
ssize_t bayer_read(const char* file, int fd, void* buf, size_t size)
{
    int tries = BAYER_IO_TRIES;
    ssize_t n = 0;
    while ((size_t)n < size) {
        ssize_t rc = read(fd, (char*) buf + n, size - (size_t)n);
        if (rc > 0) {
            /* read some data */
            n += rc;
            tries = BAYER_IO_TRIES;
        }
        else if (rc == 0) {
            /* EOF */
            return n;
        }
        else {   /* (rc < 0) */
            /* something worth printing an error about */
            tries--;
            if (tries <= 0) {
                /* too many failed retries, give up */
                BAYER_ABORT(-1, "Failed to read file %s errno=%d (%s)",
                            file, errno, strerror(errno)
                           );
            }

            /* sleep a bit before consecutive tries */
            usleep(BAYER_IO_USLEEP);
        }
    }
    return n;
}

/* reliable write to file descriptor (retries, if necessary, until hard error) */
ssize_t bayer_write(const char* file, int fd, const void* buf, size_t size)
{
    int tries = 10;
    ssize_t n = 0;
    while ((size_t)n < size) {
        ssize_t rc = write(fd, (const char*) buf + n, size - (size_t)n);
        if (rc > 0) {
            /* wrote some data */
            n += rc;
            tries = BAYER_IO_TRIES;
        }
        else if (rc == 0) {
            /* something bad happened, print an error and abort */
            BAYER_ABORT(-1, "Failed to write file %s errno=%d (%s)",
                        file, errno, strerror(errno)
                       );
        }
        else {   /* (rc < 0) */
            /* something worth printing an error about */
            tries--;
            if (tries <= 0) {
                /* too many failed retries, give up */
                BAYER_ABORT(-1, "Failed to write file %s errno=%d (%s)",
                            file, errno, strerror(errno)
                           );
            }

            /* sleep a bit before consecutive tries */
            usleep(BAYER_IO_USLEEP);
        }
    }
    return n;
}

/* delete a file */
int bayer_unlink(const char* file)
{
    int rc;
    int tries = BAYER_IO_TRIES;
retry:
    rc = unlink(file);
    if (rc != 0) {
        if (errno == EINTR || errno == EIO) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return rc;
}

/* force flush of written data */
int bayer_fsync(const char* file, int fd)
{
    int rc;
    int tries = BAYER_IO_TRIES;
retry:
    rc = fsync(fd);
    if (rc < 0) {
        if (errno == EINTR || errno == EIO) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return rc;
}

/*****************************
 * Directories
 ****************************/

/* get current working directory, abort if fail or buffer too small */
void bayer_getcwd(char* buf, size_t size)
{
    char* p = getcwd(buf, size);
    if (p == NULL) {
        BAYER_ABORT(-1, "Failed to get current working directory errno=%d (%s)",
                    errno, strerror(errno)
                   );
    }
}

/* create directory, retry a few times on EINTR or EIO */
int bayer_mkdir(const char* dir, mode_t mode)
{
    int rc;
    int tries = BAYER_IO_TRIES;
retry:
    rc = mkdir(dir, mode);
    if (rc < 0) {
        if (errno == EINTR || errno == EIO) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return rc;
}

/* remove directory, retry a few times on EINTR or EIO */
int bayer_rmdir(const char* dir)
{
    int rc;
    int tries = BAYER_IO_TRIES;
retry:
    rc = rmdir(dir);
    if (rc < 0) {
        if (errno == EINTR || errno == EIO) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return rc;
}

/* open directory, retry a few times on EINTR or EIO */
DIR* bayer_opendir(const char* dir)
{
    DIR* dirp;
    int tries = BAYER_IO_TRIES;
retry:
    dirp = opendir(dir);
    if (dirp == NULL) {
        if (errno == EINTR || errno == EIO) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return dirp;
}

/* close directory, retry a few times on EINTR or EIO */
int bayer_closedir(DIR* dirp)
{
    int rc;
    int tries = BAYER_IO_TRIES;
retry:
    rc = closedir(dirp);
    if (rc < 0) {
        if (errno == EINTR || errno == EIO) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return rc;
}

/* read directory entry, retry a few times on ENOENT, EIO, or EINTR */
struct dirent* bayer_readdir(DIR* dirp)
{
    /* read next directory entry, retry a few times */
    struct dirent* entry;
    int tries = BAYER_IO_TRIES;
retry:
    entry = readdir(dirp);
    if (entry == NULL) {
        if (errno == EINTR || errno == EIO || errno == ENOENT) {
            tries--;
            if (tries > 0) {
                /* sleep a bit before consecutive tries */
                usleep(BAYER_IO_USLEEP);
                goto retry;
            }
        }
    }
    return entry;
}
