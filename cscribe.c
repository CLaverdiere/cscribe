#include <ncurses.h>
#include <string.h>

static int max_col, max_row;
static int quit;
static char* audio_name;
static char* mode_line;
static FILE* audio_f;

void printw_center_x(int, int, char*);
void show_help();
void show_main();
void show_progress_bar(int, float, int, int);

// Prints text centered horizontally.
void printw_center_x(int row, int max_col, char* str) {
  mvprintw(row, (max_col / 2) - (strlen(str) / 2), str);
}

// Prints a help menu with all commands.
void show_help() {
  clear();

  printw_center_x(1, max_col, "cscribe help:\n\n");

  printw("<: Decrease tempo\n");
  printw(">: Increase tempo\n");
  printw("h: Show / exit this help menu\n");
  printw("j: Back 2 seconds\n");
  printw("k: Forward 2 seconds\n");
  printw("o: Open file\n");
  printw("q: Quit cscribe\n");

  refresh();

  getch();

  show_main();
}

// Places a progress bar starting at row and col.
// progress is between 0 and 1.
void show_progress_bar(int bar_len, float progress, int row, int col) {
  int pos = 0;

  mvaddch(row, col, '[');

  while ((float) pos++ / bar_len < progress) {
    mvaddch(row, col+pos, '=');
  }

  while (pos++ < bar_len) {
    mvaddch(row, col+pos, ' ');
  }

  mvaddch(row, col+pos, ']');

  refresh();
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
    mode_line = "Happy transcribing!";
  }

  printw_center_x(1, max_col, welcome_msg);

  if (audio_name != NULL) {
    printw("Transcribing %s\n", audio_name);
  } else {
    printw("Type ");
    addch('o' | A_BOLD);
    printw(" to open an audio file.\n");
  }

  printw("Type ");
  addch('h' | A_BOLD);
  printw(" for the list of all commands.\n");

  show_progress_bar(mid_col, 0.3, mid_row, mid_col / 2);

  mvprintw(max_row - 1, 0, mode_line);
  refresh();

  while (!quit && (ch = getch())) {
    switch(ch) {
      case 'q':
        quit = 1;
        return;
      case 'h':
        show_help();
        break;
    }
  }
}

int main(int argc, char* argv[])
{
  initscr();
  raw();
  noecho();
  keypad(stdscr, TRUE);
  getmaxyx(stdscr, max_row, max_col);

  if (argc > 2) {
    fprintf(stderr, "cscribe <audio_file>");
  }

  if (argc == 2) {
    audio_name = argv[1];
    audio_f = fopen(audio_name, "r");
  }

  show_main(audio_f);

  endwin();
  return 0;
}
