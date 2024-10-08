/* cell_rendezvous.h -- generated by Trunnel v1.5.3.
 * https://gitweb.torproject.org/trunnel.git
 * You probably shouldn't edit this file.
 */
#ifndef TRUNNEL_CELL_RENDEZVOUS_H
#define TRUNNEL_CELL_RENDEZVOUS_H

#include <stdint.h>
#include "ext/trunnel/trunnel.h"

#define TRUNNEL_REND_COOKIE_LEN 20
#define TRUNNEL_HANDSHAKE_INFO_LEN 64
#if !defined(TRUNNEL_OPAQUE) && !defined(TRUNNEL_OPAQUE_TRN_CELL_RENDEZVOUS1)
struct trn_cell_rendezvous1_st {
  uint8_t rendezvous_cookie[TRUNNEL_REND_COOKIE_LEN];
  TRUNNEL_DYNARRAY_HEAD(, uint8_t) handshake_info;
  uint8_t trunnel_error_code_;
};
#endif
typedef struct trn_cell_rendezvous1_st trn_cell_rendezvous1_t;
#if !defined(TRUNNEL_OPAQUE) && !defined(TRUNNEL_OPAQUE_TRN_CELL_RENDEZVOUS2)
struct trn_cell_rendezvous2_st {
  uint8_t handshake_info[TRUNNEL_HANDSHAKE_INFO_LEN];
  uint8_t trunnel_error_code_;
};
#endif
typedef struct trn_cell_rendezvous2_st trn_cell_rendezvous2_t;
/** Return a newly allocated trn_cell_rendezvous1 with all elements
 * set to zero.
 */
trn_cell_rendezvous1_t *trn_cell_rendezvous1_new(void);
/** Release all storage held by the trn_cell_rendezvous1 in 'victim'.
 * (Do nothing if 'victim' is NULL.)
 */
void trn_cell_rendezvous1_free(trn_cell_rendezvous1_t *victim);
/** Try to parse a trn_cell_rendezvous1 from the buffer in 'input',
 * using up to 'len_in' bytes from the input buffer. On success,
 * return the number of bytes consumed and set *output to the newly
 * allocated trn_cell_rendezvous1_t. On failure, return -2 if the
 * input appears truncated, and -1 if the input is otherwise invalid.
 */
ssize_t trn_cell_rendezvous1_parse(trn_cell_rendezvous1_t **output, const uint8_t *input, const size_t len_in);
/** Return the number of bytes we expect to need to encode the
 * trn_cell_rendezvous1 in 'obj'. On failure, return a negative value.
 * Note that this value may be an overestimate, and can even be an
 * underestimate for certain unencodeable objects.
 */
ssize_t trn_cell_rendezvous1_encoded_len(const trn_cell_rendezvous1_t *obj);
/** Try to encode the trn_cell_rendezvous1 from 'input' into the
 * buffer at 'output', using up to 'avail' bytes of the output buffer.
 * On success, return the number of bytes used. On failure, return -2
 * if the buffer was not long enough, and -1 if the input was invalid.
 */
ssize_t trn_cell_rendezvous1_encode(uint8_t *output, size_t avail, const trn_cell_rendezvous1_t *input);
/** Check whether the internal state of the trn_cell_rendezvous1 in
 * 'obj' is consistent. Return NULL if it is, and a short message if
 * it is not.
 */
const char *trn_cell_rendezvous1_check(const trn_cell_rendezvous1_t *obj);
/** Clear any errors that were set on the object 'obj' by its setter
 * functions. Return true iff errors were cleared.
 */
int trn_cell_rendezvous1_clear_errors(trn_cell_rendezvous1_t *obj);
/** Return the (constant) length of the array holding the
 * rendezvous_cookie field of the trn_cell_rendezvous1_t in 'inp'.
 */
size_t trn_cell_rendezvous1_getlen_rendezvous_cookie(const trn_cell_rendezvous1_t *inp);
/** Return the element at position 'idx' of the fixed array field
 * rendezvous_cookie of the trn_cell_rendezvous1_t in 'inp'.
 */
uint8_t trn_cell_rendezvous1_get_rendezvous_cookie(trn_cell_rendezvous1_t *inp, size_t idx);
/** As trn_cell_rendezvous1_get_rendezvous_cookie, but take and return
 * a const pointer
 */
uint8_t trn_cell_rendezvous1_getconst_rendezvous_cookie(const trn_cell_rendezvous1_t *inp, size_t idx);
/** Change the element at position 'idx' of the fixed array field
 * rendezvous_cookie of the trn_cell_rendezvous1_t in 'inp', so that
 * it will hold the value 'elt'.
 */
int trn_cell_rendezvous1_set_rendezvous_cookie(trn_cell_rendezvous1_t *inp, size_t idx, uint8_t elt);
/** Return a pointer to the TRUNNEL_REND_COOKIE_LEN-element array
 * field rendezvous_cookie of 'inp'.
 */
uint8_t * trn_cell_rendezvous1_getarray_rendezvous_cookie(trn_cell_rendezvous1_t *inp);
/** As trn_cell_rendezvous1_get_rendezvous_cookie, but take and return
 * a const pointer
 */
const uint8_t  * trn_cell_rendezvous1_getconstarray_rendezvous_cookie(const trn_cell_rendezvous1_t *inp);
/** Return the length of the dynamic array holding the handshake_info
 * field of the trn_cell_rendezvous1_t in 'inp'.
 */
