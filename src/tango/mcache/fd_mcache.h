#ifndef HEADER_fd_src_tango_mcache_fd_mcache_h
#define HEADER_fd_src_tango_mcache_fd_mcache_h

#include "../fd_tango_base.h"

/* FD_MCACHE_{ALIGN,FOOTPRINT} specify the alignment and footprint
   needed for a mcache.  ALIGN must be >= FD_FRAG_META_ALIGN.  Depth is
   assumed to be valid.  These are provided to facilitate compile time
   mcache declarations. */

#define FD_MCACHE_ALIGN              (4096UL)
#define FD_MCACHE_FOOTPRINT( depth ) ((4096UL+(depth)*sizeof(fd_frag_meta_t)+(FD_MCACHE_ALIGN-1UL)) & (~(FD_MCACHE_ALIGN-1UL)))

/* FD_MCACHE_SEQ_{ALIGN,CNT} give the alignemnt and number entries in
   the mcache's seq array.  ALIGN is double cache lined for false
   sharing protection reasons.  CNT should be a multiple of 16. */

#define FD_MCACHE_SEQ_ALIGN (128UL)
#define FD_MCACHE_SEQ_CNT   (16UL)

/* FD_MCACHE_{ALIGN,FOOTPRINT} specify the alignment and footprint of
   the portion of an mcache set aside for application use.  This region
   is double cache line aligned to help mitigate various kinds of false
   sharing. */

#define FD_MCACHE_APP_ALIGN     (128UL)
#define FD_MCACHE_APP_FOOTPRINT (3840UL)

/* FD_MCACHE_{LG_BLOCK,LG_INTERLEAVE,BLOCK} specifies how recent
   fragment meta data should be packed into mcaches.  LG_BLOCK should be
   in [0,64).  LG_INTERLEAVE should be in [0,FD_MCACHE_BLOCK).  BLOCK ==
   2^LG_BLOCK.  See below for more details. */

#define FD_MCACHE_LG_BLOCK      (7)
#define FD_MCACHE_LG_INTERLEAVE (2)
#define FD_MCACHE_BLOCK         (128UL) /* == 2^FD_MCACHE_LG_BLOCK, explicit to workaround compiler limitations */

FD_PROTOTYPES_BEGIN

/* Construction API */

/* fd_mcache_{align,footprint} return the required alignment and
   footprint of a memory region suitable for use as mcache with depth
   entries.  align returns FD_MCACHE_ALIGN.  If depth is invalid (e.g.
   not an integer power-of-2 >= FD_MCACHE_BLOCK, footprint will silently
   return 0 (and thus can be used by the caller to validate depth
   configuration parameters). */

FD_FN_CONST ulong
fd_mcache_align( void );

FD_FN_CONST ulong
fd_mcache_footprint( ulong depth );

/* fd_mcache_new formats an unused memory region for use as a mcache.
   shmem is a non-NULL pointer to this region in the local address space
   with the required footprint and alignment.  depth is the number of
   cache entries (should be an integer power of 2 >= FD_MCACHE_BLOCK).
   seq0 is the initial fragment sequence number a producer for this
   mcache.

   The cache entries will be initialized such all queries for any
   sequence number will fail immediately after creation.  They will
   further be initialized such that for any consumer initialized to
   start receiving a sequence number at or after seq0 will think it is
   ahead of the producer (such that it will wait for its sequence number
   cleanly instead of immediately trying to recover a gap).  Conversely,
   consumers initialized to start receiving a sequence number before
   seq0 will think they are behind the producer (thus realize it is been
   incorrectly initialized and can recover appropriately).  Anybody who
   looks at the mcache entries directly will also see the entries are
   initialized to have zero sz (such that they shouldn't try deference
   any fragment payloads), have the SOM and EOM bits set (so they
   shouldn't try to interpret the entry as part of some message spread
   over multiple fragments) and have the ERR bit set (so they don't
   think there is any validity to the meta data or payload).

   Returns shmem (and the memory region it points to will be formatted
   as a mcache) on success and NULL on failure (logs details).  Reasons
   for failure include obviously bad shmem or bad depth. */

void *
fd_mcache_new( void * shmem,
               ulong  depth,
               ulong  seq0 );

