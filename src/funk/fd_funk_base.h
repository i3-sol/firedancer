#ifndef HEADER_fd_src_funk_fd_funk_base_h
#define HEADER_fd_src_funk_fd_funk_base_h

/* Funk terminology / concepts:

   - A funk instance stores records.

   - A record is a key-value pair.

   - keys are a fixed length fd_funk_rec_key_t.

   - values are variable size arbitrary binary data with a upper bound
     to the size.

   - Records are indexed by key.

   - A funk transaction describes changes to the funk records.

   - A transactions has a globally unique identifier and a parent
     transaction.

   - Transactions with children cannot be modified.

   - The chain of transactions through a transaction's ancestors
     (its parent, grandparent, great-grandparent, ...) provides a
     history of the funk all the way back the "root" transaction.

   - A transaction can be either in preparation or published.

   - The ancestors of a published transaction cannot be modified.

   - In preparation transactions can be cancelled.

   - Cancelling a transaction will discard all funk record updates for
     that transaction and any descendant transactions.

   - Published transactions cannot be cancelled.

   - Critically, competing/parallel transaction histories are allowed.

   - A user can to read / write all funk records for the most recently
     published transactions and all transactions locally in preparation. */

#include "../util/fd_util.h"

/* FD_FUNK_SUCCESS is used by various APIs to indicate the operation
   successfully completed.  This will be 0.  FD_FUNK_ERR_* gives a
   number of error codes used by fd_funk APIs.  These will be negative
   integers. */

#define FD_FUNK_SUCCESS   (0)  /* Success */


#define FD_FUNK_ERR_INVAL (-1) /* E.g. Failed due to invalid inputs passed by caller */
#define FD_FUNK_ERR_KEY   (-2) /* E.g. Failed due to record key not found */
#define FD_FUNK_ERR_TXN   (-3) /* E.g. Failed due to transaction txn not found */
#define FD_FUNK_ERR_MEM   (-4) /* E.g. Failed due to no suitable wksp memory available */
#define FD_FUNK_ERR_IO    (-5) /* E.g. Failed due to persistent backing store I/O issue */

/* FD_FUNK_REC_KEY_{ALIGN,FOOTPRINT} describe the alignment and
   footprint of a fd_funk_rec_key_t.  ALIGN is a positive integer power
   of 2.  FOOTPRINT is a multiple of ALIGN.  These are provided to
   facilitate compile time declarations. */

#define FD_FUNK_REC_KEY_ALIGN     (32UL)
#define FD_FUNK_REC_KEY_FOOTPRINT (64UL)

/* A fd_funk_rec_key_t identifies a funk record.  Compact binary keys
   are encouraged but a cstr can be used so long as it has
   strlen(cstr)<FD_FUNK_REC_KEY_FOOTPRINT and the characters c[i] for i
   in [strlen(cstr),FD_FUNK_REC_KEY_FOOTPRINT) zero.  (Also, if encoding
   a cstr in a key, recommend using first byte to encode the strlen for
   accelerating cstr operations further but this is up to the user.) */

union __attribute__((aligned(FD_FUNK_REC_KEY_ALIGN))) fd_funk_rec_key {
  char  c [ FD_FUNK_REC_KEY_FOOTPRINT ];
  uchar uc[ FD_FUNK_REC_KEY_FOOTPRINT ];
  ulong ul[ FD_FUNK_REC_KEY_FOOTPRINT / sizeof(ulong) ];
};

typedef union fd_funk_rec_key fd_funk_rec_key_t;

/* FD_FUNK_REC_VAL_MAX is the maximum size of a record value.  The
   current value is aligned with Solana usages. */

#define FD_FUNK_REC_VAL_MAX (10UL<<20) /* 10 MiB */

/* FD_FUNK_TXN_ID_{ALIGN,FOOTPRINT} describe the alignment and footprint
   of a fd_funk_txn_id_t.  ALIGN is a positive integer power of 2.
   FOOTPRINT is a multiple of ALIGN.  These are provided to facilitate
   compile time declarations. */

