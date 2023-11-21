/* fd_tls_client_hs_harness symbolically executes all reachable client
   handshake logic, assuming all parsers are correct. */

#include <assert.h>
#include <stdlib.h>
#include <tango/tls/fd_tls.h>
#include <tango/tls/fd_tls_base.h>

static void * tls_rand_ctx;  /* arbitrary */

static void *
tls_rand( void * ctx,
          void * buf,
          ulong  bufsz ) {
  assert( tls_rand_ctx==ctx );
  __CPROVER_w_ok( buf, bufsz );
  __CPROVER_havoc_slice( buf, bufsz );
  return buf;
}

static void
tls_secrets( void const * handshake,
             void const * recv_secret,
             void const * send_secret,
             uint         encryption_level ) {
  __CPROVER_r_ok( handshake, sizeof(fd_tls_estate_base_t ) );
  __CPROVER_r_ok( recv_secret, 32UL );
  __CPROVER_r_ok( send_secret, 32UL );
  assert( ( encryption_level==FD_TLS_LEVEL_INITIAL     ) |
          ( encryption_level==FD_TLS_LEVEL_EARLY       ) |
          ( encryption_level==FD_TLS_LEVEL_HANDSHAKE   ) |
          ( encryption_level==FD_TLS_LEVEL_APPLICATION ) );
}

static int
tls_sendmsg( void const * handshake,
             void const * record,
             ulong        record_sz,
             uint         encryption_level,
             int          flush ) {
  __CPROVER_r_ok( handshake, sizeof(fd_tls_estate_base_t ) );
  __CPROVER_r_ok( record, record_sz );
  assert( ( encryption_level==FD_TLS_LEVEL_INITIAL     ) |
          ( encryption_level==FD_TLS_LEVEL_EARLY       ) |
          ( encryption_level==FD_TLS_LEVEL_HANDSHAKE   ) |
          ( encryption_level==FD_TLS_LEVEL_APPLICATION ) );
  assert( flush==0 || flush==1 );

  int retval;  /* unconstrained */
  return retval;
}

ulong __attribute__((warn_unused_result))
tls_quic_tp_self( void *  handshake,
                  uchar * quic_tp,
                  ulong   quic_tp_bufsz ) {
  __CPROVER_r_ok( handshake, sizeof(fd_tls_estate_base_t ) );
  __CPROVER_w_ok( quic_tp, quic_tp_bufsz );
  ulong sz;  __CPROVER_assume( sz<=quic_tp_bufsz );
  __CPROVER_havoc_slice( quic_tp, sz );
  return sz;
}

void
tls_quic_tp_peer( void  *       handshake,
                  uchar const * quic_tp,
                  ulong         quic_tp_sz ) {
  __CPROVER_r_ok( handshake, sizeof(fd_tls_estate_base_t ) );
  __CPROVER_r_ok( quic_tp, quic_tp_sz );
}

void
harness( void ) {
  fd_tls_t * tls = fd_tls_join( fd_tls_new( malloc( fd_tls_footprint() ) ) );
  if( !tls ) return;

  tls->rand = (fd_tls_rand_t) {
    .ctx     = tls_rand_ctx,
    .rand_fn = tls_rand
  };
  tls->secrets_fn = tls_secrets;
  tls->sendmsg_fn = tls_sendmsg;
  if( tls->quic ) {
    tls->quic_tp_self_fn = tls_quic_tp_self;
    tls->quic_tp_peer_fn = tls_quic_tp_peer;
  }

  __CPROVER_assume( tls->cert_x509_sz==0x00UL ||  /* No x509 */
                    tls->cert_x509_sz==0xf4UL );  /* Cert size generated by fd_x509_mock */
  memcpy( tls->alpn, "\xasolana-tpu", 11UL );
  tls->alpn_sz = 11UL;

  fd_tls_estate_cli_t hs_[1];
  fd_tls_estate_cli_t * hs = fd_tls_estate_cli_new( hs_ );

  do {
    ulong record_sz;  __CPROVER_assume( record_sz>0UL && record_sz<2048UL );
    uchar record[ record_sz ];  /* unconstrained */
    uint  enc_level;  __CPROVER_assume( enc_level<=3 );

    long res = fd_tls_client_handshake( tls, hs, record, record_sz, enc_level );
    if( res<0L ) break;
  } while( hs->base.state != FD_TLS_HS_CONNECTED );

  fd_tls_estate_cli_delete( hs );
  free( fd_tls_delete( fd_tls_leave( tls ) ) );
}
