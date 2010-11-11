#include <stdlib.h>
#include <stdio.h>
#include <config.h>
#include <ctype.h>

#ifdef HAVE_SAMTOOLS
#include <sam.h>
#include <bam.h>
#endif

#include "../util/fmap_error.h"
#include "../util/fmap_alloc.h"
#include "../util/fmap_progress.h"
#include "../util/fmap_definitions.h"
#include "../util/fmap_sam.h"
#include "../io/fmap_file.h"
#include "../sw/fmap_fsw.h"
#include "fmap_sam2fs_aux.h"
#include "fmap_sam2fs.h"

#ifdef HAVE_SAMTOOLS

// from bam.h
extern char *bam_nt16_rev_table;

// from fmap_fsw.c
extern int32_t fmap_fsw_sm_short[];

#define FMAP_SAM2FS_FLOW_MATCH '|'
#define FMAP_SAM2FS_FLOW_INS '+'
#define FMAP_SAM2FS_FLOW_DEL '-'
#define FMAP_SAM2FS_FLOW_SNP 'S'
#define FMAP_SAM2FS_FLOW_PAD ' '

static inline int32_t
fmap_sam2fs_is_DNA(char c)
{
  if('a' == c || 'A' == c
     || 'c' == c || 'C' == c
     || 'g' == c || 'G' == c
     || 't' == c || 'T' == c
     || 'n' == c || 'N' == c) {
      return 1;
  }
  else {
      return 0;
  }
}

static inline void 
fmap_sam2fs_bam_alloc_data(bam1_t *bam, int size)
{
  if(bam->m_data < size) {
      bam->m_data = size;
      fmap_roundup32(bam->m_data); // from bam.h
      bam->data = (uint8_t*)realloc(bam->data, bam->m_data);
  }
}


static bam1_t *
fmap_sam2fs_copy_to_sam(bam1_t *bam_old, fmap_fsw_path_t *path, int32_t path_len, int32_t score)
{
  bam1_t *bam_new = NULL;
  int32_t i;
  uint32_t *cigar;
  int32_t n_cigar;
  uint8_t *old_score;
  
  bam_new = fmap_calloc(1, sizeof(bam1_t), "bam_new");
  bam_new->data_len = 0; //bam_new->m_data;

  // query name
  bam_new->core.l_qname = bam_old->core.l_qname;
  bam_new->data_len += bam_new->core.l_qname;
  fmap_sam2fs_bam_alloc_data(bam_new, bam_new->data_len);
  memcpy(bam1_qname(bam_new), bam1_qname(bam_old), bam_old->core.l_qname);

  // flag
  bam_new->core.flag = bam_old->core.flag;

  // tid, pos, qual
  bam_new->core.tid = bam_old->core.tid;
  bam_new->core.pos = bam_old->core.pos; 
  if(0 < path[path_len-1].j) { // adjust the position
      bam_new->core.pos += path[path_len-1].j;
  }
  bam_new->core.qual = bam_old->core.qual; 

  // no pairing information
  bam_new->core.mtid = -1;
  bam_new->core.mpos = -1;
  bam_new->core.isize = 0;

  // cigar
  // TODO: soft-clipping...
  fmap_sam2fs_bam_alloc_data(bam_new, bam_new->data_len);
  cigar = fmap_fsw_path2cigar(path, path_len, &n_cigar);
  bam_new->core.n_cigar = n_cigar;
  bam_new->data_len += bam_new->core.n_cigar * sizeof(uint32_t);
  fmap_sam2fs_bam_alloc_data(bam_new, bam_new->data_len);
  for(i=0;i<bam_new->core.n_cigar;i++) {
      bam1_cigar(bam_new)[i] = cigar[i];
  }
  free(cigar);

  // seq
  bam_new->core.l_qseq = bam_old->core.l_qseq;
  bam_new->data_len += (bam_new->core.l_qseq + 1)/2;
  fmap_sam2fs_bam_alloc_data(bam_new, bam_new->data_len);
  for(i=0;i<(bam_new->core.l_qseq+1)/2;i++) {
      bam1_seq(bam_new)[i] = bam1_seq(bam_old)[i];
  }

  // qualities
  bam_new->data_len += bam_new->core.l_qseq;
  fmap_sam2fs_bam_alloc_data(bam_new, bam_new->data_len);
  for(i=0;i<bam_new->core.l_qseq;i++) {
      bam1_qual(bam_new)[i] = bam1_qual(bam_old)[i];
  }

  // copy over auxiliary data
  bam_new->data_len += bam_old->l_aux;
  fmap_sam2fs_bam_alloc_data(bam_new, bam_new->data_len);
  for(i=0;i<bam_old->l_aux;i++) {
      bam1_aux(bam_new)[i] = bam1_aux(bam_old)[i];
  }
  bam_new->l_aux = bam_old->l_aux;

  // score
  old_score = bam_aux_get(bam_new, "AS");
  if(NULL == old_score) {
      bam_aux_append(bam_new, "AS", 'i', sizeof(uint32_t), (uint8_t*)(&score));
  }
  else {
      bam_aux_del(bam_new, old_score);
      bam_aux_append(bam_new, "AS", 'i', sizeof(uint32_t), (uint8_t*)(&score));
  }

  // destroy the old bam
  bam_destroy1(bam_old);

  return bam_new;
}