#define FD_FUNK_TXN_ID_ALIGN     (32UL)
#define FD_FUNK_TXN_ID_FOOTPRINT (32UL)

/* A fd_funk_txn_id_t identifies a funk transaction.  Compact binary
   identifiers are encouraged but a cstr can be used so long as it has
   strlen(cstr)<FD_FUNK_TXN_ID_FOOTPRINT and characters c[i] for i in
   [strlen(cstr),FD_FUNK_TXN_KEY_FOOTPRINT) zero.  (Also, if encoding a
   cstr in a transaction id, recommend using first byte to encode the
   strlen for accelerating cstr operations even further but this is more
   up to the application.) */

union __attribute__((aligned(FD_FUNK_TXN_ID_ALIGN))) fd_funk_txn_id {
  char  c [ FD_FUNK_TXN_ID_FOOTPRINT ];
  uchar uc[ FD_FUNK_TXN_ID_FOOTPRINT ];
  ulong ul[ FD_FUNK_TXN_ID_FOOTPRINT / sizeof(ulong) ];
};

typedef union fd_funk_txn_id fd_funk_txn_id_t;

/* A fd_funk_t * is an opaque handle of a local join to a funk instance */

struct fd_funk_private;
typedef struct fd_funk_private fd_funk_t;

FD_PROTOTYPES_BEGIN

/* fd_funk_rec_key_hash provides a family of hashes that hash the key
   pointed to by k to a uniform quasi-random 64-bit integer.  seed
   selects the particular hash function to use and can be an arbitrary
   64-bit value.  Returns the hash.  The hash functions are high quality
   but not cryptographically secure.  Assumes k is in the caller's
   address space and valid. */

FD_FN_UNUSED FD_FN_PURE static ulong /* Workaround -Winline */
fd_funk_rec_key_hash( fd_funk_rec_key_t const * k,
                      ulong                     seed ) {
  return ( (fd_ulong_hash( seed ^ (1UL<<0) ^ k->ul[0] ) ^ fd_ulong_hash( seed ^ (1UL<<1) ^ k->ul[1] ) ) ^
           (fd_ulong_hash( seed ^ (1UL<<2) ^ k->ul[2] ) ^ fd_ulong_hash( seed ^ (1UL<<3) ^ k->ul[3] ) ) ) ^
         ( (fd_ulong_hash( seed ^ (1UL<<4) ^ k->ul[4] ) ^ fd_ulong_hash( seed ^ (1UL<<5) ^ k->ul[5] ) ) ^
           (fd_ulong_hash( seed ^ (1UL<<6) ^ k->ul[6] ) ^ fd_ulong_hash( seed ^ (1UL<<7) ^ k->ul[7] ) ) ); /* tons of ILP */
}

/* fd_funk_rec_key_eq returns 1 if keys pointed to by ka and kb are
   equal and 0 otherwise.  Assumes ka and kb are in the caller's address
   space and valid. */

FD_FN_UNUSED FD_FN_PURE static int /* Workaround -Winline */
fd_funk_rec_key_eq( fd_funk_rec_key_t const * ka,
                    fd_funk_rec_key_t const * kb ) {
  ulong const * a = ka->ul;
  ulong const * b = kb->ul;
  return !( (((a[0]^b[0]) | (a[1]^b[1])) | ((a[2]^b[2]) | (a[3]^b[3]))) |
            (((a[4]^b[4]) | (a[5]^b[5])) | ((a[6]^b[6]) | (a[7]^b[7]))) );
}

/* fd_funk_rec_key_copy copies the key pointed to by ks into the key
   pointed to by kd and returns kd.  Assumes kd and ks are in the
   caller's address space and valid. */

static inline fd_funk_rec_key_t *
fd_funk_rec_key_copy( fd_funk_rec_key_t *       kd,
                      fd_funk_rec_key_t const * ks ) {
  ulong *       d = kd->ul;
  ulong const * s = ks->ul;
  d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
  d[4] = s[4]; d[5] = s[5]; d[6] = s[6]; d[7] = s[7];
  return kd;
}

