#include <opus/opus.h>
#include <ao/ao.h>
#include <ncurses.h>
#include <locale.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#define HEADER0 "┌──────┬──────┬───┬───┬────┐"
#define HEADER1 "│ name │ seed │len│dtx│adsr│"
#define HEADER2 "┝━━━━━━┿━━━━━━┿━━━┿━━━┿━━━━┥"
#define FOOTER0 "└──────┴──────┴───┴───┴────┘"
#define DEL     "│"
#define NUMROWS 30
#define NUMCOLS 8
#define MAXNAME 6
#define MAXSEED 0x1000000
#define MAXLEN 17
#define MAXDTX 2
#define MAXTUNING 0x10

typedef struct {
	char name[MAXNAME];
	int namelen;
	int seed;
	int last_seed;
	int len;
	bool dtx;
	int a;
	int d;
	int s;
	int r;
} RowData;

#define HIGHLIGHT_IF_SEL(x, body) do { \
	if(row->namelen == 0) attron(A_DIM); \
	if(sel==x) attron(A_STANDOUT); \
	body; \
	attroff(A_STANDOUT); \
	attroff(A_DIM); \
} while(0)
void print_row(int idx, int sel, RowData* row) {
	(void) sel;
	move(3+idx, 0);
	printw("│");
	HIGHLIGHT_IF_SEL(0, printw("%.6s", row->name));
	printw("│");
	HIGHLIGHT_IF_SEL(1, printw("%06x", row->seed));
	printw("│");
	HIGHLIGHT_IF_SEL(2, printw("%3d", row->len));
	printw("│");
	HIGHLIGHT_IF_SEL(3, printw("%s", row->dtx ? " on" : "off"));
	printw("│");
	HIGHLIGHT_IF_SEL(4, printw("%01x", row->a));
	HIGHLIGHT_IF_SEL(5, printw("%01x", row->d));
	HIGHLIGHT_IF_SEL(6, printw("%01x", row->s));
	HIGHLIGHT_IF_SEL(7, printw("%01x", row->r));
	printw("│");
}

void init_row(RowData* row) {
	memset(&row->name, ' ', 6);
	row->namelen = 0;
	row->seed = 0;
	row->last_seed = 0;
	row->len = 5;
	row->dtx = 1;
	row->a = row->d = row->s = row->r = 0;
}

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

void play_random_packet(uint32_t seed, int total_len, bool use_dtx, ao_device* device) {
	random_packet(seed);
	OpusDecoder* opus_decoder = opus_decoder_create(SAMPLE_RATE, 1, NULL);

	float empty_data[DECODED_DATA_SIZE] = { 0 };
	float decoded_data[DECODED_DATA_SIZE];
	int length;
	length = opus_decode_float(opus_decoder, PACKET, PACKET_SIZE, decoded_data, DECODED_DATA_SIZE, 0);
	(void)length;

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
		ao_play(device, (char*)converted_data, DECODED_DATA_SIZE*sizeof(int16_t));
		if (i < 0 || !use_dtx) {
			random_packet(seed);
			length = opus_decode_float(opus_decoder, PACKET, PACKET_SIZE, decoded_data, DECODED_DATA_SIZE, 0);
		} else {
			// discontinuous transmission
			length = opus_decode_float(opus_decoder, PACKET, PACKET_SIZE, decoded_data, DECODED_DATA_SIZE, 1);
		}
		(void)length;
	}
	// play some empty data afterward, otherwise the audio server gets angry
	ao_play(device, (char*)empty_data, DECODED_DATA_SIZE*sizeof(int16_t));
	opus_decoder_destroy(opus_decoder);
}