bam1_t *
fmap_sam2fs_aux(bam1_t *bam, char *flow_order, int32_t flow_score, int32_t flow_offset, 
                int32_t aln_global, int32_t output_type, int32_t j_type)
{
  int32_t i, j, k, l;

  uint8_t *md_data = NULL;
  char *md = NULL;
  uint32_t md_i = 0;
  char *ref, *read, *aln;

  uint32_t aln_len = 0;
  uint32_t *cigar = NULL;
  uint8_t *bam_seq = NULL;

  char *read_bases = NULL;
  int32_t read_bases_len = 0, read_bases_mem = 256;
  char *ref_bases = NULL;
  int32_t ref_bases_len = 0, ref_bases_mem = 64;
  uint8_t flow_order_tmp[4];

  int32_t soft_clip_start=0, soft_clip_end=0;

  int32_t flow_len;
  uint8_t *base_calls = NULL;
  uint16_t *flowgram = NULL;
  fmap_fsw_path_t *path = NULL;
  int32_t path_len;
  fmap_fsw_param_t param;
  int64_t score;

  // set the alignment parameters
  param.gap_open = 13*100;
  param.gap_ext = 2*100;
  param.gap_end = 2*100;
  param.matrix = fmap_fsw_sm_short;
  param.fscore = 100*flow_score;
  param.offset = flow_offset;
  param.row = 5;
  param.band_width = 50;

  if(BAM_FUNMAP & bam->core.flag) {
      return bam;
  }

  // get the MD tag
  if(NULL == (md_data = bam_aux_get(bam, "MD"))) {
      fmap_error("MD tag is missing", Exit, OutOfRange);
  }
  md = bam_aux2Z(md_data);

  // cigar
  cigar = bam1_cigar(bam);

  // get the alignment length
  for(i=aln_len=0;i<bam->core.n_cigar;i++) {
      aln_len += (cigar[i] >> 4);
  }
  if(0 == aln_len) {
      fmap_error("zero alignment length", Exit, OutOfRange);
  }

  // get soft clipping at the start/end of the sequence
  if((BAM_CSOFT_CLIP & cigar[0])) {
      soft_clip_start  = (cigar[0] >> 4);
  }
  if(1 < bam->core.n_cigar && (BAM_CSOFT_CLIP & cigar[bam->core.n_cigar-1])) {
      soft_clip_end = (cigar[bam->core.n_cigar-1] >> 4);
  }

  if(0 < soft_clip_start || 0 < soft_clip_end) {
      fmap_error("Soft clipping currently not supported", Exit, OutOfRange);
  }

  // get the read bases
  bam_seq = bam1_seq(bam);
  read_bases = fmap_calloc(1+bam->core.l_qseq, sizeof(char), "read_bases");
  for(i=0;i<bam->core.l_qseq;i++) {
      read_bases[i] = bam_nt16_rev_table[bam1_seqi(bam_seq, i)]; 
  }
  read_bases[i]='\0';
  read_bases_len = bam->core.l_qseq; 
  read_bases_mem = read_bases_len + 1;

  // get the reference bases
  ref_bases = fmap_calloc(ref_bases_mem, sizeof(char), "read_bases");

  // pre-process using the cigar array
  for(i=j=k=0;i<bam->core.n_cigar;i++) {
      int32_t op, op_len;
      op = (cigar[i] & 0xf);
      op_len = (cigar[i] >> 4);

      switch(op) {
        case BAM_CMATCH:
          // copy over these bases
          while(ref_bases_mem <= ref_bases_len + op_len + 1) {
              ref_bases_mem <<= 1;
              ref_bases = fmap_realloc(ref_bases, sizeof(char)*ref_bases_mem, "ref_bases");
          }
          for(l=0;l<op_len;l++) {
              ref_bases[k+l] = read_bases[j+l];
          }
          j += op_len; 
          k += op_len; 
          ref_bases_len += op_len;
          break;
        case BAM_CSOFT_CLIP:
        case BAM_CINS:
          // skip soft clipped and inserted bases 
          j += op_len;
          break;
        case BAM_CREF_SKIP:
        case BAM_CDEL:
        case BAM_CHARD_CLIP:
        case BAM_CPAD:
          // ignore
          break;
        default:
          fmap_error("unknown cigar operator", Exit, OutOfRange);
          break;
      }
  }

  // fill in with the MD array
  for(md_i=i=0;md_i<strlen(md);) {
      if('0' <= md[md_i] && md[md_i] <= '9') { // 0-9, matches
          l = atoi(md + md_i);
          i += l;
          while(md_i < strlen(md) && '0' <= md[md_i] && md[md_i] <= '9') { // 0-9
              md_i++; // skip over integers
          }
      }
      else if('^' == md[md_i]) { // deletion from the reference
          // how many bases are deleted?
          md_i++; // skip over '^'
          l=0;
          while(md_i+l < strlen(md) && 1 == fmap_sam2fs_is_DNA(md[md_i+l])) {
              l++;
          }
          // reallocate
          while(ref_bases_mem <= ref_bases_len + l + 1) { // more memory please
              ref_bases_mem <<= 1;
              ref_bases = fmap_realloc(ref_bases, sizeof(char)*ref_bases_mem, "ref_bases");
          }
          // shift up
          for(j=ref_bases_len-1;i<=j;j--) {
              ref_bases[j+l] = ref_bases[j];
          }
          // fill in
          for(j=0;j<l;j++) {
              ref_bases[i+j] = md[md_i+j];
          }
          md_i += l;
          i += l;
          ref_bases_len += l;
      }
      else if(1 == fmap_sam2fs_is_DNA(md[md_i])) { // SNP
          ref_bases[i] = md[md_i];
          i++;
          md_i++;
      }
      else {
          fmap_error("could not parse the MD tag", Exit, OutOfRange);
      }
  }
  ref_bases[i]='\0';
  //fprintf(stderr, "ref_bases_len=%d\nref_bases=%s\n", ref_bases_len, ref_bases);
  //fprintf(stderr, "read_bases_len=%d\nread_bases=%s\n", read_bases_len, read_bases);

  // DNA to integer
  for(i=0;i<read_bases_len;i++) {
      read_bases[i] = fmap_nt_char_to_int[(int)read_bases[i]];
  }
  for(i=0;i<ref_bases_len;i++) {
      ref_bases[i] = fmap_nt_char_to_int[(int)ref_bases[i]];
  }
  for(i=0;i<4;i++) {
      flow_order_tmp[i] = fmap_nt_char_to_int[(int)flow_order[i]];
  }

  flow_len = 0;
  for(i=j=0;i<read_bases_len;i++) {
      while(flow_order_tmp[j] != read_bases[i]) {
          flow_len++;
          j = (1+j) & 3;
      }
  }
  flow_len++; // the last flow

  base_calls = fmap_calloc(flow_len, sizeof(uint8_t), "base_calls");
  flowgram = fmap_calloc(flow_len, sizeof(uint16_t), "base_calls");

  for(i=j=0;i<flow_len;i++) {
      if(flow_order_tmp[i&3] == read_bases[j]) {
          k=j;
          while(j < read_bases_len 
                && read_bases[j] == read_bases[k]) {
              j++;
          }
          base_calls[i] = j-k;
          flowgram[i] = 100*(j-k);
      }
      else {
          base_calls[i] = 0;
          flowgram[i] = 0;
      }
  }

  path = fmap_calloc(FMAP_FSW_MAX_PATH_LENGTH(ref_bases_len, flow_len, param.offset), sizeof(fmap_fsw_path_t), "path"); 

  // re-align 
  if(1 == aln_global) {
      score = fmap_fsw_global_core((uint8_t*)ref_bases, ref_bases_len,
                                   flow_order_tmp, base_calls, flowgram, flow_len,
                                   -1, 0,
                                   &param, path, &path_len);
  }
  else {
      score = fmap_fsw_fitting_core((uint8_t*)ref_bases, ref_bases_len,
                                    flow_order_tmp, base_calls, flowgram, flow_len,
                                    -1, 0,
                                    &param, path, &path_len);
  }

  switch(output_type) {
    case FMAP_SAM2FS_OUTPUT_ALN:
      // bound, since we could start with an insertion (read starts with bases,
      // reference starts with gaps)
      i = path[0].j;
      j = path[path_len-1].j;
      if(ref_bases_len <= i) i = ref_bases_len;
      if(j < 0) j = 0;

      fmap_file_fprintf(fmap_file_stdout, "%s\t", bam1_qname(bam));
      fmap_fsw_print_aln(fmap_file_stdout, score, path, path_len, flow_order_tmp, 
                         (uint8_t*)ref_bases,
                         (BAM_FREVERSE & bam->core.flag) ? 1 : 0,
                         j_type);
      fmap_file_fprintf(fmap_file_stdout, "\t");
      fmap_sam2fs_aux_flow_align(fmap_file_stdout, 
                                 (uint8_t*)read_bases, 
                                 read_bases_len,
                                 (uint8_t*)(ref_bases + j),
                                 i - j + 1, 
                                 flow_order_tmp);
      fmap_file_fprintf(fmap_file_stdout, "\n");
      break;
    case FMAP_SAM2FS_OUTPUT_SAM:
      bam = fmap_sam2fs_copy_to_sam(bam, path, path_len, score);

      if(FMAP_FSW_NO_JUSTIFY != j_type) {
          
          fmap_fsw_get_aln(path, path_len, flow_order_tmp, (uint8_t*)ref_bases, 
                           (BAM_FREVERSE & bam->core.flag) ? 1 : 0,
                           &ref, &read, &aln, j_type);

          if((BAM_FREVERSE & bam->core.flag)) { // set it the forward strand of the reference
              for(i=0;i<(path_len>>1);i++) {
                  char tmp;
                  tmp = ref[i]; ref[i] = ref[path_len-i-1]; ref[path_len-i-1] = tmp;
                  tmp = read[i]; read[i] = read[path_len-i-1]; read[path_len-i-1] = tmp;
                  tmp = aln[i]; aln[i] = aln[path_len-i-1]; aln[path_len-i-1] = tmp;
              }
          }

          // do not worry about read fitting since fmap_fsw_get_aln handles this
          // above
          fmap_sam_left_justify(bam, ref, read, path_len);
          free(ref); free(read); free(aln);
      }
      break;
  }

  // free memory
  free(path);
  free(base_calls);
  free(flowgram);
  free(read_bases);
  free(ref_bases);

  return bam;
}

