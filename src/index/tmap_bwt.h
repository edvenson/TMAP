/* Copyright (C) 2010 Ion Torrent Systems, Inc. All Rights Reserved */
#ifndef TMAP_BWT_H
#define TMAP_BWT_H

/* The MIT License

   Copyright (c) 2008 Genome Research Ltd (GRL).

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
   BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
   ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
   SOFTWARE.
   */

#include <stdint.h>

// TODO: parameterize the SA size (uint32_t vs. uint64_t).  NB: be careful about
// where uint32_t is assumed...
// requirement: ((b)->occ_interval%16 == 0)

#ifndef TMAP_UBYTE
#define TMAP_UBYTE
typedef unsigned char uint8_t;
#endif

/*! 
  A BWT index library
  */

#define TMAP_BWT_OCC_INTERVAL 0x80
#define TMAP_BWT_HASH_WIDTH 12

// NB: we do not need a multi-level hash, just the highest-level hash.  We can
// simulate the others from this one...
/*! 
  BWT data structure
  */
typedef struct {
    uint32_t version_id;  /*!< the version id of this file */
    uint32_t primary;  /*!< S^{-1}(0), or the primary index of BWT */
    uint32_t L2[5];  /*!< C(), cumulative count */
    uint32_t seq_len;  /*!< reference sequence length */
    uint32_t bwt_size;  /*!< size of bwt in bytes */
    uint32_t occ_interval;  /*!< occurrence array interval, must be a strictly positive power of 16: (16, 32, 48, ...) */
    uint32_t *bwt;  /*!< burrows-wheeler transform */
    uint32_t cnt_table[256];  /*!< occurrence array */
    uint32_t is_rev;  /*!< 1 if the reference sequence was reversed, 0 otherwise */
    uint32_t **hash_k;  /*!< hash of the BWT occurrence array (lower bounds) */
    uint32_t **hash_l;  /*!< hash of the BWT occurrence array (upper bounds) */
    uint32_t hash_width;  /*!< the k-mer that is hashed */
    uint32_t is_shm;  /*!< 1 if loaded from shared memory, 0 otherwise */
} tmap_bwt_t;

/*! 
  @param  fn_fasta  the FASTA file name
  @param  is_rev    0 if to read the reverse packed sequence, 1 otherwise
  @return           pointer to the bwt structure 
  */
tmap_bwt_t *
tmap_bwt_read(const char *fn_fasta, uint32_t is_rev);

/*! 
  @param  fn_fasta  the FASTA file name
  @param  is_rev    0 if to write the reverse packed sequence, 1 otherwise
  @param  bwt       the bwt structure to write
  */
void 
tmap_bwt_write(const char *fn_fasta, tmap_bwt_t *bwt, uint32_t is_rev);

/*! 
  @param  bwt  the bwt structure 
  @return      the number of bytes required for this bwt in shared memory
  */
size_t
tmap_bwt_shm_num_bytes(tmap_bwt_t *bwt);

/*! 
  @param  fn_fasta  the FASTA file name
  @param  is_rev    0 if to write the reverse packed sequence, 1 otherwise
  @return      the number of bytes required for this bwt in shared memory
  */
size_t
tmap_bwt_shm_read_num_bytes(const char *fn_fasta, uint32_t is_rev);

/*! 
  @param  bwt  the bwt structure to pack 
  @param  buf  the byte array in which to pack the bwt data
  @return      a pointer to the next unused byte in memory
  */
uint8_t *
tmap_bwt_shm_pack(tmap_bwt_t *bwt, uint8_t *buf);

/*! 
  @param  buf  the byte array in which to unpack the bwt data
  @return      a pointer to the initialized bwt structure
  */
tmap_bwt_t *
tmap_bwt_shm_unpack(uint8_t *buf);

/*! 
  @param  bwt  pointer to the bwt structure to destroy
  */
void 
tmap_bwt_destroy(tmap_bwt_t *bwt);

/*! 
  @param  bwt           pointer to the bwt structure to update
  @param  occ_interval  the new occurrence interval
  */
void 
tmap_bwt_update_occ_interval(tmap_bwt_t *bwt, uint32_t occ_interval);

/*! 
  generates the occurrence array
  @param  bwt  pointer to the bwt structure to update 
  */
void 
tmap_bwt_gen_cnt_table(tmap_bwt_t *bwt);

/*! 
  generates the occurrence hash
  @param  bwt         pointer to the bwt structure to update 
  @param  hash_width  the k-mer length to hash
  */