int main(int argc, char** argv) {
	(void) argc; (void) argv;
	//todo:
	// [ ] print usage
	// [ ] argv[1] should be project folder
	// [ ] create folder if it does not exist
	// [ ] open/parse saved rowdata if it exists
	// [ ] dump rowdata on change or exit
	// [ ] convert generated sample to brr and back before playback
	// [ ] write preferences txt file
	// [ ] resample to 32000hz

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
	int driver_id = ao_default_driver_id();
	if (driver_id == -1) {
		fprintf(stderr, "Couldn't find default driver for libao!\n");
		return -1;
	}
	ao_device* device = ao_open_live(driver_id, &format, &option);

	// init ncurses
	setlocale(LC_ALL, "");
	initscr(); noecho(); raw(); curs_set(0);

	RowData rows[NUMROWS];
	for(int i = 0; i < NUMROWS; i++) {
		init_row(&rows[i]);
	}

	keypad(stdscr, true);
	mvprintw(0, 0, HEADER0);
	mvprintw(1, 0, HEADER1);
	mvprintw(2, 0, HEADER2);
	for(int i = 0; i < NUMROWS; i++) {
		print_row(i, i==0 ? 0 : -1, &rows[i]);
	}
	mvprintw(3+NUMROWS, 0, FOOTER0);

	int col = 0;
	int row = 0;
	int ch;
	while ((ch = getch()) > 7) {
		// mvprintw(0, 0, " %d ", ch);
		int oldrow = row;
		if (ch == KEY_DOWN)  row = (row + 1) % NUMROWS;
		if (ch == KEY_UP)    row = (row - 1 + NUMROWS) % NUMROWS;
		if (ch == KEY_RIGHT) col = (col + 1) % NUMCOLS;
		if (ch == KEY_LEFT)  col = (col - 1 + NUMCOLS) % NUMCOLS;
		if (oldrow != row) {
			print_row(oldrow, -1, &rows[oldrow]);
		}

		RowData* rowdat = &rows[row];
		bool changed = false;

		if (col == 0) {
			char chc = (char)ch;
			if (ch == KEY_BACKSPACE) {
				if (rowdat->namelen != 0) {
					rowdat->namelen -= 1;
					rowdat->name[rowdat->namelen] = ' ';
				}
			} else if (isdigit(chc) || isalpha(chc)) {
				if (rowdat->namelen < MAXNAME) {
					rowdat->name[rowdat->namelen] = chc;
					rowdat->namelen += 1;
				}
			}
		} else {
			int inc = 0;
			switch (ch) {
				case KEY_NPAGE:
				case (int)'.':
				case (int)' ':
				case (int)']':
					inc = 1;
					break;
				case KEY_PPAGE:
				case KEY_BACKSPACE:
				case (int)',':
				case (int)'[':
					inc = -1;
					break;
				default: break;
			}
			if (inc != 0) changed = true;
			if (col == 1) rowdat->seed = (rowdat->seed + MAXSEED + inc) % MAXSEED;
			if (col == 2) rowdat->len = (rowdat->len + (MAXLEN - 1) + inc - 1) % (MAXLEN-1) + 1;
			if (col == 3) rowdat->dtx = (rowdat->dtx + 2 + inc) % 2;
			if (col == 4) rowdat->a = (rowdat->a + MAXTUNING + inc) % MAXTUNING;
			if (col == 5) rowdat->d = (rowdat->d + MAXTUNING + inc) % MAXTUNING;
			if (col == 6) rowdat->s = (rowdat->s + MAXTUNING + inc) % MAXTUNING;
			if (col == 7) rowdat->r = (rowdat->r + MAXTUNING + inc) % MAXTUNING;

			if (col == 1 && ch == (int)'r') {
				rowdat->last_seed = rowdat->seed;
				rowdat->seed = rand() % MAXSEED;
				changed = true;
			}
			if (col == 1 && ch == (int)'z') {
				int tmp = rowdat->seed;
				rowdat->seed = rowdat->last_seed;
				rowdat->last_seed = tmp;
				changed = true;
			}
		}

		print_row(row, col, rowdat);
		refresh();
		if (changed || ch == (int)'\n') {
			play_random_packet(rowdat->seed, rowdat->len, rowdat->dtx, device);
			// flush input since the playback might take long
			flushinp();
		}
	}

	// destroy ncurses
	endwin();

	return 0;
}