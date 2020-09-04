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
		// state *= 1685821657u;
		PACKET[i] = state;
	}
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
	ao_device* device = ao_open_live(ao_driver_id("pulse"), &format, &option);
	// ao_device* device = ao_open_file(ao_driver_id("wav"), "samples.wav", 1, &format, &option);
	(void) device;

	for (int j = 0; j < 32; j++) {
	random_packet(42342+j);
	OpusDecoder* opus_decoder = opus_decoder_create(SAMPLE_RATE, 1, NULL);
	for (int k = 0; k < 4; k++) {
	float decoded_data[DECODED_DATA_SIZE];
	int length;
	length = opus_decode_float(opus_decoder, PACKET, PACKET_SIZE, decoded_data, DECODED_DATA_SIZE, 0);
	(void)length;
	float rms = 0.;

	// float top = decoded_data[DECODED_DATA_SIZE-1];
	// float bottom = decoded_data[0];
	// float avg = (top+bottom)/2;

	for (int i = 0; i < DECODED_DATA_SIZE; i++) {
		// float t = ((float)i)/(DECODED_DATA_SIZE-1);
		// decoded_data[i] -= t*top + (1-t)*bottom - avg;
		rms += pow(decoded_data[i], 2);
	}
	rms = sqrt(rms/DECODED_DATA_SIZE);

	for (int i = 0; i < 10; i++) {
		float rms2 = 0;
		for (int i = 0; i < DECODED_DATA_SIZE; i++) {
			// float t = ((float)i)/(DECODED_DATA_SIZE-1);
			// decoded_data[i] -= t*top + (1-t)*bottom - avg;
			rms2 += pow(decoded_data[i], 2);
		}
		rms2 = sqrt(rms2/DECODED_DATA_SIZE);
		int16_t converted_data[DECODED_DATA_SIZE];
		for (int i = 0; i < DECODED_DATA_SIZE; i++) {
			float t = ((float)i)/(DECODED_DATA_SIZE-1);
			float thisscale = t*fmax(rms,rms2) + (1-t)*rms;
			converted_data[i] = tanh(decoded_data[i]/thisscale/4.) * INT16_MAX;
		}
		rms = fmax(rms,rms2);
		ao_play(device, (char*)converted_data, DECODED_DATA_SIZE*sizeof(int16_t));
		if (i < 0) {
			random_packet(422+j%4*7483+i*432);
			length = opus_decode_float(opus_decoder, PACKET, PACKET_SIZE, decoded_data, DECODED_DATA_SIZE, 0);
		} else {
			length = opus_decode_float(opus_decoder, PACKET, PACKET_SIZE, decoded_data, DECODED_DATA_SIZE, 1);
		}

		(void) length;
	}
	}
}

	return 0;
}