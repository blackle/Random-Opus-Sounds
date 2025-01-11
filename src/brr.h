#pragma once
#include <stdint.h>
#include <stddef.h>

// encode audio sample into brr
void encode_brr(int16_t* in, size_t in_len, uint8_t** out, size_t* out_len);

// decode brr into audio sample
void decode_brr(uint8_t* in, size_t in_len, int16_t** out, size_t* out_len);