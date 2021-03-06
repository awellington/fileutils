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

/*
 * For an overview of how argument handling was originally designed in dcp,
 * please see the blog post at:
 *
 * <http://www.bringhurst.org/2012/12/16/file-copy-tool-argument-handling.html>
 */

#include "handle_args.h"
#include "dcp.h"
#include "bayer.h"

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/** Where we should store options specified by the user. */
DCOPY_options_t DCOPY_user_opts;

static int num_src_params;
static bayer_param_path* src_params;
static bayer_param_path  dest_param;

/**
 * Determine if the destination path is a file or directory.
 *
 * It does this by first checking to see if an object is actually at the
 * destination path. If an object isn't already at the destination path, we
 * examine the source path(s) to determine the type of what the destination
 * path will be.
 *
 * @return true if the destination should be a directory, false otherwise.
 */

/**
 * Analyze all file path inputs and place on the work queue.
 *
 * We start off with all of the following potential options in mind and prune
 * them until we figure out what situation we have.
 *
 * Libcircle only calls this function from rank 0, so there's no need to check
 * the current rank here.
 *
 * Source must overwrite destination.
 *   - Single file to single file
 *
 * Must return an error. Impossible condition.
 *   - Single directory to single file
 *   - Many file to single file
 *   - Many directory to single file
 *   - Many directory and many file to single file
 *
 * All Sources must be placed inside destination.
 *   - Single file to single directory
 *   - Single directory to single directory
 *   - Many file to single directory
 *   - Many directory to single directory
 *   - Many file and many directory to single directory
 */

/* check that source and destination paths are valid */
static void DCOPY_check_paths(void)
{
    /* assume path parameters are valid */
    int valid = 1;

    /* just have rank 0 check */
    if(DCOPY_global_rank == 0) {
        /* count number of readable source paths */
        int i;
        int num_readable = 0;
        for(i = 0; i < num_src_params; i++) {
            char* path = src_params[i].path;
            if(bayer_access(path, R_OK) == 0) {
                num_readable++;
            }
            else {
                /* found a source path that we can't read, not fatal,
                 * but print an error to notify user */
                char* orig = src_params[i].orig;
                BAYER_LOG(BAYER_LOG_ERR, "Could not read `%s' errno=%d %s",
                    orig, errno, strerror(errno));
            }
        }

        /* verify that we have at least one source path */
        if(num_readable < 1) {
            BAYER_LOG(BAYER_LOG_ERR, "At least one valid source must be specified");
            valid = 0;
            goto bcast;
        }

        /*
         * First we need to determine if the last argument is a file or a directory.
         * We first attempt to see if the last argument already exists on disk. If it
         * doesn't, we then look at the sources to see if we can determine what the
         * last argument should be.
         */

        bool dest_exists = false;
        bool dest_is_dir = false;
        bool dest_is_file = false;
        bool dest_is_link_to_dir = false;
        bool dest_is_link_to_file = false;
        bool dest_required_to_be_dir = false;

        /* check whether dest exists, its type, and whether it's writable */
        if(dest_param.path_stat_valid) {
            /* we could stat dest path, so something is there */
            dest_exists = true;

            /* now determine its type */
            if(S_ISDIR(dest_param.path_stat.st_mode)) {
                /* dest is a directory */
                dest_is_dir  = true;
            }
            else if(S_ISREG(dest_param.path_stat.st_mode)) {
                /* dest is a file */
                dest_is_file = true;
            }
            else if(S_ISLNK(dest_param.path_stat.st_mode)) {
                /* dest is a symlink, but to what? */
                if (dest_param.target_stat_valid) {
                    /* target of the symlink exists, determine what it is */
                    if(S_ISDIR(dest_param.target_stat.st_mode)) {
                        /* dest is link to a directory */
                        dest_is_link_to_dir = true;
                    }
                    else if(S_ISREG(dest_param.target_stat.st_mode)) {
                        /* dest is link to a file */
                        dest_is_link_to_file = true;
                    }
                    else {
                        /* unsupported type */
                        BAYER_LOG(BAYER_LOG_ERR, "Unsupported filetype `%s' --> `%s'",
                            dest_param.orig, dest_param.target);
                        valid = 0;
                        goto bcast;
                    }
                }
                else {
                    /* dest is a link, but its target does not exist,
                     * consider this an error */
                    BAYER_LOG(BAYER_LOG_ERR, "Destination is broken symlink `%s'",
                        dest_param.orig);
                    valid = 0;
                    goto bcast;
                }
            }
            else {
                /* unsupported type */
                BAYER_LOG(BAYER_LOG_ERR, "Unsupported filetype `%s'",
                    dest_param.orig);
                valid = 0;
                goto bcast;
            }

            /* check that dest is writable */
            if(bayer_access(dest_param.path, W_OK) < 0) {
                BAYER_LOG(BAYER_LOG_ERR, "Destination is not writable `%s'",
                    dest_param.path);
                valid = 0;
                goto bcast;
            }
        }
        else {
            /* destination does not exist, so we'll be creating it,
             * check that its parent is writable */

            /* compute parent path */
            bayer_path* parent = bayer_path_from_str(dest_param.path);
            bayer_path_dirname(parent);
            char* parent_str = bayer_path_strdup(parent);
            bayer_path_delete(&parent);

            /* check that parent is writable */
            if(bayer_access(parent_str, W_OK) < 0) {
                BAYER_LOG(BAYER_LOG_ERR, "Destination parent directory is not writable `%s'",
                    parent_str);
                valid = 0;
                bayer_free(&parent_str);
                goto bcast;
            }
            bayer_free(&parent_str);
        }

        /* determine whether caller *requires* copy into dir */

        /* TODO: if caller specifies dest/ or dest/. */

        /* if caller specifies more than one source,
         * then dest has to be a directory */
        if(num_src_params > 1) {
            dest_required_to_be_dir = true;
        }

        /* if caller requires dest to be a directory, and if dest does not
         * exist or it does it exist but it's not a directory, then abort */
        if(dest_required_to_be_dir &&
           (!dest_exists || (!dest_is_dir && !dest_is_link_to_dir)))
        {
            BAYER_LOG(BAYER_LOG_ERR, "Destination is not a directory `%s'",
                dest_param.orig);
            valid = 0;
            goto bcast;
        }

        /* we copy into a directory if any of the following:
         *   1) user specified more than one source
         *   2) destination already exists and is a directory
         *   3) destination already exists and is a link to a directory */
        bool copy_into_dir = (dest_required_to_be_dir || dest_is_dir || dest_is_link_to_dir);
        DCOPY_user_opts.copy_into_dir = copy_into_dir ? 1 : 0;
    }

    /* get status from rank 0 */
bcast:
    MPI_Bcast(&valid, 1, MPI_INT, 0, MPI_COMM_WORLD);

    /* exit job if we found a problem */
    if(! valid) {
        if(DCOPY_global_rank == 0) {
            BAYER_LOG(BAYER_LOG_ERR, "Exiting run");
        }
        MPI_Barrier(MPI_COMM_WORLD);
        DCOPY_exit(EXIT_FAILURE);
    }

    /* rank 0 broadcasts whether we're copying into a directory */
    MPI_Bcast(&DCOPY_user_opts.copy_into_dir, 1, MPI_INT, 0, MPI_COMM_WORLD);
}