/* fd_mcache_join joins the caller to the mcache.  shmcache points to
   the first byte of the memory region backing the mcache in the
   caller's addresss space.

   Returns a pointer in the local address space to the mcache on success
   (IMPORTANT! THIS IS NOT JUST A CAST OF SHMCACHE) and NULL on failure
   (logs details).  Reasons for failure are that shmcache is obviously
   not a pointer to memory region holding a mcache.  Every successful
   join should have a matching leave.
   
   Cache lines are indexed [0,depth) and the mapping from sequence
   number to depth is nontrivial (see below for accessors and mapping
   functions).  There is no restrictions on the number of joins overall
   and a single thread can join multiple times (all joins to the same
   shmcache laddr will return same mcache laddr). */

fd_frag_meta_t *
fd_mcache_join( void * shmcache );

/* fd_mcache_leave leaves a current local join.  Returns a pointer to
   the underlying shared memory region on success (IMPORTANT!  THIS IS
   NOT JUST A CAST OF MCACHE) and NULL on failure (logs details).
   Reasons for failure include mcache is NULL. */

void *
fd_mcache_leave( fd_frag_meta_t const * mcache );
 
/* fd_mcache_delete unformats a memory region used as a mcache.  Assumes
   nobody is joined to the region.  Returns a pointer to the underlying
   shared memory region or NULL if used obviously in error (e.g.
   shmcache is obviously not a mcache ...  logs details).  The ownership
   of the memory region is transferred to the caller. */

void *
fd_mcache_delete( void * shmcache );

/* Accessor API */

/* fd_mcache_{depth,seq0} return the values corresponding to those use
   at the mcache's construction.  Assume mcache is a current local join. */

FD_FN_PURE ulong fd_mcache_depth( fd_frag_meta_t const * mcache );
FD_FN_PURE ulong fd_mcache_seq0 ( fd_frag_meta_t const * mcache );

/* fd_mcache_seq_laddr returns location in the caller's local address
   space of mcache's sequence array.  This array is indexed
   [0,FD_MCACHE_SEQ_CNT) with FD_MCACHE_SEQ_ALIGN alignment (double
   cache line).  laddr_const is a const correct version.  Assumes mcache
   is a current local join.

   seq[0] has special meaning.  Specifically, sequence numbers in
   [seq0,seq[0]) cyclic are guaranteed to have been published.  seq[0]
   is not strictly atomically updated by the producer when it publishes
   so seq[0] can lag the most recently published sequence number
   somewhat.  As seq[0] is moderately to aggressively frequently updated
   by the mcache's producer (depending on the application), this is on
   its own cache line pair to avoid false sharing.  seq[0] is mostly
   used for monitoring, initialization and support for some methods or
   unreliable consumer overrun handling.

   The meaning of the remaining sequence numbers is application
   dependent.  Application should try to restrict any use of these to
   ones cache-friendly with seq[0] (e.g. use for producer write oriented
   cases or use for rarely used cases). */

FD_FN_CONST ulong const * fd_mcache_seq_laddr_const( fd_frag_meta_t const * mcache );
FD_FN_CONST ulong *       fd_mcache_seq_laddr      ( fd_frag_meta_t *       mcache );

/* fd_mcache_app_laddr returns location in the caller's local address
   space of memory set aside for application specific usage.  Assumes
   mcache is a current local join.  This region has FD_MCACHE_APP_ALIGN
   alignment (double cache line) and is FD_MCACHE_APP_FOOTPRINT in size.
   This is quite large with multiple cache line pairs of space to
   support multiple use cases with good DRAM cache proprieties,
   including flow control, command and control, and real time
   monitoring.  laddr_const is a const-correct version. */

FD_FN_CONST uchar const * fd_mcache_app_laddr_const( fd_frag_meta_t const * mcache );
FD_FN_CONST uchar *       fd_mcache_app_laddr      ( fd_frag_meta_t *       mcache );

