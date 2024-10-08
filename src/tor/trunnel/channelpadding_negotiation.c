/* channelpadding_negotiation.c -- generated by Trunnel v1.5.3.
 * https://gitweb.torproject.org/trunnel.git
 * You probably shouldn't edit this file.
 */
#include <stdlib.h>
#include "ext/trunnel/trunnel-impl.h"

#include "trunnel/channelpadding_negotiation.h"

#define TRUNNEL_SET_ERROR_CODE(obj) \
  do {                              \
    (obj)->trunnel_error_code_ = 1; \
  } while (0)

#if defined(__COVERITY__) || defined(__clang_analyzer__)
/* If we're running a static analysis tool, we don't want it to complain
 * that some of our remaining-bytes checks are dead-code. */
int channelpaddingnegotiation_deadcode_dummy__ = 0;
#define OR_DEADCODE_DUMMY || channelpaddingnegotiation_deadcode_dummy__
#else
#define OR_DEADCODE_DUMMY
#endif

#define CHECK_REMAINING(nbytes, label)                           \
  do {                                                           \
    if (remaining < (nbytes) OR_DEADCODE_DUMMY) {                \
      goto label;                                                \
    }                                                            \
  } while (0)

channelpadding_negotiate_t *
channelpadding_negotiate_new(void)
{
  channelpadding_negotiate_t *val = trunnel_calloc(1, sizeof(channelpadding_negotiate_t));
  if (NULL == val)
    return NULL;
  val->command = CHANNELPADDING_COMMAND_START;
  return val;
}

/** Release all storage held inside 'obj', but do not free 'obj'.
 */
static void
channelpadding_negotiate_clear(channelpadding_negotiate_t *obj)
{
  (void) obj;
}

void
channelpadding_negotiate_free(channelpadding_negotiate_t *obj)
{
  if (obj == NULL)
    return;
  channelpadding_negotiate_clear(obj);
  trunnel_memwipe(obj, sizeof(channelpadding_negotiate_t));
  trunnel_free_(obj);
}

uint8_t
channelpadding_negotiate_get_version(const channelpadding_negotiate_t *inp)
{
  return inp->version;
}
int
channelpadding_negotiate_set_version(channelpadding_negotiate_t *inp, uint8_t val)
{
  if (! ((val == 0))) {
     TRUNNEL_SET_ERROR_CODE(inp);
     return -1;
  }
  inp->version = val;
  return 0;
}
uint8_t
channelpadding_negotiate_get_command(const channelpadding_negotiate_t *inp)
{
  return inp->command;
}
int
channelpadding_negotiate_set_command(channelpadding_negotiate_t *inp, uint8_t val)
{
  if (! ((val == CHANNELPADDING_COMMAND_START || val == CHANNELPADDING_COMMAND_STOP))) {
     TRUNNEL_SET_ERROR_CODE(inp);
     return -1;
  }
  inp->command = val;
  return 0;
}
uint16_t
channelpadding_negotiate_get_ito_low_ms(const channelpadding_negotiate_t *inp)
{
  return inp->ito_low_ms;
}
int
channelpadding_negotiate_set_ito_low_ms(channelpadding_negotiate_t *inp, uint16_t val)
{
  inp->ito_low_ms = val;
  return 0;
}
uint16_t
channelpadding_negotiate_get_ito_high_ms(const channelpadding_negotiate_t *inp)
{
  return inp->ito_high_ms;
}
int
channelpadding_negotiate_set_ito_high_ms(channelpadding_negotiate_t *inp, uint16_t val)
{
  inp->ito_high_ms = val;
  return 0;
}
const char *
channelpadding_negotiate_check(const channelpadding_negotiate_t *obj)
{
  if (obj == NULL)
    return "Object was NULL";
  if (obj->trunnel_error_code_)
    return "A set function failed on this object";
  if (! (obj->version == 0))
    return "Integer out of bounds";
  if (! (obj->command == CHANNELPADDING_COMMAND_START || obj->command == CHANNELPADDING_COMMAND_STOP))
    return "Integer out of bounds";
  return NULL;
}