void
tmap_bwt_gen_hash(tmap_bwt_t *bwt, uint32_t hash_width);

/*! 
  calculates the next occurrence given the previous occurrence and the next base
  @param  bwt  pointer to the bwt structure 
  @param  k    previous occurrence
  @param  c    base in two-bit integer format
  @return      the next occurrence given the base
  */
inline uint32_t 
tmap_bwt_occ(const tmap_bwt_t *bwt, uint32_t k, uint8_t c);

/*! 
  calculates the next occurrences given the previous occurrence for all four bases
  @param  bwt  pointer to the bwt structure 
  @param  k    previous occurrence
  @param  cnt  pointer to the next occurrences for all four bases
  */
inline void 
tmap_bwt_occ4(const tmap_bwt_t *bwt, uint32_t k, uint32_t cnt[4]);

/*! 
  calculates the SA interval given the previous SA interval and the next base
  @param  bwt  pointer to the bwt structure 
  @param  k    previous lower occurrence
  @param  l    previous upper occurrence
  @param  c    base in two-bit integer format
  @param  ok   the next lower occurrence
  @param  ol   the next upper occurrence
  @details     more efficient version of bwt_occ but requires that k <= l (not checked)
  */
inline void 
tmap_bwt_2occ(const tmap_bwt_t *bwt, uint32_t k, uint32_t l, uint8_t c, uint32_t *ok, uint32_t *ol);

/*! 
  calculates the next SA intervals given the previous SA intervals for all four bases
  @param  bwt   pointer to the bwt structure 
  @param  k     previous lower occurrence
  @param  l     previous upper occurrence
  @param  cntk  next upper occurrences
  @param  cntl  next lower occurrences
  @details      more efficient version of bwt_occ4 but requires that k <= l (not checked)
  */
inline void 
tmap_bwt_2occ4(const tmap_bwt_t *bwt, uint32_t k, uint32_t l, uint32_t cntk[4], uint32_t cntl[4]);

// TODO: document
// Returns the index of the occurrence array at or before k
#define tmap_bwt_get_occ_array_i(b, k) ((k)/(b)->occ_interval * ((b)->occ_interval/(sizeof(uint32_t)*8/2) + sizeof(uint32_t)/4*4))

// TODO: document
// Returns the array of 16 bases at [(k-(k%16),k+(16-(k%16))-1]
#define tmap_bwt_get_bwt16(b, k) ((b)->bwt[tmap_bwt_get_occ_array_i(b, k) + sizeof(uint32_t)/4*4 + (k)%(b)->occ_interval/16])
//#define tmap_bwt_get_bwt16(b, k) ((b)->bwt[(k)/(b)->occ_interval*12 + 4 + (k)%(b)->occ_interval/16])

/*! 
  @param  b   pointer to the bwt structure
  @param  k   the zero-based index of the bwt character to retrieve
  @return     the bwt character from the $-removed BWT string.
  @details    Note that tmap_bwt_t::bwt is not exactly the BWT string 
  and therefore this define is called tmap_bwt_B0 instead of tmap_bwt_B. 
  */
#define tmap_bwt_B0(b, k) (tmap_bwt_get_bwt16(b, k)>>((~(k)&0xf)<<1)&3)

// TODO: document
// Returns the occurrence array at or before k
#define tmap_bwt_occ_intv(b, k) ((b)->bwt + tmap_bwt_get_occ_array_i(b, k))
//#define tmap_bwt_occ_intv(b, k) ((b)->bwt + (k)/(b)->occ_interval*12)

/*!  
  inverse Psi function
  @param  bwt  pointer to the bwt structure
  @param  k    the occurrence position 
  @return      the suffix array position
  */
#define tmap_bwt_invPsi(bwt, k)												\
  (((k) == (bwt)->primary)? 0 :										\
   ((k) < (bwt)->primary)?											\
   (bwt)->L2[tmap_bwt_B0(bwt, k)] + tmap_bwt_occ(bwt, k, tmap_bwt_B0(bwt, k))		\
   : (bwt)->L2[tmap_bwt_B0(bwt, (k)-1)] + tmap_bwt_occ(bwt, k, tmap_bwt_B0(bwt, (k)-1)))

/*! 
  main-like function for 'tmap pac2bwt'
  @param  argc  the number of arguments
  @param  argv  the argument list
  @return       0 if executed successful
  */
int
tmap_bwt_pac2bwt_main(int argc, char *argv[]);

#endif