/* fd_funk_txn_id_hash provides a family of hashes that hash the id
   pointed to by x to a uniform quasi-random 64-bit integer.  seed
   selects the particular hash function to use and can be an arbitrary
   64-bit value.  Returns the hash.  The hash functions are high quality
   but not cryptographically secure.  Assumes x is in the caller's
   address space and valid. */
   
FD_FN_UNUSED FD_FN_PURE static ulong /* Work around -Winline */
fd_funk_txn_id_hash( fd_funk_txn_id_t const * x,
                     ulong                    seed ) {
  return ( fd_ulong_hash( seed ^ (1UL<<0) ^ x->ul[0] ) ^ fd_ulong_hash( seed ^ (1UL<<1) ^ x->ul[1] ) ) ^
         ( fd_ulong_hash( seed ^ (1UL<<2) ^ x->ul[2] ) ^ fd_ulong_hash( seed ^ (1UL<<3) ^ x->ul[3] ) ); /* tons of ILP */
}

/* fd_funk_txn_id_eq returns 1 if transaction id pointed to by xa and xb
   are equal and 0 otherwise.  Assumes xa and xb are in the caller's
   address space and valid. */

FD_FN_PURE static inline int
fd_funk_txn_id_eq( fd_funk_txn_id_t const * xa,
                   fd_funk_txn_id_t const * xb ) {
  ulong const * a = xa->ul;
  ulong const * b = xb->ul;
  return !( (a[0]^b[0]) | (a[1]^b[1]) | (a[2]^b[2]) | (a[3]^b[3]) );
}

/* fd_funk_txn_id_copy copies the transaction id pointed to by xs into
   the transaction id pointed to by xd and returns xd.  Assumes xd and
   xs are in the caller's address space and valid. */

static inline fd_funk_txn_id_t *
fd_funk_txn_id_copy( fd_funk_txn_id_t *       xd,
                     fd_funk_txn_id_t const * xs ) {
  ulong *       d = xd->ul;
  ulong const * s = xs->ul;
  d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
  return xd;
}

/* fd_funk_txn_id_eq_root returns 1 if transaction id pointed to by x is
   the root transaction.  Assumes x is in the caller's address space and
   valid. */

FD_FN_PURE static inline int
fd_funk_txn_id_eq_root( fd_funk_txn_id_t const * x ) {
  ulong const * a = x->ul;
  return !(a[0] | a[1] | a[2] | a[3]);
}

/* fd_funk_txn_id_set_root sets transaction id pointed to by x to the
   root transaction and returns x.  Assumes x is in the caller's address
   space and valid. */

static inline fd_funk_txn_id_t *
fd_funk_txn_id_set_root( fd_funk_txn_id_t * x ) {
  ulong * a = x->ul;
  a[0] = 0UL; a[1] = 0UL; a[2] = 0UL; a[3] = 0UL;
  return x;
}

/* fd_funk_strerror converts an FD_FUNK_SUCCESS / FD_FUNK_ERR_* code
   into a human readable cstr.  The lifetime of the returned pointer is
   infinite.  The returned pointer is always to a non-NULL cstr. */

FD_FN_CONST char const *
fd_funk_strerror( int err );

/* TODO: Consider renaming transaction/txn to update (too much typing)?
   upd (probably too similar to UDP) node? block? blk? state? commit?
   ... to reduce naming collisions with terminology in use elsewhere?

   TODO: Consider fine tuning {REC,TXN}_{ALIGN,FOOTPRINT} to balance
   application use cases with in memory packing with AVX / CPU cache
   friendly accelerability.  Likewise, virtually everything in here can
   be AVX accelerated if desired.  E.g. 8 uint hash in parallel then an
   8 wide xor lane reduction tree in hash?

   TODO: Consider letting the user provide alternatives for record and
   transaction hashes at compile time (e.g. ids in blockchain apps are
   often already crypto secure hashes in which case x->ul[0] ^ seed is
   just as good theoretically and faster practically). */

FD_PROTOTYPES_END

#endif /* HEADER_fd_src_funk_fd_funk_base_h */