/**
 * Parse the source and destination paths that the user has provided.
 */
void DCOPY_parse_path_args(char** argv, \
                           int optind_local, \
                           int argc)
{
    /* compute number of paths and index of last argument */
    int num_args = argc - optind_local;
    int last_arg_index = num_args + optind_local - 1;

    /* we need to have at least two paths,
     * one or more sources and one destination */
    if(argv == NULL || num_args < 2) {
        if(DCOPY_global_rank == 0) {
            DCOPY_print_usage();
            BAYER_LOG(BAYER_LOG_ERR, "You must specify a source and destination path");
        }

        MPI_Barrier(MPI_COMM_WORLD);
        DCOPY_exit(EXIT_FAILURE);
    }

    /* determine number of source paths */
    src_params = NULL;
    num_src_params = last_arg_index - optind_local;

    /* allocate space to record info about each source */
    size_t src_params_bytes = ((size_t) num_src_params) * sizeof(bayer_param_path);
    src_params = (bayer_param_path*) BAYER_MALLOC(src_params_bytes);

    /* record standardized paths and stat info for each source */
    int opt_index;
    for(opt_index = optind_local; opt_index < last_arg_index; opt_index++) {
        char* path = argv[opt_index];
        int idx = opt_index - optind_local;
        bayer_param_path_set(path, &src_params[idx]);
    }

    /* standardize destination path */
    const char* dstpath = argv[last_arg_index];
    bayer_param_path_set(dstpath, &dest_param);

    /* copy the destination path to user opts structure */
    DCOPY_user_opts.dest_path = BAYER_STRDUP(dest_param.path);

    /* check that source and destinations are ok */
    DCOPY_check_paths();
}

void DCOPY_walk_paths(bayer_flist flist)
{
    int i;
    for (i = 0; i < num_src_params; i++) {
        /* get path for step */
        const char* target = src_params[i].path;

        if (DCOPY_global_rank == 0) {
            BAYER_LOG(BAYER_LOG_INFO, "Walking %s", target);
        }

        /* walk file tree and record stat data for each item */
        int walk_stat = 1;
        bayer_flist_walk_path(target, walk_stat, flist);
    }
}

char* DCOPY_build_dest(const char* name)
{
    /* identify which source directory this came from */
    int i;
    int idx = -1;
    for (i = 0; i < num_src_params; i++) {
        /* get path for step */
        const char* path = src_params[i].path;

        /* get length of source path */
        size_t len = strlen(path);

        /* see if name is a child of path */
        if (strncmp(path, name, len) == 0) {
            idx = i;
            break;
        }
    }

    /* hopefully this won't happen... */
    if (idx == -1) {
        return NULL;
    }

    /* create path of item */
    bayer_path* item = bayer_path_from_str(name);

    /* get source directory */
    bayer_path* src = bayer_path_from_str(src_params[i].path);

    /* get number of components in item */
    int item_components = bayer_path_components(item);

    /* get number of components in source path */
    int src_components = bayer_path_components(src);

    /* if copying into directory, keep last component,
     * otherwise cut all components listed in source path */
    int cut = src_components;
    if (DCOPY_user_opts.copy_into_dir && cut > 0) {
        cut--;
    }

    /* compute number of components to keep */
    int keep = item_components - cut;

    /* chop prefix from item */
    bayer_path_slice(item, cut, keep);

    /* prepend destination path */
    bayer_path_prepend_str(item, dest_param.path);

    /* conver to a NUL-terminated string */
    char* dest = bayer_path_strdup(item);

    /* free our temporary paths */
    bayer_path_delete(&src);
    bayer_path_delete(&item);

    return dest;
}

/* frees resources allocated in call to parse_path_args() */
void DCOPY_free_path_args(void)
{
    /* free memory associated with destination path */
    bayer_param_path_free(&dest_param);

    /* free memory associated with source paths */
    int i;
    for(i = 0; i < num_src_params; i++) {
        bayer_param_path_free(&src_params[i]);
    }
    num_src_params = 0;
    bayer_free(&src_params);
}

/* EOF */
