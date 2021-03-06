/* Copyright (C) 2010 Ion Torrent Systems, Inc. All Rights Reserved */
#ifndef TMAP_MAP_VSW_H
#define TMAP_MAP_VSW_H

#include <config.h>
#include <sys/types.h>
#include "../util/tmap_map_stats.h"

/*!
 initializes the mapping routine
 @param  data    pointer to the mapping data pointer
 @param  refseq  the reference sequence
 @param  opt     the program options
 @return         0 if successful, non-zero otherwise
 */
int32_t
tmap_map_vsw_init(void **data, tmap_refseq_t *refseq, tmap_map_opt_t *opt);

/*!
 initializes the mapping routine for a given thread
 @param  data  pointer to the mapping data pointer
 @param  opt   the program options
 @return       0 if successful, non-zero otherwise
 */
int32_t 
tmap_map_vsw_thread_init(void **data, tmap_map_opt_t *opt);

/*!
 runs the mapping routine for a given thread
 @param  data     pointer to the mapping data pointer
 @param  seqs     the sequence to map (forward, reverse compliment, reverse, and compliment)
 @param  seq_len  the sequence lenth
 @param  index    the reference index
 @param  opt      the program options
 @return          the mappings, NULL otherwise
 */
tmap_map_sams_t*
tmap_map_vsw_thread_map_core(void **data, tmap_seq_t **seqs, int32_t seq_len,
                             tmap_index_t *index, tmap_map_opt_t *opt);

// TODO
tmap_map_sams_t*
tmap_map_vsw_thread_map(void **data, tmap_seq_t **seqs, 
                     tmap_index_t *index, tmap_map_stats_t *stat, tmap_rand_t *rand, 
                     tmap_bwt_match_hash_t *hash[2],
                     tmap_map_opt_t *opt);

/*!
 cleans up the mapping routine for a given thread
 @param  data  pointer to the mapping data pointer
 @param  opt   the program options
 @return       0 if successful, non-zero otherwise
 */
int32_t
tmap_map_vsw_thread_cleanup(void **data, tmap_map_opt_t *opt);

/*! 
  main-like function for 'tmap map_vsw'
  @param  argc  the number of arguments
  @param  argv  the argument list
  @return       0 if executed successful
  */
int 
tmap_map_vsw_main(int argc, char *argv[]);

#endif
