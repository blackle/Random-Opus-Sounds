#include <ao/ao.h>
#include <ncurses.h>
#include <locale.h>
#include <ctype.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include "brrUtils.h"
#include "opusgen.h"

#define HEADER0 "┌────────┬──────┬───┬───┬────┐"
#define HEADER1 "│name    │seed  │len│dtx│adsr│"
#define HEADER2 "┝━━━━━━━━┿━━━━━━┿━━━┿━━━┿━━━━┥"
#define FOOTER0 "└────────┴──────┴───┴───┴────┘"
#define DEL     "│"
#define XSTRINGIFY(s) STRINGIFY(s)
#define STRINGIFY(x) #x
#define NUMROWS 10
#define NUMCOLS 8
#define MAXNAME 8
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
	HIGHLIGHT_IF_SEL(0, printw("%." XSTRINGIFY(MAXNAME) "s", row->name));
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
	memset(&row->name, ' ', MAXNAME);
	row->namelen = 0;
	row->seed = 0;
	row->last_seed = 0;
	row->len = 5;
	row->dtx = 1;
	row->a = row->d = row->s = row->r = 0;
}

void play_random_packet(uint32_t seed, int total_len, bool use_dtx, ao_device* device) {
	int16_t* packetdata = 0;
	size_t packetdata_len = 0;
	opus_generate(seed, total_len, use_dtx, &packetdata, &packetdata_len);
	// fprintf(stderr, "%p len=%ld\n", packetdata, packetdata_len);
	size_t packetdata_samples = packetdata_len/sizeof(int16_t);

	size_t brrlen = 9*((15+packetdata_samples)/16);
	uint8_t* brr_out = (uint8_t*)malloc(brrlen+9);
	brrEncode(packetdata, brr_out, packetdata_samples, -1, false);
	memset(packetdata, 0, packetdata_len);
	brrDecode(brr_out, packetdata, brrlen, false);
	free(brr_out);

	ao_play(device, (char*)packetdata, packetdata_len);
	// play some empty data afterward, otherwise the audio server gets angry
	// ao_play(device, (char*)empty_data, DECODED_DATA_SIZE_16);

	free(packetdata);
}

int main(int argc, char** argv) {
	(void) argc; (void) argv;
	//todo:
	// [ ] print usage
	// [ ] argv[1] should be project folder
	// [ ] create folder if it does not exist
	// [ ] open/parse saved rowdata if it exists
	// [ ] dump rowdata on change or exit
	// [/] convert generated sample to brr and back before playback
	// [ ] write preferences txt file
	// [ ] resample to 32000hz

	ao_initialize();
	ao_sample_format format = {
		.bits = 16,
		.channels = 1,
		.rate = 48000,
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