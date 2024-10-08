/* channelpadding_negotiation.h -- generated by Trunnel v1.5.3.
 * https://gitweb.torproject.org/trunnel.git
 * You probably shouldn't edit this file.
 */
#ifndef TRUNNEL_CHANNELPADDING_NEGOTIATION_H
#define TRUNNEL_CHANNELPADDING_NEGOTIATION_H

#include <stdint.h>
#include "ext/trunnel/trunnel.h"

#define CHANNELPADDING_COMMAND_STOP 1
#define CHANNELPADDING_COMMAND_START 2
#if !defined(TRUNNEL_OPAQUE) && !defined(TRUNNEL_OPAQUE_CHANNELPADDING_NEGOTIATE)
struct channelpadding_negotiate_st {
  uint8_t version;
  uint8_t command;
  uint16_t ito_low_ms;
  uint16_t ito_high_ms;
  uint8_t trunnel_error_code_;
};
#endif
typedef struct channelpadding_negotiate_st channelpadding_negotiate_t;
/** Return a newly allocated channelpadding_negotiate with all
 * elements set to zero.
 */
channelpadding_negotiate_t *channelpadding_negotiate_new(void);
/** Release all storage held by the channelpadding_negotiate in
 * 'victim'. (Do nothing if 'victim' is NULL.)
 */
void channelpadding_negotiate_free(channelpadding_negotiate_t *victim);
/** Try to parse a channelpadding_negotiate from the buffer in
 * 'input', using up to 'len_in' bytes from the input buffer. On
 * success, return the number of bytes consumed and set *output to the
 * newly allocated channelpadding_negotiate_t. On failure, return -2
 * if the input appears truncated, and -1 if the input is otherwise
 * invalid.
 */
ssize_t channelpadding_negotiate_parse(channelpadding_negotiate_t **output, const uint8_t *input, const size_t len_in);
/** Return the number of bytes we expect to need to encode the
 * channelpadding_negotiate in 'obj'. On failure, return a negative
 * value. Note that this value may be an overestimate, and can even be
 * an underestimate for certain unencodeable objects.
 */
ssize_t channelpadding_negotiate_encoded_len(const channelpadding_negotiate_t *obj);
/** Try to encode the channelpadding_negotiate from 'input' into the
 * buffer at 'output', using up to 'avail' bytes of the output buffer.
 * On success, return the number of bytes used. On failure, return -2
 * if the buffer was not long enough, and -1 if the input was invalid.
 */
ssize_t channelpadding_negotiate_encode(uint8_t *output, size_t avail, const channelpadding_negotiate_t *input);
/** Check whether the internal state of the channelpadding_negotiate
 * in 'obj' is consistent. Return NULL if it is, and a short message
 * if it is not.
 */
const char *channelpadding_negotiate_check(const channelpadding_negotiate_t *obj);
/** Clear any errors that were set on the object 'obj' by its setter
 * functions. Return true iff errors were cleared.
 */
int channelpadding_negotiate_clear_errors(channelpadding_negotiate_t *obj);
/** Return the value of the version field of the
 * channelpadding_negotiate_t in 'inp'
 */
uint8_t channelpadding_negotiate_get_version(const channelpadding_negotiate_t *inp);
/** Set the value of the version field of the
 * channelpadding_negotiate_t in 'inp' to 'val'. Return 0 on success;
 * return -1 and set the error code on 'inp' on failure.
 */
int channelpadding_negotiate_set_version(channelpadding_negotiate_t *inp, uint8_t val);
/** Return the value of the command field of the
 * channelpadding_negotiate_t in 'inp'
 */
uint8_t channelpadding_negotiate_get_command(const channelpadding_negotiate_t *inp);
/** Set the value of the command field of the
 * channelpadding_negotiate_t in 'inp' to 'val'. Return 0 on success;
 * return -1 and set the error code on 'inp' on failure.
 */
int channelpadding_negotiate_set_command(channelpadding_negotiate_t *inp, uint8_t val);
/** Return the value of the ito_low_ms field of the
 * channelpadding_negotiate_t in 'inp'
 */
uint16_t channelpadding_negotiate_get_ito_low_ms(const channelpadding_negotiate_t *inp);
/** Set the value of the ito_low_ms field of the
 * channelpadding_negotiate_t in 'inp' to 'val'. Return 0 on success;
 * return -1 and set the error code on 'inp' on failure.
 */
int channelpadding_negotiate_set_ito_low_ms(channelpadding_negotiate_t *inp, uint16_t val);
/** Return the value of the ito_high_ms field of the
 * channelpadding_negotiate_t in 'inp'
 */
uint16_t channelpadding_negotiate_get_ito_high_ms(const channelpadding_negotiate_t *inp);
/** Set the value of the ito_high_ms field of the
 * channelpadding_negotiate_t in 'inp' to 'val'. Return 0 on success;
 * return -1 and set the error code on 'inp' on failure.
 */
int channelpadding_negotiate_set_ito_high_ms(channelpadding_negotiate_t *inp, uint16_t val);


#endif
