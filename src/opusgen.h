#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// caller responsible for freeing pointer put into "out"

// allocate an audio sample using opus
void opus_generate(uint32_t seed, int total_len, bool use_dtx, int16_t** out, size_t* out_len);