size_t trn_cell_rendezvous1_getlen_handshake_info(const trn_cell_rendezvous1_t *inp);
/** Return the element at position 'idx' of the dynamic array field
 * handshake_info of the trn_cell_rendezvous1_t in 'inp'.
 */
uint8_t trn_cell_rendezvous1_get_handshake_info(trn_cell_rendezvous1_t *inp, size_t idx);
/** As trn_cell_rendezvous1_get_handshake_info, but take and return a
 * const pointer
 */
uint8_t trn_cell_rendezvous1_getconst_handshake_info(const trn_cell_rendezvous1_t *inp, size_t idx);
/** Change the element at position 'idx' of the dynamic array field
 * handshake_info of the trn_cell_rendezvous1_t in 'inp', so that it
 * will hold the value 'elt'.
 */
int trn_cell_rendezvous1_set_handshake_info(trn_cell_rendezvous1_t *inp, size_t idx, uint8_t elt);
/** Append a new element 'elt' to the dynamic array field
 * handshake_info of the trn_cell_rendezvous1_t in 'inp'.
 */
int trn_cell_rendezvous1_add_handshake_info(trn_cell_rendezvous1_t *inp, uint8_t elt);
/** Return a pointer to the variable-length array field handshake_info
 * of 'inp'.
 */
uint8_t * trn_cell_rendezvous1_getarray_handshake_info(trn_cell_rendezvous1_t *inp);
/** As trn_cell_rendezvous1_get_handshake_info, but take and return a
 * const pointer
 */
const uint8_t  * trn_cell_rendezvous1_getconstarray_handshake_info(const trn_cell_rendezvous1_t *inp);
/** Change the length of the variable-length array field
 * handshake_info of 'inp' to 'newlen'.Fill extra elements with 0.
 * Return 0 on success; return -1 and set the error code on 'inp' on
 * failure.
 */
int trn_cell_rendezvous1_setlen_handshake_info(trn_cell_rendezvous1_t *inp, size_t newlen);
/** Return a newly allocated trn_cell_rendezvous2 with all elements
 * set to zero.
 */
trn_cell_rendezvous2_t *trn_cell_rendezvous2_new(void);
/** Release all storage held by the trn_cell_rendezvous2 in 'victim'.
 * (Do nothing if 'victim' is NULL.)
 */
void trn_cell_rendezvous2_free(trn_cell_rendezvous2_t *victim);
/** Try to parse a trn_cell_rendezvous2 from the buffer in 'input',
 * using up to 'len_in' bytes from the input buffer. On success,
 * return the number of bytes consumed and set *output to the newly
 * allocated trn_cell_rendezvous2_t. On failure, return -2 if the
 * input appears truncated, and -1 if the input is otherwise invalid.
 */
ssize_t trn_cell_rendezvous2_parse(trn_cell_rendezvous2_t **output, const uint8_t *input, const size_t len_in);
/** Return the number of bytes we expect to need to encode the
 * trn_cell_rendezvous2 in 'obj'. On failure, return a negative value.
 * Note that this value may be an overestimate, and can even be an
 * underestimate for certain unencodeable objects.
 */
ssize_t trn_cell_rendezvous2_encoded_len(const trn_cell_rendezvous2_t *obj);
/** Try to encode the trn_cell_rendezvous2 from 'input' into the
 * buffer at 'output', using up to 'avail' bytes of the output buffer.
 * On success, return the number of bytes used. On failure, return -2
 * if the buffer was not long enough, and -1 if the input was invalid.
 */
ssize_t trn_cell_rendezvous2_encode(uint8_t *output, size_t avail, const trn_cell_rendezvous2_t *input);
/** Check whether the internal state of the trn_cell_rendezvous2 in
 * 'obj' is consistent. Return NULL if it is, and a short message if
 * it is not.
 */
const char *trn_cell_rendezvous2_check(const trn_cell_rendezvous2_t *obj);
/** Clear any errors that were set on the object 'obj' by its setter
 * functions. Return true iff errors were cleared.
 */
int trn_cell_rendezvous2_clear_errors(trn_cell_rendezvous2_t *obj);
/** Return the (constant) length of the array holding the
 * handshake_info field of the trn_cell_rendezvous2_t in 'inp'.
 */
size_t trn_cell_rendezvous2_getlen_handshake_info(const trn_cell_rendezvous2_t *inp);
/** Return the element at position 'idx' of the fixed array field
 * handshake_info of the trn_cell_rendezvous2_t in 'inp'.
 */
uint8_t trn_cell_rendezvous2_get_handshake_info(trn_cell_rendezvous2_t *inp, size_t idx);
/** As trn_cell_rendezvous2_get_handshake_info, but take and return a
 * const pointer
 */
uint8_t trn_cell_rendezvous2_getconst_handshake_info(const trn_cell_rendezvous2_t *inp, size_t idx);
/** Change the element at position 'idx' of the fixed array field
 * handshake_info of the trn_cell_rendezvous2_t in 'inp', so that it
 * will hold the value 'elt'.
 */
int trn_cell_rendezvous2_set_handshake_info(trn_cell_rendezvous2_t *inp, size_t idx, uint8_t elt);
/** Return a pointer to the TRUNNEL_HANDSHAKE_INFO_LEN-element array
 * field handshake_info of 'inp'.
 */
uint8_t * trn_cell_rendezvous2_getarray_handshake_info(trn_cell_rendezvous2_t *inp);
/** As trn_cell_rendezvous2_get_handshake_info, but take and return a
 * const pointer
 */
const uint8_t  * trn_cell_rendezvous2_getconstarray_handshake_info(const trn_cell_rendezvous2_t *inp);


#endif