static int
usage(fmap_sam2fs_opt_t *opt)
{
  fmap_file_fprintf(fmap_file_stderr, "\n");
  fmap_file_fprintf(fmap_file_stderr, "Usage: %s sam2fs <in.sam/in.bam> [options]", PACKAGE);
  fmap_file_fprintf(fmap_file_stderr, "\n");
  fmap_file_fprintf(fmap_file_stderr, "Options (optional):\n");
  fmap_file_fprintf(fmap_file_stderr, "         -f          the flow order [%s]\n", opt->flow_order);
  fmap_file_fprintf(fmap_file_stderr, "         -F INT      the flow penalty [%d]\n", opt->flow_score);
  fmap_file_fprintf(fmap_file_stderr, "         -o INT      search for homopolymer errors +- offset during re-alignment [%d]\n",
                    opt->flow_offset);
  fmap_file_fprintf(fmap_file_stderr, "         -S          the input is a SAM file\n");
  fmap_file_fprintf(fmap_file_stderr, "         -g          run global alignment (otherwise read fitting) [%d]\n", opt->aln_global);
  fmap_file_fprintf(fmap_file_stderr, "         -O          the output type: 0-alignment 1-SAM 2-BAM [%d]\n", opt->output_type);
  fmap_file_fprintf(fmap_file_stderr, "         -l INT      indel justification type: 0 - none, 1 - 5' strand of the reference, 2 - 5' strand of the read [%d]\n", opt->j_type);
  fmap_file_fprintf(fmap_file_stderr, "         -v          print verbose progress information\n");
  fmap_file_fprintf(fmap_file_stderr, "         -h          print this message\n");
  fmap_file_fprintf(fmap_file_stderr, "\n");

  return 1;
}

