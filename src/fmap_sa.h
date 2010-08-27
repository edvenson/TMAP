#ifndef FMAP_SA_H_
#define FMAP_SA_H_

#define FMAP_SA_INTERVAL 32

/*! @typedef
  @field  primary  S^{-1}(0), or the primary index of BWT
  @field  sa_intv  the suffix array interval (sampled)
  @field  seq_len  the length of the reference sequence
  @field  n_sa     number of suffix array entries
  @field  sa       pointer to the suffix array entries
  */
typedef struct {
    uint32_t primary; // S^{-1}(0), or the primary index of BWT
    uint32_t sa_intv; // a power of 2
    uint32_t seq_len;  
    uint32_t n_sa;
    uint32_t *sa;
} fmap_sa_t;

/*! @function
  @abstract
  @param  fn_fasta  the FASTA file name
  @return           pointer to the sa structure 
  */
fmap_sa_t *
fmap_sa_read(const char *fn_fasta);

/*! @function
  @abstract
  @param  fn_fasta  the FASTA file name
  @param  sa        the sa structure to write
  */
void
fmap_sa_write(const char *fn_fasta, fmap_sa_t *sa);

/*! @function
  @param  sa  pointer to the suffix array structure
  */
void 
fmap_sa_destroy(fmap_sa_t *sa);

/*! @function
  @abstract    returns the suffix array position given the occurence position
  @param  sa   the suffix array
  @param  bwt  the bwt structure 
  @param  k    the suffix array position
  @return      the pac position
*/
uint32_t 
fmap_sa_pac_pos(const fmap_sa_t *sa, const fmap_bwt_t *bwt, uint32_t k);

/*! @function
  @param  fn_fasta  the FASTA file name
  @param  intv      the suffix array interval
  */
void
fmap_sa_bwt2sa(const char *fn_fasta, uint32_t intv);

/*! @function
  @abstract  constructs the suffix array of a given string.
  @param T   T[0..n-1] The input string.
  @param SA  SA[0..n] The output array of suffixes.
  @param n   the length of the given string.
  @return    0 if no error occurred
 */
uint32_t 
fmap_sa_gen_short(const uint8_t *T, int32_t *SA, uint32_t n);


/*! @function
  @abstract     main-like function for 'fmap bwt2sa'
  @param  argc  the number of arguments
  @param  argv  the argument list
  @return       0 if executed successful
  */
int
fmap_sa_bwt2sa_main(int argc, char *argv[]);

#define KEY(V, I, p, h)                                 ( V[ I[p] + h ] )
#define INSERT_SORT_NUM_ITEM    16

void QSufSortSuffixSort(int32_t* __restrict V, int32_t* __restrict I, const int32_t numChar, const int32_t largestInputSymbol,
                                                                        const int32_t smallestInputSymbol, const int32_t skipTransform);
void QSufSortGenerateSaFromInverse(const int32_t *V, int32_t* __restrict I, const int32_t numChar);

#endif
