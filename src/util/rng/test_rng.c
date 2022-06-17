#include "../fd_util.h"
#include <math.h>

/* FIXME: ADD COVERAGE FOR ROLL */

static void
log_ref( ulong const * ref,
         uint          seq,
         ulong         idx ) {
  FD_LOG_NOTICE(( "ulong x%u_%lu[10] = {", seq, idx ));
  for( int i=0; i<10; i++ ) FD_LOG_NOTICE(( "  0x%016lxUL%s\n", ref[i], i<9 ? "," : "" ));
  FD_LOG_NOTICE(( "};" ));
}

int
main( int     argc,
      char ** argv ) {

  fd_boot( &argc, &argv );

# define TEST(c) do if( !(c) ) { FD_LOG_WARNING(( "FAIL: " #c )); return 1; } while(0)

  FD_LOG_NOTICE(( "Testing footprint / align" ));

  TEST( fd_rng_footprint()==sizeof (fd_rng_t) );
  TEST( fd_rng_align    ()==alignof(fd_rng_t) );

  FD_LOG_NOTICE(( "Testing new" ));

  fd_rng_t _rng[1];
  void * shrng = fd_rng_new( _rng, 0U, 0UL ); TEST( !!shrng );

  FD_LOG_NOTICE(( "Testing join" ));

  fd_rng_t * rng  = fd_rng_join( shrng ); TEST( !!rng );

  FD_LOG_NOTICE(( "Testing seq and idx" ));

  TEST( fd_rng_seq( rng )==0U  );
  TEST( fd_rng_idx( rng )==0UL );

  static ulong const x0_0[10] = {
    0xa036f9b67313c1aaUL,
    0x110a4ea5e65927d2UL,
    0x9ddf34cf83d17c94UL,
    0xeb33d1b534e210ecUL,
    0x9b15b94d3e81b76aUL,
    0xee9544dab2bf64bfUL,
    0x5c4b0ccf7c94d274UL,
    0xf0f83ab44262ad1fUL,
    0xf11b1aa14c7dabd6UL,
    0x3800dde6d02d6ed7UL
  };

  ulong x[10];
  for( int i=0; i<10; i++ ) x[i] = fd_rng_ulong( rng );
  for( int i=0; i<10; i++ )
    if( x[i]!=x0_0[i] ) {
      FD_LOG_WARNING(( "FAIL(0,0)" ));
      log_ref( x0_0, 0U, 0UL );
      log_ref( x,    0U, 0UL );
      return 1;
    }

  TEST( fd_rng_seq( rng )== 0U  );
  TEST( fd_rng_idx( rng )==20UL );

  TEST( fd_rng_seq_set( rng, 1U )==0U );

  static ulong const x1_20[10] = {
    0x55369a6a0817cbceUL,
    0xce1baeb695229132UL,
    0x0e443e81e9e722d2UL,
    0xc6c065484f76e825UL,
    0x37dc474806fabc8aUL,
    0x9fa3305df1b56824UL,
    0xf3b3961a17ed881cUL,
    0x646f40006cef8d6fUL,
    0x4d6955f607a153b2UL,
    0xf806bccb58d0e60bUL
  };

  for( int i=0; i<10; i++ ) x[i] = fd_rng_ulong( rng );
  for( int i=0; i<10; i++ )
    if( x[i]!=x1_20[i] ) {
      FD_LOG_WARNING(( "FAIL(1,20)" ));
      log_ref( x1_20, 1U, 20UL );
      log_ref( x,     1U, 20UL );
      return 1;
    }

  TEST( fd_rng_seq( rng )== 1U  );
  TEST( fd_rng_idx( rng )==40UL );

  TEST( fd_rng_seq_set( rng, 0U  )== 1U  );
  TEST( fd_rng_idx_set( rng, 0UL )==40UL );

  for( int i=0; i<10; i++ ) TEST( fd_rng_ulong( rng )==x0_0[i] );

  FD_LOG_NOTICE(( "Testing generator domains" ));

  TEST( fd_rng_uint_to_float_c0  ( 0U  )==0.f ); TEST( fd_rng_uint_to_float_c0  ( ~0U  )< 1.f );
  TEST( fd_rng_uint_to_float_c1  ( 0U  )> 0.f ); TEST( fd_rng_uint_to_float_c1  ( ~0U  )==1.f );
  TEST( fd_rng_uint_to_float_c   ( 0U  )==0.f ); TEST( fd_rng_uint_to_float_c   ( ~0U  )==1.f );
  TEST( fd_rng_uint_to_float_o   ( 0U  )> 0.f ); TEST( fd_rng_uint_to_float_o   ( ~0U  )< 1.f );
# if FD_HAS_DOUBLE
  TEST( fd_rng_ulong_to_double_c0( 0UL )==0.  ); TEST( fd_rng_ulong_to_double_c0( ~0UL )< 1.  );
  TEST( fd_rng_ulong_to_double_c1( 0UL )> 0.  ); TEST( fd_rng_ulong_to_double_c1( ~0UL )==1.  );
  TEST( fd_rng_ulong_to_double_c ( 0UL )==0.  ); TEST( fd_rng_ulong_to_double_c ( ~0UL )==1.  );
  TEST( fd_rng_ulong_to_double_o ( 0UL )> 0.  ); TEST( fd_rng_ulong_to_double_o ( ~0UL )< 1.  );
# endif

  ulong domain = 0UL;
  for( int i=0; i<1000; i++ ) {
    uchar   u8   = fd_rng_uchar  ( rng );
    ushort  u16  = fd_rng_ushort ( rng );
    uint    u32  = fd_rng_uint   ( rng );
    ulong   u64  = fd_rng_ulong  ( rng );
#   if FD_HAS_INT128
    uint128 u128 = fd_rng_uint128( rng );
#   endif
    schar   i8   = fd_rng_schar  ( rng ); TEST( i8  >=(schar)0  );
    short   i16  = fd_rng_short  ( rng ); TEST( i16 >=(short)0  );
    int     i32  = fd_rng_int    ( rng ); TEST( i32 >=       0  );
    long    i64  = fd_rng_long   ( rng ); TEST( i64 >=       0L );
#   if FD_HAS_INT128
    int128  i128 = fd_rng_int128 ( rng ); TEST( i128>=(int128)0 );
#   endif

    float  fc0 = fd_rng_float_c0    ( rng ); TEST(      0.f<=fc0 && fc0< 1.f     );
    float  fc1 = fd_rng_float_c1    ( rng ); TEST(      0.f< fc1 && fc1<=1.f     );
    float  fc  = fd_rng_float_c     ( rng ); TEST(      0.f<=fc  && fc <=1.f     );
    float  fnc = fd_rng_float_o     ( rng ); TEST(      0.f< fnc && fnc< 1.f     );
    float  fr  = fd_rng_float_robust( rng ); TEST(      0.f<=fr  && fr <=1.f     );
    float  fe  = fd_rng_float_exp   ( rng ); TEST(      0.f<=fe  && fe <=FLT_MAX );
    float  fn  = fd_rng_float_norm  ( rng ); TEST( -FLT_MAX<=fn  && fn <=FLT_MAX );
#   if FD_HAS_DOUBLE
    double dc0 = fd_rng_double_c0    ( rng ); TEST(       0.<=dc0 && dc0< 1.      );
    double dc1 = fd_rng_double_c1    ( rng ); TEST(       0.< dc1 && dc1<=1.      );
    double dc  = fd_rng_double_c     ( rng ); TEST(       0.<=dc  && dc <=1.      );
    double dnc = fd_rng_double_o     ( rng ); TEST(       0.< dnc && dnc< 1.      );
    double dr  = fd_rng_double_robust( rng ); TEST(       0.<=dr  && dr <=1.      );
    double de  = fd_rng_double_exp   ( rng ); TEST(       0.<=de  && de <=DBL_MAX );
    double dn  = fd_rng_double_norm  ( rng ); TEST( -DBL_MAX<=dn  && dn <=DBL_MAX );
#   endif

    ulong  ct  = fd_rng_coin_tosses( rng );

#   define LO(i,w) ((ulong)((ulong)(i >> (w-6))==0x00UL))
#   define HI(i,w) ((ulong)((ulong)(i >> (w-6))==0x3fUL))
    domain |= LO(u8,    8) <<  0; domain |= HI(u8,    8) <<  1;
    domain |= LO(u16,  16) <<  2; domain |= HI(u16,  16) <<  3;
    domain |= LO(u32,  32) <<  4; domain |= HI(u32,  32) <<  5;
    domain |= LO(u64,  64) <<  6; domain |= HI(u64,  64) <<  7;
#   if FD_HAS_INT128
    domain |= LO(u128,128) <<  8; domain |= HI(u128,128) <<  9;
#   else
    domain |= 1UL          <<  8; domain |= 1UL          <<  9;
#   endif
    domain |= LO(i8,    7) << 10; domain |= HI(i8,    7) << 11;
    domain |= LO(i16,  15) << 12; domain |= HI(i16,  15) << 13;
    domain |= LO(i32,  31) << 14; domain |= HI(i32,  31) << 15;
    domain |= LO(i64,  63) << 16; domain |= HI(i64,  63) << 17;
#   if FD_HAS_INT128
    domain |= LO(i128,127) << 18; domain |= HI(i128,127) << 19;
#   else
    domain |= 1UL          << 18; domain |= 1UL          << 19;
#   endif
#   undef HI
#   undef LO

#   define LO(f) ((ulong)(f<0.02f))
#   define HI(f) ((ulong)(f>0.98f))
    domain |= LO(fc0) << 20; domain |= HI(fc0) << 21;
    domain |= LO(fc1) << 22; domain |= HI(fc1) << 23;
    domain |= LO(fc ) << 24; domain |= HI(fc ) << 25;
    domain |= LO(fnc) << 26; domain |= HI(fnc) << 27;
    domain |= LO(fr ) << 28; domain |= HI(fr ) << 29;
#   undef HI
#   undef LO
#   define LO(d) ((ulong)(d<0.02))
#   define HI(d) ((ulong)(d>0.98))
#   if FD_HAS_DOUBLE
    domain |= LO(dc0) << 30; domain |= HI(dc0) << 31;
    domain |= LO(dc1) << 32; domain |= HI(dc1) << 33;
    domain |= LO(dc ) << 34; domain |= HI(dc ) << 35;
    domain |= LO(dnc) << 36; domain |= HI(dnc) << 37;
    domain |= LO(dr ) << 38; domain |= HI(dr ) << 39;
#   else
    domain |= 1UL     << 30; domain |= 1UL     << 31;
    domain |= 1UL     << 32; domain |= 1UL     << 33;
    domain |= 1UL     << 34; domain |= 1UL     << 35;
    domain |= 1UL     << 36; domain |= 1UL     << 37;
    domain |= 1UL     << 38; domain |= 1UL     << 39;
#   endif
#   undef HI
#   undef LO

#   define LO(f) ((ulong)(f<0.1f))
#   define HI(f) ((ulong)(f>4.0f))
    domain |= LO(fe ) << 40; domain |= HI(fe ) << 41;
#   undef HI
#   undef LO
#   define LO(d) ((ulong)(d<0.1))
#   define HI(d) ((ulong)(d>4.0))
#   if FD_HAS_DOUBLE
    domain |= LO(de ) << 42; domain |= HI(de ) << 43;
#   else
    domain |= 1UL     << 42; domain |= 1UL     << 43;
#   endif
#   undef HI
#   undef LO

#   define LO(f) ((ulong)(f<-2.0f))
#   define HI(f) ((ulong)(f> 2.0f))
    domain |= LO(fn ) << 44; domain |= HI(fn ) << 45;
#   undef HI
#   undef LO
#   define LO(d) ((ulong)(d<-2.0))
#   define HI(d) ((ulong)(d> 2.0))
#   if FD_HAS_DOUBLE
    domain |= LO(dn ) << 46; domain |= HI(dn ) << 47;
#   else
    domain |= 1UL     << 46; domain |= 1UL     << 47;
#   endif
#   undef HI
#   undef LO

#   define LO(u) ((ulong)(u<2UL))
#   define HI(u) ((ulong)(u>5UL))
    domain |= LO(ct ) << 48; domain |= HI(ct ) << 49;
#   undef HI
#   undef LO
  }

  TEST( domain==fd_ulong_mask_lsb(50) );

  FD_LOG_NOTICE(( "Testing seed expansion" ));

  long sum_pop  = 0L;
  long sum_pop2 = 0L;
  int  min_pop  = INT_MAX;
  int  max_pop  = INT_MIN;

  int ctr = 0;
  for( long i=0; i<(1L<<32); i++ ) {
    if( !ctr ) { FD_LOG_NOTICE(( "Completed %li iterations", i )); ctr = 100000000; }
    ctr--;

    ulong seq = fd_rng_private_expand( (uint)i ); TEST( fd_rng_private_contract( seq )==(uint)i );
    int pop  = fd_ulong_popcnt( seq );
    sum_pop  += (long) pop;
    sum_pop2 += (long)(pop*pop);
    min_pop  = pop<min_pop ? pop : min_pop;
    max_pop  = pop>max_pop ? pop : max_pop;
  }
  float avg_pop = ((float)sum_pop ) / ((float)(1L<<32));
  float rms_pop = sqrtf( (((float)sum_pop2) / ((float)(1L<<32))) - avg_pop*avg_pop );
  FD_LOG_NOTICE(( "expand popcount stats: %.3f +/- %.3f [%i,%i]", (double)avg_pop, (double)rms_pop, min_pop, max_pop ));

  FD_LOG_NOTICE(( "Testing leave" ));

  TEST( fd_rng_leave( rng )==shrng );

  FD_LOG_NOTICE(( "Testing delete" ));

  TEST( fd_rng_delete( shrng )==_rng );

# undef TEST

  FD_LOG_NOTICE(( "pass" ));
  fd_halt();
  return 0;
}

