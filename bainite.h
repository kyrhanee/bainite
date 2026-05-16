#ifndef BAINITE_H
#define BAINITE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BAINITE_OK                     0
#define BAINITE_ERR_NULL              -1
#define BAINITE_ERR_INVALID_UTF8      -2
#define BAINITE_ERR_IO               -10
#define BAINITE_ERR_INVALID_PE       -20
#define BAINITE_ERR_UNSUPPORTED_ARCH -21
#define BAINITE_ERR_ALREADY_PACKED   -22
#define BAINITE_ERR_NOT_PACKED       -23
#define BAINITE_ERR_COMPRESS         -30
#define BAINITE_ERR_INTERNAL         -90
#define BAINITE_ERR_BUFFER_TOO_SMALL -91

typedef struct {
    int32_t  level;
    uint8_t  algorithm;
    uint8_t  filter;
    uint8_t  encrypt;
    uint8_t  auto_strategy;
    uint8_t  _reserved[12];
} BainiteOptions;

typedef struct {
    uint64_t original_size;
    uint64_t packed_size;
    uint64_t uncompressed_size;
    uint8_t  algorithm;
    uint8_t  filter;
    int32_t  level;
    uint64_t elapsed_ms;
    uint8_t  _reserved[8];
} BainiteReport;

const char*    bainite_version(void);
BainiteOptions bainite_default_options(void);
int32_t        bainite_last_error(char* buf, size_t buf_len);

int32_t bainite_pack_file(const char* input_path,
                          const char* output_path,
                          const BainiteOptions* options,
                          BainiteReport* report);

int32_t bainite_pack_buffer(const uint8_t* input_ptr,
                            size_t input_len,
                            const BainiteOptions* options,
                            uint8_t* output_ptr,
                            size_t output_capacity,
                            size_t* output_len,
                            BainiteReport* report);

int32_t bainite_is_packed(const char* path);
int32_t bainite_inspect(const char* path, BainiteReport* report);

#ifdef __cplusplus
}
#endif

#endif