/* fd_mcache_line_idx returns the index of the cache line in a depth
   entry mcache (depth is asssumed to be a power of 2) where the
   metadata for the frag with sequence number seq will be stored when it
   is in cache.  Outside of startup transients, a mcache is guaranteed
   to exactly hold the depth most recently sequence numbers (the act of
   publishing a new sequence number atomically unpublishes the oldest
   sequence number implicitly).

   FD_MCACHE_LG_INTERLEAVE is in [0,FD_MCACHE_LG_BLOCK) and controls the
   details of this mapping.  LG_INTERLEAVE 0 indicates no interleaving.
   Values from 1 to LG_BLOCK space out sequential frag meta data in
   memory to avoid false sharing between producers and fast consumers to
   keep fast consumers low latency while keeping frag meta data storage
   compact in memory to help throughput of slow consumers.

   Specifically, at a LG_INTERLEAVE of i with s byte frag meta data,
   meta data storage for sequential frags is typically s*2^i bytes
   apart.  To avoid wasting memory and bandwidth, the interleaving is
   implemented by doing a rotation of the least LG_BLOCK bits of the lg
   depth bits of the sequence number (NOTE: this imposes a requirement
   that mcaches have at least a depth of 2^LG_BLOCK fragments).  This
   yields a frag sequence number to line idx mapping that avoids false
   sharing for fast consumers and maintains compactness, avoids TLB
   thrashing (even if meta data is backed by normal pages) and exploits
   CPU data and TLB prefetching behavior for slow consumers.

   How useful block interleaving is somewhat application dependent.
   Different values have different trade offs between optimizing for
   fast and slow consumers and for different sizes of meta data and
   different page sizes backing memory.

   Using 0 / B for FD_MCACHE_LG_INTERLEAVE / LG_BLOCK will disable meta
   data interleaving while still requiring mcaches be at least 2^B in
   size.  This implicit optimizes for slow consumers.  The 2 / 7 default
   above (with a 32-byte size 32-byte aligned fd_frag_meta_t and a
   mcache that is at least normal page aligned) will access cached meta
   data in sequential blocks of 128 message fragments that are normal
   page size and aligned while meta data within those blocks will
   typically be strided at double DRAM cache line granularity.  As such,
   fast consumers (e.g. those within 32 of the producers) will rarely
   have false sharing with the producers as nearby sequence numbers are
   on different DRAM cache line pairs.  And slow consumers (e.g. ones
   that fall more than 128 fragments behind) will access meta data in a
   very DRAM cache friendly / data prefetcher / TLB friendly / bandwidth
   efficient manner (and without needing to load any prefilterable
   payload data while completely avoiding memory being written by the
   producer).  That is, it typically has good balance of performance for
   both fast and slow consumers simultaneously. */

#if FD_MCACHE_LG_INTERLEAVE==0

FD_FN_CONST static inline ulong /* Will be in [0,depth) */
fd_mcache_line_idx( ulong seq,
                    ulong depth ) { /* Assumed power of 2 >= BLOCK */
  return seq & (depth-1UL);
}

#else

FD_FN_CONST static inline ulong /* Will be in [0,depth) */
fd_mcache_line_idx( ulong seq,
                    ulong depth ) { /* Assumed power of 2 >= BLOCK */
  ulong block_mask = FD_MCACHE_BLOCK - 1UL; /* Compile time */
  ulong page_mask  = (depth-1UL) & (~block_mask);    /* Typically compile time or loop invariant */
  ulong page = seq & page_mask;
  ulong bank = (seq << FD_MCACHE_LG_INTERLEAVE) & block_mask;
  ulong idx  = (seq & block_mask) >> (FD_MCACHE_LG_BLOCK-FD_MCACHE_LG_INTERLEAVE);
  return page | bank | idx;
}

#endif

#if FD_HAS_AVX

/* FIXME: DOCUMENT */
/* FIXME: IMPORT FALLBACKS TO OTHER NON-AVX PLATFORMS */
/* Note: this is only valid on x86 CPUs that have atomic AVX load /
   stores. */

#define FD_MCACHE_AVX_WAIT( meta_avx, seq_found, seq_diff, mcache, seq_expected, depth ) do {       \
    ulong           _seq_expected         = (seq_expected);                                         \
    __m256i const * _mcache_line = &((mcache)[ fd_mcache_line_idx( _seq_expected, (depth) ) ].avx); \
    __m256i         _frag_meta_avx;                                                                 \
    ulong           _seq_found;                                                                     \
    long            _seq_diff;                                                                      \
    for(;;) {                                                                                       \
      FD_COMPILER_MFENCE();                                                                         \
      _frag_meta_avx = _mm256_load_si256( _mcache_line );                                           \
      FD_COMPILER_MFENCE();                                                                         \
      _seq_found = fd_frag_meta_avx_seq( _frag_meta_avx );                                          \
      _seq_diff  = fd_seq_diff( _seq_found, _seq_expected );                                        \
      if( FD_LIKELY( _seq_diff>=0L ) ) break;                                                       \
      FD_SPIN_PAUSE();                                                                              \
    }                                                                                               \
    (meta_avx)  = _frag_meta_avx;                                                                   \
    (seq_found) = _seq_found;                                                                       \
    (seq_diff)  = _seq_diff;                                                                        \
  } while(0)

#endif

FD_PROTOTYPES_END

#endif /* HEADER_fd_src_tango_mcache_fd_mcache_h */