ssize_t
channelpadding_negotiate_encoded_len(const channelpadding_negotiate_t *obj)
{
  ssize_t result = 0;

  if (NULL != channelpadding_negotiate_check(obj))
     return -1;


  /* Length of u8 version IN [0] */
  result += 1;

  /* Length of u8 command IN [CHANNELPADDING_COMMAND_START, CHANNELPADDING_COMMAND_STOP] */
  result += 1;

  /* Length of u16 ito_low_ms */
  result += 2;

  /* Length of u16 ito_high_ms */
  result += 2;
  return result;
}
int
channelpadding_negotiate_clear_errors(channelpadding_negotiate_t *obj)
{
  int r = obj->trunnel_error_code_;
  obj->trunnel_error_code_ = 0;
  return r;
}
ssize_t
channelpadding_negotiate_encode(uint8_t *output, const size_t avail, const channelpadding_negotiate_t *obj)
{
  ssize_t result = 0;
  size_t written = 0;
  uint8_t *ptr = output;
  const char *msg;
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  const ssize_t encoded_len = channelpadding_negotiate_encoded_len(obj);
#endif

  if (NULL != (msg = channelpadding_negotiate_check(obj)))
    goto check_failed;

#ifdef TRUNNEL_CHECK_ENCODED_LEN
  trunnel_assert(encoded_len >= 0);
#endif

  /* Encode u8 version IN [0] */
  trunnel_assert(written <= avail);
  if (avail - written < 1)
    goto truncated;
  trunnel_set_uint8(ptr, (obj->version));
  written += 1; ptr += 1;

  /* Encode u8 command IN [CHANNELPADDING_COMMAND_START, CHANNELPADDING_COMMAND_STOP] */
  trunnel_assert(written <= avail);
  if (avail - written < 1)
    goto truncated;
  trunnel_set_uint8(ptr, (obj->command));
  written += 1; ptr += 1;

  /* Encode u16 ito_low_ms */
  trunnel_assert(written <= avail);
  if (avail - written < 2)
    goto truncated;
  trunnel_set_uint16(ptr, trunnel_htons(obj->ito_low_ms));
  written += 2; ptr += 2;

  /* Encode u16 ito_high_ms */
  trunnel_assert(written <= avail);
  if (avail - written < 2)
    goto truncated;
  trunnel_set_uint16(ptr, trunnel_htons(obj->ito_high_ms));
  written += 2; ptr += 2;


  trunnel_assert(ptr == output + written);
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  {
    trunnel_assert(encoded_len >= 0);
    trunnel_assert((size_t)encoded_len == written);
  }

#endif

  return written;

 truncated:
  result = -2;
  goto fail;
 check_failed:
  (void)msg;
  result = -1;
  goto fail;
 fail:
  trunnel_assert(result < 0);
  return result;
}

/** As channelpadding_negotiate_parse(), but do not allocate the
 * output object.
 */
static ssize_t
channelpadding_negotiate_parse_into(channelpadding_negotiate_t *obj, const uint8_t *input, const size_t len_in)
{
  const uint8_t *ptr = input;
  size_t remaining = len_in;
  ssize_t result = 0;
  (void)result;

  /* Parse u8 version IN [0] */
  CHECK_REMAINING(1, truncated);
  obj->version = (trunnel_get_uint8(ptr));
  remaining -= 1; ptr += 1;
  if (! (obj->version == 0))
    goto fail;

  /* Parse u8 command IN [CHANNELPADDING_COMMAND_START, CHANNELPADDING_COMMAND_STOP] */
  CHECK_REMAINING(1, truncated);
  obj->command = (trunnel_get_uint8(ptr));
  remaining -= 1; ptr += 1;
  if (! (obj->command == CHANNELPADDING_COMMAND_START || obj->command == CHANNELPADDING_COMMAND_STOP))
    goto fail;

  /* Parse u16 ito_low_ms */
  CHECK_REMAINING(2, truncated);
  obj->ito_low_ms = trunnel_ntohs(trunnel_get_uint16(ptr));
  remaining -= 2; ptr += 2;

  /* Parse u16 ito_high_ms */
  CHECK_REMAINING(2, truncated);
  obj->ito_high_ms = trunnel_ntohs(trunnel_get_uint16(ptr));
  remaining -= 2; ptr += 2;
  trunnel_assert(ptr + remaining == input + len_in);
  return len_in - remaining;

 truncated:
  return -2;
 fail:
  result = -1;
  return result;
}

ssize_t
channelpadding_negotiate_parse(channelpadding_negotiate_t **output, const uint8_t *input, const size_t len_in)
{
  ssize_t result;
  *output = channelpadding_negotiate_new();
  if (NULL == *output)
    return -1;
  result = channelpadding_negotiate_parse_into(*output, input, len_in);
  if (result < 0) {
    channelpadding_negotiate_free(*output);
    *output = NULL;
  }
  return result;
}