static void
fmap_sam2fs_core(const char *fn_in, const char *sam_open_flags, fmap_sam2fs_opt_t *opt)
{
  samfile_t *fp_in = NULL;
  samfile_t *fp_out = NULL;
  bam1_t *b = NULL;

  fp_in = samopen(fn_in, sam_open_flags, 0);
  if(NULL == fp_in) fmap_error(fn_in, Exit, OpenFileError);

  switch(opt->output_type) {
    case FMAP_SAM2FS_OUTPUT_ALN:
      fmap_file_stdout = fmap_file_fdopen(fileno(stdout), "wb", FMAP_FILE_NO_COMPRESSION);
      break;
    case FMAP_SAM2FS_OUTPUT_SAM:
      fp_out = samopen("-", "wh", fp_in->header);
      break;
    case FMAP_SAM2FS_OUTPUT_BAM:
      fp_out = samopen("-", "wb", fp_in->header);
      break;
  }

  b = bam_init1();
  while(0 < samread(fp_in, b)) { 
      // process 
      b = fmap_sam2fs_aux(b, opt->flow_order, opt->flow_score, opt->flow_offset, 
                          opt->aln_global, opt->output_type, opt->j_type);
      // write to SAM/BAM if necessary
      if(FMAP_SAM2FS_OUTPUT_SAM == opt->output_type
         || FMAP_SAM2FS_OUTPUT_BAM == opt->output_type) {
          if(samwrite(fp_out, b) < 0) {
              fmap_error(NULL, Exit, WriteFileError);
          }
      }
      // destroy the bam
      bam_destroy1(b);
      // reinitialize
      b = bam_init1();
  }
  bam_destroy1(b);

  // close
  samclose(fp_in); 
  switch(opt->output_type) {
    case FMAP_SAM2FS_OUTPUT_ALN:
      fmap_file_fclose(fmap_file_stdout);
      break;
    case FMAP_SAM2FS_OUTPUT_SAM:
    case FMAP_SAM2FS_OUTPUT_BAM:
      samclose(fp_out);
      break;
  }
}

