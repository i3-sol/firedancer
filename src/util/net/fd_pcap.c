#include "fd_pcap.h"

#if FD_HAS_HOSTED

#include <stdio.h>
#include <errno.h>

#define FD_PCAP_HDR_NETWORK_ETHERNET  (1U)
#define FD_PCAP_HDR_NETWORK_LINUX_SLL (113U)

struct fd_pcap_hdr {
  uint   magic_number;
  ushort version_major;
  ushort version_minor;
  int    thiszone;
  uint   sigfigs;
  uint   snaplen;
  uint   network;
};

typedef struct fd_pcap_hdr fd_pcap_hdr_t;

struct fd_pcap_pkt_hdr {
  uint sec;      /* Host order */
  uint usec;     /* Host order (code below assumes a ns capture) */
  uint incl_len; /* Host order */
  uint orig_len; /* Host order */
};

typedef struct fd_pcap_pkt_hdr fd_pcap_pkt_hdr_t;

typedef struct {
  ushort dir;
  ushort ha_type;
  ushort ha_len;
  uchar  ha[8];
  ushort net_type;
} fd_pcap_sll_hdr_t;

fd_pcap_iter_t *
fd_pcap_iter_new( void * _file ) {
  FILE * file = (FILE *)_file;
  
  if( FD_UNLIKELY( !file ) ) {
    FD_LOG_WARNING(( "NULL file" ));
    return NULL;
  }

  if( FD_UNLIKELY( !fd_ulong_is_aligned( (ulong)file, 2UL ) ) ) {
    FD_LOG_WARNING(( "misaligned file" ));
    return NULL;
  }

  fd_pcap_hdr_t pcap[1];
  if( FD_UNLIKELY( fread( pcap, sizeof(fd_pcap_hdr_t), 1UL, file ) != 1UL ) ) {
    FD_LOG_WARNING(( "fread failed (%i-%s)", errno, strerror( errno ) ));
    return NULL;
  }

  if( FD_UNLIKELY( !((pcap->magic_number==0xa1b2c3d4U) |
                     (pcap->magic_number==0xa1b23c4dU) ) ) ) {
    FD_LOG_WARNING(( "not a supported pcap file (bad magic number)" ));
    return NULL;
  }

  if( FD_UNLIKELY( !( (pcap->network==FD_PCAP_HDR_NETWORK_ETHERNET ) |
                      (pcap->network!=FD_PCAP_HDR_NETWORK_LINUX_SLL) ) ) ) {
    FD_LOG_WARNING(( "unsupported network type (neither an Ethernet nor a cooked socket pcap)" ));
    return NULL;
  }

  ulong cooked = (ulong)( pcap->network==FD_PCAP_HDR_NETWORK_LINUX_SLL );

  return (fd_pcap_iter_t *)((ulong)file | cooked);
}

