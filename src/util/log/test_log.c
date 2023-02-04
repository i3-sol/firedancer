#include "../fd_util.h"

int volatile volatile_yes = 1;

static void
backtrace_test( void ) {
  if( volatile_yes ) FD_LOG_CRIT((    "Test CRIT         (warning + backtrace and abort program)" ));
  if( volatile_yes ) FD_LOG_ALERT((   "Test ALERT        (warning + backtrace and abort program)" ));
  if( volatile_yes ) FD_LOG_EMERG((   "Test EMERG        (warning + backtrace and abort program)" ));
}

int
main( int     argc,
      char ** argv ) {
  fd_boot( &argc, &argv );

  /* Non-cancelling log messages */
  FD_LOG_DEBUG((   "Test DEBUG        (silent)"                                ));
  FD_LOG_INFO((    "Test INFO         (log file only)"                         ));
  FD_LOG_NOTICE((  "Test NOTICE       (info+shortend to stderr)"               ));
  FD_LOG_WARNING(( "Test WARNING      (notice+flush log and stderr)"           ));

  /* Info about the calling thread */
  FD_LOG_NOTICE(( "fd_log_level_logfile %i",  fd_log_level_logfile() ));
  FD_LOG_NOTICE(( "fd_log_level_stderr  %i",  fd_log_level_stderr()  ));
  FD_LOG_NOTICE(( "fd_log_level_flush   %i",  fd_log_level_flush()   ));
  FD_LOG_NOTICE(( "fd_log_level_core    %i",  fd_log_level_core()    ));
  FD_LOG_NOTICE(( "fd_log_app_id        %lu", fd_log_app_id()        ));
  FD_LOG_NOTICE(( "fd_log_thread_id     %lu", fd_log_thread_id()     ));
  FD_LOG_NOTICE(( "fd_log_host_id       %lu", fd_log_host_id()       ));
  FD_LOG_NOTICE(( "fd_log_cpu_id        %lu", fd_log_cpu_id()        ));
  FD_LOG_NOTICE(( "fd_log_group_id      %lu", fd_log_group_id()      ));
  FD_LOG_NOTICE(( "fd_log_tid           %lu", fd_log_tid()           ));
  FD_LOG_NOTICE(( "fd_log_app           %s",  fd_log_app()           ));
  FD_LOG_NOTICE(( "fd_log_thread        %s",  fd_log_thread()        ));
  FD_LOG_NOTICE(( "fd_log_host          %s",  fd_log_host()          ));
  FD_LOG_NOTICE(( "fd_log_cpu           %s",  fd_log_cpu()           ));
  FD_LOG_NOTICE(( "fd_log_group         %s",  fd_log_group()         ));
  FD_LOG_NOTICE(( "fd_log_user          %s",  fd_log_user()          ));

  FD_LOG_NOTICE( ( "Testing hexdump logging API: " ) );
  FD_LOG_HEXDUMP( "very_small_blob", "a", strlen( "a" ) );
  FD_LOG_HEXDUMP( "test_blob", "aaaa", strlen( "aaaa" ) );
  FD_LOG_HEXDUMP( "alphabet_blob", "abcdefghijklmnopqrstuvwxyz", strlen( "abcdefghijklmnopqrstuvwxyz" ) );
  FD_LOG_HEXDUMP( "empty_blob", "abcdefghijklmnopqrstuvwxyz", 0 );

  char unprintable_blob[] = "\xff\x00\xff\x82\x90\x02\x05\x09\xff\x00\xff\x82\x90\x02\x05\x09"
                            "\xff\x00\xff\x82\x90\x02\x05\x09\xff\x00\xff\x82\x90\x02\x05\x09"
                            "\xff\x00\xff\x82\x90\x02\x05\x09\xff\x00\xff\x82\x90\x02\x05\x09"
                            "\xff\x00\xff\x82\x90\x02\x05\x09\xff\x00\xff\x82\x90\x02\x05\x09";
  FD_LOG_HEXDUMP( "hex_unprintable_blob", unprintable_blob, 64 );

  char test_blob[] = "\xff\x00\xff\x82\x90\x02\x05\x09\xff\x00\xff\x82\x90\x02\x05\x09"
                     "\xff\x00\xff\x82\x90\x02\x61\x09\xff\x00\xff\x82\x90\x02\x05\x09"
                     "\x66\x90\x69\x05\x72\xff\x65\xff\x64\x90\x61\x05\x6e\x00\x63\x08"
                     "\x65\x00\x72\x82\x90\x02\x05\x09\xff\x78\xff\x72\x90\x02\x05\x09"
                     "\xff\x00\x41\x82\x45\x02\x05\x09\xff\x78\xff\x72\x90\x02\x05\x09"
                     "\xff\x00\xff\x82\x90\x02\x05\x09\xff\x78\xff\x72\x90\x02\x05\x09"
                     "\xff\x00\xff\x82\x90\x55\x05\x09\xff\x78\xff\x72\x90\x02\x05\x09"
                     "\x50\x00\x44\x82\x90\x02\x05\x09\xff\x78\xff\x72\x90\x02\x05\x09";
  FD_LOG_HEXDUMP( "mixed_printable_and_unprintable_blob", test_blob, 128 );

  char *large_blob = fd_alloca( alignof(char), 8000 );
  memset( large_blob, 0x62, 8000 );
  FD_LOG_HEXDUMP( "large_blob", large_blob, 8000 );

  FD_LOG_NOTICE(( "Testing log levels" ));
  int i;
  i = fd_log_level_logfile(); fd_log_level_logfile_set(i-1); FD_TEST( fd_log_level_logfile()==i-1 ); fd_log_level_logfile_set(i);
  i = fd_log_level_stderr();  fd_log_level_stderr_set (i-1); FD_TEST( fd_log_level_stderr() ==i-1 ); fd_log_level_stderr_set (i);
  i = fd_log_level_flush();   fd_log_level_flush_set  (i-1); FD_TEST( fd_log_level_flush()  ==i-1 ); fd_log_level_flush_set  (i);
  i = fd_log_level_core();    fd_log_level_core_set   (i-1); FD_TEST( fd_log_level_core()   ==i-1 ); fd_log_level_core_set   (i);

  FD_LOG_NOTICE(( "Setting thread name" ));
  fd_log_thread_set( "main-thread" );
  if( strcmp( fd_log_thread(), "main-thread" ) ) FD_LOG_ERR(( "FAIL: fd_log_thread_set" ));

  FD_LOG_NOTICE(( "Setting cpu name" ));
  fd_log_cpu_set( "main-cpu" );
  if( strcmp( fd_log_cpu(), "main-cpu" ) ) FD_LOG_ERR(( "FAIL: fd_log_cpu_set" ));

  /* Rudimentary wallclock tests */
  long tic;
  long toc = fd_log_wallclock();
  for( int i=0; i<10; i++ ) { tic = toc; toc = fd_log_wallclock(); }
  FD_LOG_NOTICE((  "Test wallclock overhead %li ns", toc-tic ));

  tic = fd_log_wallclock();
  toc = (long)1e9; do toc = fd_log_sleep( toc ); while( toc );
  tic = fd_log_wallclock() - (tic+(long)1e9);
  FD_LOG_NOTICE(( "Test fd_log_sleep delta %li ns", tic ));
  FD_TEST( fd_long_abs( tic ) < (ulong)25e6 );

  tic = fd_log_wallclock();
  tic = fd_log_wait_until( tic+(long)1e9 ) - (tic+(long)1e9);
  FD_LOG_NOTICE(( "Test fd_log_wait_until delta %li ns", tic ));
  FD_TEST( fd_long_abs( tic ) < (ulong)25e3 );

  /* Debugging helpers */
  static uchar const hex[16] = { 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf };
  FD_LOG_NOTICE((  "Test hex          " FD_LOG_HEX16_FMT, FD_LOG_HEX16_FMT_ARGS( hex ) ));

  if( volatile_yes ) for( int i=0; i<20000000; i++ ) FD_LOG_NOTICE(( "dup" ));

  FD_LOG_NOTICE((  "Test fd_log_flush" ));
  fd_log_flush();

  /* Ensure FD_TEST doesn't interpret line as a format string.  Note
     strict clang compiles don't permit implicit conversion of a cstr to
     a logical so we avoid those tests if FD_USING_CLANG.  Arguably, we
     could do !!"foo" here instead but that in some sense defeats the
     point of these tests. */

# if !FD_USING_CLANG
  FD_TEST( "%n %n %n %n %n %n %n %n %n %n %n %n %n" );
  FD_TEST( "\"\\\"" );
# endif

  /* Cancelling log messages */
  if( !volatile_yes ) FD_LOG_ERR((     "Test ERR          (warning+exit program with error 1)"     ));
  if( !volatile_yes ) backtrace_test();

  FD_LOG_NOTICE(( "pass" ));
  fd_halt();
  return 0;
}

