#ifndef __RIPEMD160_H__
#define __RIPEMD160_H__

#include <stdint.h>

#define RIPEMD160_BLOCK_LENGTH_ 64
#define RIPEMD160_DIGEST_LENGTH_ 20

typedef struct _CORE_RIPEMD160_CTX {
  uint32_t total[2];                      /*!< number of bytes processed  */
  uint32_t state[5];                      /*!< intermediate digest state  */
  uint8_t buffer[RIPEMD160_BLOCK_LENGTH_]; /*!< data block being processed */
} CORE_RIPEMD160_CTX;

void ripemd160_Init(CORE_RIPEMD160_CTX *ctx);
void ripemd160_Update(CORE_RIPEMD160_CTX *ctx, const uint8_t *input, uint32_t ilen);
void ripemd160_Final(CORE_RIPEMD160_CTX *ctx,
                     uint8_t output[RIPEMD160_DIGEST_LENGTH_]);
void ripemd160(const uint8_t *msg, uint32_t msg_len,
               uint8_t hash[RIPEMD160_DIGEST_LENGTH_]);

#endif