ulong   
fd_pcap_iter_next( fd_pcap_iter_t * iter,
                   void *           pkt,
                   ulong            pkt_max,
                   long *           _pkt_ts ) {
  FILE * file   = (FILE *)fd_pcap_iter_file( iter );
  int    cooked = (int   )fd_pcap_iter_type( iter );

  fd_pcap_pkt_hdr_t pcap[1];
  if( FD_UNLIKELY( fread( pcap, sizeof(fd_pcap_pkt_hdr_t), 1, file ) != 1 ) ) {
    if( FD_UNLIKELY( !feof( file ) ) ) FD_LOG_WARNING(( "Could not read link header from pcap (%i-%s)", errno, strerror( errno ) ));
    return 0UL;
  }

  ulong pkt_sz = (ulong)pcap->incl_len;

  if( FD_UNLIKELY( pkt_sz!=pcap->orig_len ) ) {
    FD_LOG_WARNING(( "Read a truncated packet (%lu bytes to %u bytes), run tcpdump with '-s0' option to capture everything",
                     pkt_sz, pcap->orig_len ));
    return 0UL;
  }

  ulong pcap_hdr_sz = fd_ulong_if( cooked, sizeof(fd_pcap_sll_hdr_t), sizeof(fd_eth_hdr_t) );
  if( FD_UNLIKELY( pkt_sz<pcap_hdr_sz ) ) {
    FD_LOG_WARNING(( "Corrupt incl_len in cooked pcap file %lu", pkt_sz ));
    return 0UL;
  }

  if( FD_UNLIKELY( pkt_sz>pkt_max ) ) {
    FD_LOG_WARNING(( "Too large packet detected in pcap (%lu bytes with %lu max)", pkt_sz, pkt_max ));
    return 0UL;
  }

  fd_eth_hdr_t * hdr = (fd_eth_hdr_t *)pkt;
  if( FD_UNLIKELY( cooked ) ) {

    fd_pcap_sll_hdr_t sll[1];
    if( FD_UNLIKELY( fread( sll, sizeof(fd_pcap_sll_hdr_t), 1, file ) != 1 ) ) {
      if( FD_UNLIKELY( !feof( file ) ) ) FD_LOG_WARNING(( "packet sll header fread failed (%i-%s)", errno, strerror( errno ) ));
      else                               FD_LOG_WARNING(( "packet sll header fread failed (truncated pcap file?)" ));
      return 0UL;
    }

    /* Construct an ethernet compatible header that encodes the sll
       header info in a reasonable way */

    hdr->dst[0] = (uchar)(sll->dir    ); hdr->dst[1] = (uchar)(sll->dir     >> 8);
    hdr->dst[2] = (uchar)(sll->ha_type); hdr->dst[3] = (uchar)(sll->ha_type >> 8);
    hdr->dst[4] = (uchar)(sll->ha_len ); hdr->dst[5] = (uchar)(sll->ha_len  >> 8);
    hdr->src[0] = sll->ha[0];            hdr->src[1] = sll->ha[1];
    hdr->src[2] = sll->ha[2];            hdr->src[3] = sll->ha[3];
    hdr->src[4] = sll->ha[4];            hdr->src[5] = sll->ha[5];
    hdr->net_type = sll->net_type;

    hdr->dst[0] = (uchar)(((ulong)hdr->dst[0] & ~3UL) | 2UL); /* Mark as a local admin unicast MAC */
    hdr->src[0] = (uchar)(((ulong)hdr->src[0] & ~3UL) | 2UL); /* " */
    /* FIXME: ENCODE LOST BITS TOO? */

    pkt_sz -= sizeof(fd_pcap_sll_hdr_t);
    pkt_sz += sizeof(fd_eth_hdr_t);

  } else {

    if( FD_UNLIKELY( fread( hdr, sizeof(fd_eth_hdr_t), 1, file ) ) != 1 ) {
      if( FD_UNLIKELY( !feof( file ) ) ) FD_LOG_WARNING(( "packet eth header fread failed (%i-%s)", errno, strerror( errno ) ));
      else                               FD_LOG_WARNING(( "packet eth header fread failed (truncated pcap file?)" ));
      return 0UL;
    }

  }

  if( FD_UNLIKELY( fread( hdr+1, pkt_sz-sizeof(fd_eth_hdr_t), 1, file )!=1 ) ) {
    if( FD_UNLIKELY( !feof( file ) ) ) FD_LOG_WARNING(( "packet payload fread failed (%i-%s)", errno, strerror( errno ) ));
    else                               FD_LOG_WARNING(( "packet payload fread failed (truncated pcap file?)" ));
  }
    
  *_pkt_ts = ((long)pcap->usec) + 1000000000L*((long)pcap->sec); /* Note: assumes ns resolution capture */
  return pkt_sz;
}

#define FD_PCAP_SNAPLEN (2048UL) /* FIXME: Allow for Jumbos? */