fmap_sam2fs_opt_t *
fmap_sam2fs_opt_init()
{
  fmap_sam2fs_opt_t *opt=NULL;

  opt = fmap_calloc(1, sizeof(fmap_sam2fs_opt_t), "opt");

  opt->flow_order = fmap_strdup("TACG");
  opt->flow_score = 26; 
  opt->flow_offset = 1;
  opt->aln_global = 0;
  opt->output_type = FMAP_SAM2FS_OUTPUT_ALN;
  opt->j_type = FMAP_FSW_NO_JUSTIFY;

  return opt;
}

void
fmap_sam2fs_opt_destroy(fmap_sam2fs_opt_t *opt)
{
  free(opt->flow_order);
  free(opt);
}

int
fmap_sam2fs_main(int argc, char *argv[])
{
  int c;
  fmap_sam2fs_opt_t *opt;
  char sam_open_flags[16] = "rb";

  opt = fmap_sam2fs_opt_init();

  while((c = getopt(argc, argv, "f:F:o:SgO:l:vh")) >= 0) {
      switch(c) {
        case 'f':
          strncpy(opt->flow_order, optarg, 4); break;
        case 'F':
          opt->flow_score = atoi(optarg); break;
        case 'o':
          opt->flow_offset = atoi(optarg); break;
        case 'S':
          strcpy(sam_open_flags, "r"); break;
        case 'g':
          opt->aln_global = 1; break;
        case 'O':
          opt->output_type = atoi(optarg); break;
        case 'l':
          opt->j_type = atoi(optarg); break;
        case 'v':
          fmap_progress_set_verbosity(1); break;
        case 'h':
        default:
          return usage(opt);
      }
  }

  if(argc != optind+1 || 1 == argc) {
      return usage(opt);
  }
  else { // check command line options
      fmap_error_cmd_check_int(opt->flow_score, 0, INT32_MAX, "-F");
      fmap_error_cmd_check_int(opt->flow_offset, 0, INT32_MAX, "-o");
      fmap_error_cmd_check_int((int)strlen(opt->flow_order), 4, 4, "-f");
      fmap_error_cmd_check_int(opt->output_type, 0, 2, "-z");
  }

  fmap_sam2fs_core(argv[optind], sam_open_flags, opt);

  fmap_sam2fs_opt_destroy(opt);

  fmap_progress_print2("terminating successfully");

  return 0;
}  
#endif
