#include "fd_tango.h"

#if FD_HAS_HOSTED && FD_HAS_AVX

FD_STATIC_ASSERT( FD_CHUNK_SZ==64UL, unit_test );

/* This test uses the mcache application region for holding the rx
   flow controls and tx backpressure counters.  We'll use a cache line
   pair for each reliable rx_seq and the very end will hold backpressure
   counters. */

#define TX_MAX (256UL) /* Less than FD_FRAG_META_ORIG_MAX */
#define RX_MAX (256UL)

static uchar __attribute__((aligned(FD_FCTL_ALIGN))) fctl_mem[ FD_FCTL_FOOTPRINT( RX_MAX ) ];

int
main( int     argc,
      char ** argv ) {
  fd_boot( &argc, &argv );

# define TEST(c) do if( FD_UNLIKELY( !(c) ) ) { FD_LOG_WARNING(( "FAIL: " #c )); return 1; } while(0)

  char const * _mcache = fd_env_strip_cmdline_cstr ( &argc, &argv, "--mcache", NULL,         NULL ); /* (req) tx's mcache */
  char const * _dcache = fd_env_strip_cmdline_cstr ( &argc, &argv, "--dcache", NULL,         NULL ); /* (req) tx's dcache */
  char const * _init   = fd_env_strip_cmdline_cstr ( &argc, &argv, "--init",   NULL,         NULL ); /* (opt) initial tx_seq */
  ulong        tx_idx  = fd_env_strip_cmdline_ulong( &argc, &argv, "--tx-idx", NULL,          0UL ); /* (opt) tx identity */
  ulong        rx_cnt  = fd_env_strip_cmdline_ulong( &argc, &argv, "--rx-cnt", NULL,          0UL ); /* (opt) num rel rx for tx */
  uint         seed    = fd_env_strip_cmdline_uint ( &argc, &argv, "--seed",   NULL, (uint)tx_idx ); /* (opt) rng seed */
  ulong        max     = fd_env_strip_cmdline_ulong( &argc, &argv, "--max",    NULL,    ULONG_MAX ); /* (opt) num iter */

  if( FD_UNLIKELY( !_mcache       ) ) FD_LOG_ERR(( "--mcache not specified" ));
  if( FD_UNLIKELY( !_dcache       ) ) FD_LOG_ERR(( "--dcache not specified" ));
  if( FD_UNLIKELY( tx_idx>=TX_MAX ) ) FD_LOG_ERR(( "--tx-idx too large for this unit-test" ));
  if( FD_UNLIKELY( rx_cnt> RX_MAX ) ) FD_LOG_ERR(( "--rx-cnt too large for this unit-test" ));

  /* burst_avg is average number of data bytes in a burst (0 min, exp
     distributed).  burst_bw is the average bandwidth (excluding header
     overheads in the burst) in bit/second (with 8 bit bytes).
     pkt_framing number of framing bytes for a burst packet (84
     corresponds to logical L1 VLAN tagged ethernet bytes on a wire).
     pkt_payload_max is the number of payload bytes in a maximum size
     packet.  2.1e17 ~ LONG_MAX / 43.668 is such that exponentially
     distributed random with an average in (0,2.1e17) will practically
     never overflow when rounded to a long. */
  /* FIXME: TWEAK TO USE LINE BW INSTEAD OF PAYLOAD BW */

  float burst_avg       = fd_env_strip_cmdline_float( &argc, &argv, "--burst-avg",       NULL, 1472.f );
  float burst_bw        = fd_env_strip_cmdline_float( &argc, &argv, "--burst-bw",        NULL,  50e9f );
  ulong pkt_framing     = fd_env_strip_cmdline_ulong( &argc, &argv, "--pkt-framing",     NULL,   84UL );
  ulong pkt_payload_max = fd_env_strip_cmdline_ulong( &argc, &argv, "--pkt-payload-max", NULL, 1472UL );

  FD_LOG_NOTICE(( "Configuring synthetic load (--burst-avg %g B --burst-bw %g b/s --pkt-framing %lu B --pkt-payload-max %lu B)",
                  (double)burst_avg, (double)burst_bw, pkt_framing, pkt_payload_max ));

  if( FD_UNLIKELY( !((0.f<burst_avg) & (burst_avg<2.1e17f)) ) ) FD_LOG_ERR(( "--burst-avg out of range" ));

  if( FD_UNLIKELY( !(0.f<burst_bw) ) ) FD_LOG_ERR(( "--burst-bw out of range" ));
  float burst_tau = burst_avg*(8.f*1e9f/burst_bw); /* Avg time in ns between bursts (8 bit/byte, 1e9 ns/s), bw in bit/s */
  if( FD_UNLIKELY( !(burst_tau<2.1e17f) ) ) FD_LOG_ERR(( "--burst-bw out of range" ));

  ulong mtu = pkt_framing + pkt_payload_max;
  if( FD_UNLIKELY( (mtu<pkt_framing) | (mtu>(ulong)USHORT_MAX) ) )
    FD_LOG_ERR(( "Too large mtu implied by --pkt-framing and --pkt-payload-max" ));

  FD_LOG_NOTICE(( "Creating rng --seed %u", seed ));

  fd_rng_t _rng[1]; fd_rng_t * rng = fd_rng_join( fd_rng_new( _rng, seed, 0UL ) );

  FD_LOG_NOTICE(( "Joining to --mcache %s", _mcache ));

  fd_frag_meta_t * mcache = fd_mcache_join( fd_wksp_map( _mcache ) );
  if( FD_UNLIKELY( !mcache ) ) FD_LOG_ERR(( "join failed" ));

  ulong   depth   = fd_mcache_depth    ( mcache );
  ulong * _tx_seq = fd_mcache_seq_laddr( mcache );
  uchar * app     = fd_mcache_app_laddr( mcache );
  ulong   app_sz  = fd_mcache_app_sz   ( mcache );

  if( FD_UNLIKELY( rx_cnt*136UL>app_sz ) )
    FD_LOG_ERR(( "increase mcache app_sz to at least %lu for this --rx_cnt", rx_cnt*136UL ));

  ulong tx_seq = _init ? fd_cstr_to_ulong( _init ) : fd_mcache_seq_query( _tx_seq );

  FD_LOG_NOTICE(( "Joining to --dcache %s", _dcache ));

  uchar * dcache = fd_dcache_join( fd_wksp_map( _dcache ) );
  if( FD_UNLIKELY( !dcache ) ) FD_LOG_ERR(( "join failed" ));

  if( FD_UNLIKELY( !fd_dcache_compact_is_safe( dcache, dcache, mtu, depth ) ) )
    FD_LOG_ERR(( "--dcache not compatible with --pkt-framing, --pkt-payload-max and --mcache depth" ));

  ulong chunk = 0UL; /* FIXME: command line / auto recover this from dcache app region for clean dcache recovery too */
  ulong wmark = fd_dcache_compact_wmark( dcache, dcache, mtu ); /* Note: chunk0==0 as this test is uses dcache as chunk base addr */

  FD_LOG_NOTICE(( "Creating fctl for --rx-cnt %lu reliable consumers", rx_cnt ));

  fd_fctl_t * fctl = fd_fctl_join( fd_fctl_new( fctl_mem, rx_cnt ) );

  uchar * fctl_top = app;
  uchar * fctl_bot = app + fd_ulong_align_dn( app_sz, 8UL );
  for( ulong rx_idx=0UL; rx_idx<rx_cnt; rx_idx++ ) {
    ulong * rx_lseq  = (ulong *) fctl_top;      fctl_top += 128UL;
    ulong * rx_backp = (ulong *)(fctl_bot-8UL); fctl_bot -=   8UL;
    fd_fctl_cfg_rx_add( fctl, depth, rx_lseq, rx_backp );
    *rx_backp = 0UL;
  }
  fd_fctl_cfg_done( fctl, 0UL, 0UL, 0UL, 0UL );

  ulong async_min = 1UL<<13;
  ulong async_rem = 0UL;
  ulong cr_avail  = 0UL;

  FD_LOG_NOTICE(( "Running --init %lu (%s) --max %lu", tx_seq, _init ? "manual" : "auto", max ));
  
  int   ctl_som    = 1;
  long  burst_next = fd_log_wallclock();
  ulong burst_rem;
  do {
    burst_next += (long)(0.5f + burst_tau*fd_rng_float_exp( rng ));
    burst_rem   = (ulong)(long)(0.5f + burst_avg*fd_rng_float_exp( rng ));
  } while( FD_UNLIKELY( !burst_rem ) );
  ulong tsorig = 0UL; /* Irrelevant value at init */

# define RELOAD (100000UL)
  ulong iter = 0UL;
  ulong rem  = RELOAD;
  long  tic  = fd_log_wallclock();
  while( iter<max ) {

    /* Do housekeeping in the background */

    if( FD_UNLIKELY( !async_rem ) ) {
      fd_mcache_seq_update( _tx_seq, tx_seq );
      cr_avail = fd_fctl_tx_cr_update( fctl, cr_avail, tx_seq );
      async_rem = fd_async_reload( rng, async_min );
    }
    async_rem--;

    /* Check if we are backpressured */

    if( FD_UNLIKELY( !cr_avail ) ) {
      FD_SPIN_PAUSE();
      continue;
    }

    /* Check if we are waiting for the next burst to start */

    if( FD_LIKELY( ctl_som ) ) {
      if( FD_UNLIKELY( fd_log_wallclock()<burst_next ) ) { /* Opt for start */
        FD_SPIN_PAUSE(); /* Debatable given fd_log_wallclock overhead */
        continue;
      }
      /* We just "started receiving" the first bytes of the next burst
         from the "NIC".  Record the timestamp. */
      tsorig = fd_frag_meta_ts_comp( fd_tickcount() );
    }

    /* We are in the process of "receiving" a fragment from the NIC.
       Compute the details of the synthetic fragment and fill the data
       region with a suitable test pattern as fast as we can.  Note that
       the dcache region pointed by chunk through the double cache line
       region marked by sz is currently not marked as visible and has
       room for up to "MTU" worth of double chunks free.  So we can
       write it at our hearts content.  Since we align frag starts to
       double chunk boundaries, we are safe to tail clobber (which
       further speeds up test pattern generation as this amortizes loop
       overhead, flattens tail branches and only write to the dcache in
       complete double cache lines.  We don't care if this is written
       atomic or not. */
    /* FIXME: THROW IN RANDOM CTL_ERR TO MODEL DETECTED PKT CORRUPTION
       AND OPTION TO DO RANDOM SILENT ERRORS IN PAYLOAD  */

    ulong frag_sz = fd_ulong_min( burst_rem, pkt_payload_max );
    burst_rem -= frag_sz;
    int ctl_eom = !burst_rem;
    int ctl_err = 0;

    ulong sig    = tx_seq; /* Test pattern */
  /*ulong chunk  = ... already at location where next packet will be written ...; */
    ulong sz     = pkt_framing + frag_sz;
    ulong ctl    = fd_frag_meta_ctl( tx_idx, ctl_som, ctl_eom, ctl_err );
  /*ulong tsorig = ... set at burst start ...; */
  /*ulong tspub  = ... set "after" finished receiving from the "NIC" ...; */

    uchar * p   = (uchar *)fd_chunk_to_laddr( dcache, chunk );
    __m256i avx = _mm256_set1_epi64x( (long)tx_seq );
    for( ulong off=0UL; off<sz; off+=128UL ) {
      _mm256_store_si256( (__m256i *)(p     ), avx );
      _mm256_store_si256( (__m256i *)(p+32UL), avx );
      _mm256_store_si256( (__m256i *)(p+64UL), avx );
      _mm256_store_si256( (__m256i *)(p+96UL), avx );
      p += 128UL;
    }

    /* We just "finished receiving" the next fragment of the burst from
       the "NIC".  Publish to consumers as frag tx_seq.  This implicitly
       unpublishes frag tx_seq-depth (cyclic) at the same time. */

    ulong tspub = fd_frag_meta_ts_comp( fd_tickcount() );

#   define PUBLISH_STYLE 0

#   if PUBLISH_STYLE==0 /* Incompatible with WAIT_STYLE==2 */

    fd_mcache_publish( mcache, depth, tx_seq, sig, chunk, sz, ctl, tsorig, tspub );

#   elif PUBLISH_STYLE==1 /* Incompatible with WAIT_STYLE==2 */

    fd_mcache_publish_sse( mcache, depth, tx_seq, sig, chunk, sz, ctl, tsorig, tspub );

#   else /* Compatible with all wait styles, requires target with atomic
            aligned AVX load/store support */

    fd_mcache_publish_avx( mcache, depth, tx_seq, sig, chunk, sz, ctl, tsorig, tspub );

#   endif

    /* Wind up for the next iteration */

    chunk  = fd_dcache_compact_next( chunk, sz, 0UL, wmark );
    tx_seq = fd_seq_inc( tx_seq, 1UL );
    cr_avail--;

    if( FD_UNLIKELY( !ctl_eom ) ) ctl_som = 0;
    else {
      ctl_som = 1;
      do {
        burst_next += (long)(0.5f + burst_tau*fd_rng_float_exp( rng ));
        burst_rem   = (ulong)(long)(0.5f + burst_avg*fd_rng_float_exp( rng ));
      } while( FD_UNLIKELY( !burst_rem ) );
    }

    /* Advance to the next test iteration */

    iter++;
    rem--;
    if( FD_UNLIKELY( !rem ) ) {
      long  toc  = fd_log_wallclock();
      float mfps = (1e3f*(float)RELOAD) / (float)(toc-tic);
      FD_LOG_NOTICE(( "%lu: %7.3f Mfrag/s tx", iter, (double)mfps ));
      for( ulong rx_idx=0UL; rx_idx<rx_cnt; rx_idx++ ) {
        ulong * rx_backp = fd_fctl_rx_backp_laddr( fctl, rx_idx );
        FD_LOG_NOTICE(( "backp[%lu] %lu", rx_idx, *rx_backp ));
        *rx_backp = 0UL;
      }
      rem = RELOAD;
      tic = fd_log_wallclock();
    }
  }
# undef RELOAD

  FD_LOG_NOTICE(( "Cleaning up" ));

  fd_mcache_seq_update( _tx_seq, tx_seq );
  fd_fctl_delete( fd_fctl_leave( fctl ) );
  fd_wksp_unmap( fd_dcache_leave( dcache ) );
  fd_wksp_unmap( fd_mcache_leave( mcache ) );
  fd_rng_delete( fd_rng_leave( rng ) );

# undef TEST

  FD_LOG_NOTICE(( "pass" ));
  fd_halt();
  return 0;
}

#else

int
main( int     argc,
      char ** argv ) {
  fd_boot( &argc, &argv );
  FD_LOG_WARNING(( "skip: unit test requires FD_HAS_HOSTED and FD_HAS_AVX capabilities" ));
  fd_halt();
  return 0;
}

#endif
