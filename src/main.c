#include <opus/opus.h>
#include <ao/ao.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#define PACKET_SIZE 185
static unsigned char PACKET[] = {
	[0] = 0x73, [1] = 0x03, [2 ... PACKET_SIZE] = 0x00
};
#define DECODED_DATA_SIZE 1440
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

void play_random_packet(uint32_t seed, ao_device* device) {
	random_packet(seed);
	OpusDecoder* opus_decoder = opus_decoder_create(SAMPLE_RATE, 1, NULL);

	float decoded_data[DECODED_DATA_SIZE];
	int length;
	length = opus_decode_float(opus_decoder, PACKET, PACKET_SIZE, decoded_data, DECODED_DATA_SIZE, 0);
	(void)length;

	float last_rms = 0.;
	for (int i = 0; i < 10; i++) {
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
		ao_play(device, (char*)converted_data, DECODED_DATA_SIZE*sizeof(int16_t));
		if (i < 0) {
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

int main(int argc, char** argv) {
	(void) argc; (void) argv;
	ao_initialize();
	ao_sample_format format = {
		.bits = 16,
		.channels = 1,
		.rate = SAMPLE_RATE,
		.byte_format = AO_FMT_NATIVE,
		.matrix = NULL,
	};
	ao_option option = {
		.key = "buffer_time",
		.value = "200",
		.next = NULL,
	};
	(void) option;
	// ao_device* device = ao_open_live(ao_driver_id("pulse"), &format, &option);

	for (int j = 0; j < 100; j++) {
		char filename[80];
		snprintf(filename, 80, "samples/opus_%02d.wav", j);
		printf("writing %s\n", filename);
		ao_device* device = ao_open_file(ao_driver_id("wav"), filename, 1, &format, NULL);
		play_random_packet(j, device);
	}

	return 0;
}