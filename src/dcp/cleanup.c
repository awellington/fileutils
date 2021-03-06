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
 * This file contains the logic to truncate the the destination as well as
 * preserve permissions and ownership. Since it would be redundant, we only
 * pay attention to the first chunk of each file and pass the rest along.

 * See the file "COPYING" for the full license governing this code.
 */

#include "cleanup.h"
#include "dcp.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

/** Options specified by the user. */
extern DCOPY_options_t DCOPY_user_opts;

static void DCOPY_truncate_file(DCOPY_operation_t* op, \
                         CIRCLE_handle* handle)
{
    char dest_path_recursive[PATH_MAX];
    char dest_path_file_to_file[PATH_MAX];

    if(op->dest_base_appendix == NULL) {
        sprintf(dest_path_recursive, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->operand + op->source_base_offset + 1);

        strncpy(dest_path_file_to_file, DCOPY_user_opts.dest_path, PATH_MAX);
    }
    else {
        sprintf(dest_path_recursive, "%s/%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->dest_base_appendix, \
                op->operand + op->source_base_offset + 1);

        sprintf(dest_path_file_to_file, "%s/%s", \
                DCOPY_user_opts.dest_path, \
                op->dest_base_appendix);
    }

    BAYER_LOG(BAYER_LOG_DBG, "Truncating file to `%" PRId64 "'.", op->file_size);

    /*
     * Try the recursive file before file-to-file. The cast below requires us
     * to have a maximum file_size of 2^63, not 2^64.
     */
    if(truncate64(dest_path_recursive, op->file_size) < 0) {
        if(truncate64(dest_path_file_to_file, op->file_size) < 0) {
            BAYER_LOG(BAYER_LOG_ERR, "Failed to truncate destination file: %s (errno=%d %s)",
                dest_path_recursive, errno, strerror(errno));

            DCOPY_retry_failed_operation(COPY, handle, op);
            return;
        }
    }
}

void DCOPY_do_cleanup(DCOPY_operation_t* op, \
                      CIRCLE_handle* handle)
{
    char* newop;

    /*
     * Truncate file on last chunk (synchronous mode can write past end of file)
     */
    int64_t bytes_written = (int64_t)(op->chunk + 1) * (int64_t)DCOPY_user_opts.chunk_size;
    if(bytes_written >= op->file_size) {
        /* truncate file to appropriate size, to do this before
         * setting permissions in case file does not have write permission */
        DCOPY_truncate_file(op, handle);

        /* since we still may access the file in the compare step,
         * delay setting permissions and timestamps until final phase */
    }

    /*
     * Add work item to compare source and destination if user requested it.
     */
    if(DCOPY_user_opts.compare) {
        newop = DCOPY_encode_operation(COMPARE, op->chunk, op->operand, \
                                       op->source_base_offset, \
                                       op->dest_base_appendix, op->file_size);

        handle->enqueue(newop);
        free(newop);
    }

    return;
}

/* EOF */
