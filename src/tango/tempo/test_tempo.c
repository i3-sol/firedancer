#include "../fd_tango.h"

int
main( int     argc,
      char ** argv ) {
  fd_boot( &argc, &argv );

  fd_rng_t _rng[1]; fd_rng_t * rng = fd_rng_join( fd_rng_new( _rng, 0U, 0UL ) );

# define TEST(c) do if( FD_UNLIKELY( !(c) ) ) { FD_LOG_WARNING(( "FAIL: " #c )); return 1; } while(0)

# if FD_HAS_DOUBLE

  for( ulong iter=0; iter<10; iter++ ) {
    double tau;
    double t0 = fd_tempo_wallclock_model( &tau );
    TEST( ((0.< t0 ) & (t0 <=DBL_MAX)) );
    TEST( ((0.<=tau) & (tau<=DBL_MAX)) );

    double tau_;
    double t0_ = fd_tempo_wallclock_model( &tau_ );
    TEST( t0_ ==t0  );
    TEST( tau_==tau );
    FD_LOG_NOTICE(( "wallclock:   min %8.3f ns   tau %8.2e ns", t0, tau ));
  }

# if FD_HAS_X86

  for( ulong iter=0; iter<10; iter++ ) {
    double tau;
    double t0 = fd_tempo_tickcount_model( &tau );
    TEST( ((0.< t0 ) & (t0 <=DBL_MAX)) );
    TEST( ((0.<=tau) & (tau<=DBL_MAX)) );

    double tau_;
    double t0_ = fd_tempo_tickcount_model( &tau_ );
    TEST( t0_ ==t0  );
    TEST( tau_==tau );

    FD_LOG_NOTICE(( "tickcount:   min %8.3f tick tau %8.2e tick", t0, tau ));
  }

  for( ulong iter=0; iter<10; iter++ ) {
    double rms;
    double avg = fd_tempo_tick_per_ns( &rms );
    TEST( ((0.< avg) & (avg<=DBL_MAX)) );
    TEST( ((0.<=rms) & (rms<=DBL_MAX)) );

    double rms_;
    double avg_ = fd_tempo_tick_per_ns( &rms_ );
    TEST( avg_==avg );
    TEST( rms_==rms );
    FD_LOG_NOTICE(( "tick_per_ns: avg %8.3f ghz  rms %8.2e ghz", avg, rms ));
  }

# endif
# endif

# if FD_HAS_X86

  long now;
  long toc;
  long jot = fd_tempo_observe_pair( &now, &toc );

  for( ulong iter=0; iter<8; iter++ ) {

    long then;
    long tic;
    long jit = fd_tempo_observe_pair( &then, &tic );

    TEST( jot>=0L       );
    TEST( jit>=0L       );
    TEST( (then-now)>0L );
    TEST( (tic -toc)>0L );

    jot = fd_tempo_observe_pair( &now, &toc );

    TEST( jit>=0L       );
    TEST( jot>=0L       );
    TEST( (now-then)>0L );
    TEST( (toc-tic )>0L );

    FD_LOG_NOTICE(( "observe_pair\n\t"
                    "then %20li ns tic %20li tick jit %20li tick\n\t"
                    "now  %20li ns toc %20li tick jot %20li tick\n\t"
                    "diff %20li ns     %20li tick     %20li tick",
                    then, tic, jit, now, toc, jot, now-then, toc-tic, jot-jit ));
  }

# endif

  TEST( fd_tempo_lazy_default( 0UL       )== 9L      );
  TEST( fd_tempo_lazy_default( 1UL       )== 9L      );
  TEST( fd_tempo_lazy_default( 3UL       )== 9L      );
  TEST( fd_tempo_lazy_default( 3UL       )== 9L      );
  TEST( fd_tempo_lazy_default( 4UL       )==18L      );
  TEST( fd_tempo_lazy_default( ULONG_MAX )<=LONG_MAX );

# undef TEST

  fd_rng_delete( fd_rng_leave( rng ) );

  FD_LOG_NOTICE(( "pass" ));
  fd_halt();
  return 0;
}

