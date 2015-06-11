// TODO find library that can increase tempo, is lightweight and fast.

#define _GNU_SOURCE

#include <libgen.h>
#include <ncurses.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>

#define TEMPO_DELTA .05

static int max_col, max_row;
static int quit;
static char* mode_line;
static FILE* audio_f;

struct progress_bar {
  int row, col;
  int len;
  float progress;
} pbar;

struct song {
  int len;
  int time;
  float tempo;
  char* name;
} current_song;

void seek_seconds(int);
void set_tempo(float);
void show_help();
void show_main();
void show_progress_bar();
void show_song_info();

// Prints text centered horizontally.
void printw_center_x(int row, int max_col, char* fmt, ...) {
  char* str = NULL;
  va_list args;

  va_start(args, fmt);

  vasprintf(&str, fmt, args);
  mvprintw(row, (max_col / 2) - (strlen(str) / 2), str);

  va_end(args);

  free(str);
}

// TODO use a callback?
void seek_seconds(int n) {
  current_song.time = MIN(MAX(n, 0), current_song.len);
  pbar.progress = (float) current_song.time / current_song.len;
  show_progress_bar();
  show_song_info();
}

void set_tempo(float f) {
  current_song.tempo = MAX(0, f);
  show_song_info();
}

// Prints a help menu with all commands.
void show_help() {
  clear();

  printw_center_x(1, max_col, "cscribe help:\n\n");

  printw("': Jump to mark\n");
  printw("<: Decrease tempo\n");
  printw(">: Increase tempo\n");
  printw("h: Show / exit this help menu\n");
  printw("j: Back 2 seconds\n");
  printw("k: Forward 2 seconds\n");
  printw("m: Create mark\n");
  printw("o: Open file\n");
  printw("q: Quit cscribe\n");

  refresh();

  getch();

  show_main();
}

// Displays the main screen.
void show_main() {
  int ch;
  int mid_row, mid_col;
  char* welcome_msg = "Welcome to cscribe!\n\n";

  mid_row = max_row / 2;
  mid_col = max_col / 2;

  clear();

  // TODO we should use a separate window for the modeline.
  if (mode_line == NULL) {
    mode_line = "Type h for the list of all commands.";
  }

  printw_center_x(1, max_col, welcome_msg);

  if (current_song.name == NULL) {
    printw("Type ");
    addch('o' | A_BOLD);
    printw(" to open an audio file.\n");
  } else {
    show_song_info();
  }

  pbar.col = mid_col / 2;
  pbar.row = mid_row;
  pbar.len = mid_col;
  show_progress_bar();

  mvprintw(max_row - 1, 0, mode_line);
  refresh();

  while (!quit && (ch = getch())) {
    switch(ch) {
      case '<':
        set_tempo(current_song.tempo - TEMPO_DELTA);
        break;
      case '>':
        set_tempo(current_song.tempo + TEMPO_DELTA);
        break;
      case 'q':
        quit = 1;
        return;
      case 'j':
        seek_seconds(current_song.time - 2);
        break;
      case 'k':
        seek_seconds(current_song.time + 2);
        break;
      case 'h':
        show_help();
        break;
    }
  }
}

// Places a progress bar starting at row and col.
// progress is between 0 and 1.
void show_progress_bar() {
  int pos = 0;

  mvaddch(pbar.row, pbar.col, '[');

  while ((float) pos++ / pbar.len < pbar.progress) {
    mvaddch(pbar.row, pbar.col+pos, '=');
  }

  while (pos++ < pbar.len) {
    mvaddch(pbar.row, pbar.col+pos, ' ');
  }

  mvaddch(pbar.row, pbar.col+pos-1, ']');

  refresh();
}

void show_song_info() {
    printw_center_x(max_row / 2 - 2, max_col, basename(current_song.name));
    printw_center_x(max_row / 2 + 2, max_col, "%d:%02d | x%.2f",
                    current_song.time / 60, current_song.time % 60, current_song.tempo);
}


int main(int argc, char* argv[])
{
  if (argc > 2) {
    fprintf(stderr, "cscribe <audio_file>\n");
  }

  if (argc == 2) {
    char* audio_name = strdup(argv[1]);
    audio_f = fopen(audio_name, "r");

    current_song.name = audio_name;
    current_song.len = 200; // TODO placeholder
    current_song.tempo = 1.0;

    if (audio_f == NULL) {
      fprintf(stderr, "File %s doesn't exist. Exiting cscribe.\n", audio_name);
      goto cleanup;
    }
  }

  initscr();
  raw();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  getmaxyx(stdscr, max_row, max_col);

  show_main(audio_f);

cleanup:
  endwin();

  return 0;
}