ulong
fd_pcap_fwrite_hdr( void * file ) {
  fd_pcap_hdr_t hdr[1];
  hdr->magic_number  = 0xa1b23c4dU;
  hdr->version_major = (ushort)2;
  hdr->version_minor = (ushort)4;
  hdr->thiszone      = 0;
  hdr->sigfigs       = 0U;
  hdr->snaplen       = (uint)FD_PCAP_SNAPLEN;
  hdr->network       = 1U;
  return fwrite( hdr, sizeof(fd_pcap_hdr_t), 1UL, (FILE *)file );
}

ulong
fd_pcap_fwrite_pkt( long         ts,
                    void const * _hdr,
                    ulong        hdr_sz,
                    void const * _payload,
                    ulong        payload_sz,
                    uint         _fcs,
                    void *       file ) {

  ulong pkt_sz = hdr_sz + payload_sz;
  if( FD_UNLIKELY( ( (pkt_sz<hdr_sz) /* overflow */                                    |
                     (pkt_sz>(FD_PCAP_SNAPLEN-sizeof(fd_pcap_pkt_hdr_t)-sizeof(uint))) ) ) ) {
    FD_LOG_WARNING(( "packet size too large for pcap" ));
    return 0UL;
  }
  pkt_sz += sizeof(fd_pcap_pkt_hdr_t) + sizeof(uint);

  uchar pkt[ FD_PCAP_SNAPLEN ];

  uchar * p = pkt;
  fd_pcap_pkt_hdr_t * pcap    = (fd_pcap_pkt_hdr_t *)p; p += sizeof(fd_pcap_pkt_hdr_t);
  uchar *             hdr     = (uchar *            )p; p += hdr_sz;
  uchar *             payload = (uchar *            )p; p += payload_sz;
  uint *              fcs     = (uint *             )p; p += sizeof(uint);

  pcap->sec      = (uint)(((ulong)ts) / (ulong)1e9);
  pcap->usec     = (uint)(((ulong)ts) % (ulong)1e9); /* Actually nsec */
  pcap->incl_len = (uint)( pkt_sz - sizeof(fd_pcap_pkt_hdr_t) );
  pcap->orig_len = (uint)( pkt_sz - sizeof(fd_pcap_pkt_hdr_t) );

  memcpy( hdr,     _hdr,     hdr_sz     );
  memcpy( payload, _payload, payload_sz );
  fcs[0] = _fcs;

  if( FD_UNLIKELY( fwrite( pkt, pkt_sz, 1UL, (FILE *)file )!=1UL ) ) { FD_LOG_WARNING(( "fwrite failed" )); return 0UL; }
  return 1UL;
}

#if 0
  /* Try to detect the L2 header size and encode in the iter address.
     Note that this assumes last 2 bits of a file are 0 and that
     all packets in the pcap have same L2 header size. */
  int l2_hdr_type = 0; /* unknown header type */
  if( pcap->network==PCAP_HDR_NETWORK_LINUX_SLL ) l2_hdr_type = 1; /* pcap_iter_next returns these as untagged Ethernet */
  else {
    char  buf[ 1542 ];
    long  ts;
    ulong sz = pcap_iter_next( (pcap_iter_t *)file, &ts, buf, 1542UL );
    if( FD_UNLIKELY( sz<=14UL ) ) FD_LOG_WARNING(( "Empty pcap file, runt frame or empty ethernet payload detected" ));
    if( fd_ushort_bswap( *(ushort *)(buf+12) )!=FD_NET_ETH_HDR_TYPE_VLAN ) l2_hdr_type = 1; /* untagged Ethernet */
    else if( sz<=18UL ) FD_LOG_WARNING(( "Runt VLAN tagged frame or empty ethernet payload detected" ));
    else l2_hdr_type = 2; /* VLAN tagged Ethernet */
    fd_pcap_iter_reset( (pcap_iter_t *)file );
  }
#endif

#else

#endif

