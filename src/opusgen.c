#include "opusgen.h"
#include <opus/opus.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define PACKET_SIZE 185
static unsigned char PACKET[] = {
	[0] = 0x73, [1] = 0x03, [2 ... PACKET_SIZE] = 0x00
};
#define DECODED_DATA_SIZE 1440
#define DECODED_DATA_SIZE_16 (DECODED_DATA_SIZE*sizeof(int16_t))
#define SAMPLE_RATE 48000

void random_packet(uint32_t seed) {
	uint32_t state = seed;
	for (int i = 2; i < PACKET_SIZE; i++) {
		state++;
		state = state ^ (state << 13u);
		state = state ^ (state >> 17u);
		state = state ^ (state << 5u);
		state *= 1685821657u;
		PACKET[i] = state;
	}
}

void opus_generate(uint32_t seed, int total_len, bool use_dtx, int16_t** out, size_t* out_len)
{
	random_packet(seed);
	OpusDecoder* opus_decoder = opus_decoder_create(SAMPLE_RATE, 1, NULL);

	// float empty_data[DECODED_DATA_SIZE] = { 0 };
	float decoded_data[DECODED_DATA_SIZE];
	int length;
	length = opus_decode_float(opus_decoder, PACKET, PACKET_SIZE, decoded_data, DECODED_DATA_SIZE, 0);
	(void)length;

	*out_len = total_len*DECODED_DATA_SIZE_16;
	*out = (int16_t*)malloc(*out_len);

	float last_rms = 0.;
	for (int i = 0; i < total_len; i++) {
		float rms = 0;
		for (int i = 0; i < DECODED_DATA_SIZE; i++) {
			rms += pow(decoded_data[i], 2);
		}
		rms = sqrt(rms/DECODED_DATA_SIZE);
		if (i == 0) {
			last_rms = rms;
		}
		int16_t converted_data[DECODED_DATA_SIZE];
		for (int i = 0; i < DECODED_DATA_SIZE; i++) {
			// fade in to the next rms
			float t = ((float)i)/(DECODED_DATA_SIZE-1);
			float thisscale = t*fmax(last_rms,rms) + (1-t)*last_rms;
			converted_data[i] = tanh(decoded_data[i]/thisscale/4.) * INT16_MAX;
		}
		last_rms = fmax(rms,last_rms);
		memcpy(*out+i*DECODED_DATA_SIZE, converted_data, DECODED_DATA_SIZE_16);

		if (i < 0 || !use_dtx) {
			random_packet(seed);
			length = opus_decode_float(opus_decoder, PACKET, PACKET_SIZE, decoded_data, DECODED_DATA_SIZE, 0);
		} else {
			// discontinuous transmission
			length = opus_decode_float(opus_decoder, PACKET, PACKET_SIZE, decoded_data, DECODED_DATA_SIZE, 1);
		}
		(void)length;
	}

	opus_decoder_destroy(opus_decoder);
}
