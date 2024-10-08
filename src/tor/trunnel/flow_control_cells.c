/* flow_control_cells.c -- generated by Trunnel v1.5.3.
 * https://gitweb.torproject.org/trunnel.git
 * You probably shouldn't edit this file.
 */
#include <stdlib.h>
#include "ext/trunnel/trunnel-impl.h"

#include "trunnel/flow_control_cells.h"

#define TRUNNEL_SET_ERROR_CODE(obj) \
  do {                              \
    (obj)->trunnel_error_code_ = 1; \
  } while (0)

#if defined(__COVERITY__) || defined(__clang_analyzer__)
/* If we're running a static analysis tool, we don't want it to complain
 * that some of our remaining-bytes checks are dead-code. */
int flowcontrolcells_deadcode_dummy__ = 0;
#define OR_DEADCODE_DUMMY || flowcontrolcells_deadcode_dummy__
#else
#define OR_DEADCODE_DUMMY
#endif

#define CHECK_REMAINING(nbytes, label)                           \
  do {                                                           \
    if (remaining < (nbytes) OR_DEADCODE_DUMMY) {                \
      goto label;                                                \
    }                                                            \
  } while (0)

xoff_cell_t *
xoff_cell_new(void)
{
  xoff_cell_t *val = trunnel_calloc(1, sizeof(xoff_cell_t));
  if (NULL == val)
    return NULL;
  return val;
}

/** Release all storage held inside 'obj', but do not free 'obj'.
 */
static void
xoff_cell_clear(xoff_cell_t *obj)
{
  (void) obj;
}

void
xoff_cell_free(xoff_cell_t *obj)
{
  if (obj == NULL)
    return;
  xoff_cell_clear(obj);
  trunnel_memwipe(obj, sizeof(xoff_cell_t));
  trunnel_free_(obj);
}

uint8_t
xoff_cell_get_version(const xoff_cell_t *inp)
{
  return inp->version;
}
int
xoff_cell_set_version(xoff_cell_t *inp, uint8_t val)
{
  if (! ((val == 0))) {
     TRUNNEL_SET_ERROR_CODE(inp);
     return -1;
  }
  inp->version = val;
  return 0;
}
const char *
xoff_cell_check(const xoff_cell_t *obj)
{
  if (obj == NULL)
    return "Object was NULL";
  if (obj->trunnel_error_code_)
    return "A set function failed on this object";
  if (! (obj->version == 0))
    return "Integer out of bounds";
  return NULL;
}

ssize_t
xoff_cell_encoded_len(const xoff_cell_t *obj)
{
  ssize_t result = 0;

  if (NULL != xoff_cell_check(obj))
     return -1;


  /* Length of u8 version IN [0] */
  result += 1;
  return result;
}
int
xoff_cell_clear_errors(xoff_cell_t *obj)
{
  int r = obj->trunnel_error_code_;
  obj->trunnel_error_code_ = 0;
  return r;
}
ssize_t
xoff_cell_encode(uint8_t *output, const size_t avail, const xoff_cell_t *obj)
{
  ssize_t result = 0;
  size_t written = 0;
  uint8_t *ptr = output;
  const char *msg;
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  const ssize_t encoded_len = xoff_cell_encoded_len(obj);
#endif

  if (NULL != (msg = xoff_cell_check(obj)))
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

/** As xoff_cell_parse(), but do not allocate the output object.
 */
static ssize_t
xoff_cell_parse_into(xoff_cell_t *obj, const uint8_t *input, const size_t len_in)
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
  trunnel_assert(ptr + remaining == input + len_in);
  return len_in - remaining;

 truncated:
  return -2;
 fail:
  result = -1;
  return result;
}

ssize_t
xoff_cell_parse(xoff_cell_t **output, const uint8_t *input, const size_t len_in)
{
  ssize_t result;
  *output = xoff_cell_new();
  if (NULL == *output)
    return -1;
  result = xoff_cell_parse_into(*output, input, len_in);
  if (result < 0) {
    xoff_cell_free(*output);
    *output = NULL;
  }
  return result;
}
xon_cell_t *
xon_cell_new(void)
{
  xon_cell_t *val = trunnel_calloc(1, sizeof(xon_cell_t));
  if (NULL == val)
    return NULL;
  return val;
}

/** Release all storage held inside 'obj', but do not free 'obj'.
 */
static void
xon_cell_clear(xon_cell_t *obj)
{
  (void) obj;
}

void
xon_cell_free(xon_cell_t *obj)
{
  if (obj == NULL)
    return;
  xon_cell_clear(obj);
  trunnel_memwipe(obj, sizeof(xon_cell_t));
  trunnel_free_(obj);
}

uint8_t
xon_cell_get_version(const xon_cell_t *inp)
{
  return inp->version;
}
int
xon_cell_set_version(xon_cell_t *inp, uint8_t val)
{
  if (! ((val == 0))) {
     TRUNNEL_SET_ERROR_CODE(inp);
     return -1;
  }
  inp->version = val;
  return 0;
}
uint32_t
xon_cell_get_kbps_ewma(const xon_cell_t *inp)
{
  return inp->kbps_ewma;
}
int
xon_cell_set_kbps_ewma(xon_cell_t *inp, uint32_t val)
{
  inp->kbps_ewma = val;
  return 0;
}
const char *
xon_cell_check(const xon_cell_t *obj)
{
  if (obj == NULL)
    return "Object was NULL";
  if (obj->trunnel_error_code_)
    return "A set function failed on this object";
  if (! (obj->version == 0))
    return "Integer out of bounds";
  return NULL;
}

ssize_t
xon_cell_encoded_len(const xon_cell_t *obj)
{
  ssize_t result = 0;

  if (NULL != xon_cell_check(obj))
     return -1;


  /* Length of u8 version IN [0] */
  result += 1;

  /* Length of u32 kbps_ewma */
  result += 4;
  return result;
}
int
xon_cell_clear_errors(xon_cell_t *obj)
{
  int r = obj->trunnel_error_code_;
  obj->trunnel_error_code_ = 0;
  return r;
}
ssize_t
xon_cell_encode(uint8_t *output, const size_t avail, const xon_cell_t *obj)
{
  ssize_t result = 0;
  size_t written = 0;
  uint8_t *ptr = output;
  const char *msg;
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  const ssize_t encoded_len = xon_cell_encoded_len(obj);
#endif

  if (NULL != (msg = xon_cell_check(obj)))
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

  /* Encode u32 kbps_ewma */
  trunnel_assert(written <= avail);
  if (avail - written < 4)
    goto truncated;
  trunnel_set_uint32(ptr, trunnel_htonl(obj->kbps_ewma));
  written += 4; ptr += 4;


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

/** As xon_cell_parse(), but do not allocate the output object.
 */
static ssize_t
xon_cell_parse_into(xon_cell_t *obj, const uint8_t *input, const size_t len_in)
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

  /* Parse u32 kbps_ewma */
  CHECK_REMAINING(4, truncated);
  obj->kbps_ewma = trunnel_ntohl(trunnel_get_uint32(ptr));
  remaining -= 4; ptr += 4;
  trunnel_assert(ptr + remaining == input + len_in);
  return len_in - remaining;

 truncated:
  return -2;
 fail:
  result = -1;
  return result;
}

ssize_t
xon_cell_parse(xon_cell_t **output, const uint8_t *input, const size_t len_in)
{
  ssize_t result;
  *output = xon_cell_new();
  if (NULL == *output)
    return -1;
  result = xon_cell_parse_into(*output, input, len_in);
  if (result < 0) {
    xon_cell_free(*output);
    *output = NULL;
  }
  return result;
}
