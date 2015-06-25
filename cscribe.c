// cscribe.c - Chris Laverdiere 2015

// TODO find why some songs too loud.
// TODO investigate how to change tempo (ffmpeg / libav)?
// TODO add picture to README.
// TODO configure script / AUR

#define _GNU_SOURCE
#define HZ 44100
#define MILLIS 1000

#include <libgen.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>


#include <ncurses.h>
#include <portaudio.h>
#include <sndfile.h>


// Time delta contants
#define TEMPO_D .05
#define SLEEP_MILLIS_D 100
#define TIME_SKIP_D 2000


struct pa_data {
  SNDFILE *sndfile;
  SF_INFO sf_info;
  int pos;
} a_dat;

struct progress_bar {
  int row, col;
  int len;
  float progress;
} pbar;

struct song {
  int len;
  int time;
  int mark;
  float tempo;
  char* name;
} c_song;

enum {
  PLAYING,
  PAUSED,
} pause_state;


void cleanup();

void* init_audio();
void init_curses();

void toggle_pause();
void seek_mseconds(int);
void set_mark(int);
void set_tempo(float);

void show_help();
void show_greeting();
void* show_main();
void show_modeline();
void show_progress_bar();
void show_song_info();

static int max_col, max_row;
static int quit, in_help, pa_on, curses_on;
static int redraw_flag;
static char* mode_line;
static PaError err;
static PaStream *stream;
static char* welcome_msg = "Welcome to cscribe!\n\n";
static char* audio_states[] = {"Playing", "Paused"};

static int pa_callback(const void *input_buf,
                       void *output_buf,
                       unsigned long frame_cnt,
                       const PaStreamCallbackTimeInfo* time_info,
                       PaStreamCallbackFlags flags,
                       void *data) {

  struct pa_data* cb_data = data;
  int* out = (int *) output_buf;
  int* cursor = out;
  int size = frame_cnt;
  int read;
  int frames_left;

  while (size > 0) {
    sf_seek(cb_data->sndfile, cb_data->pos, SEEK_SET);
    frames_left = cb_data->sf_info.frames - cb_data->pos;

    if (size > frames_left) {
      read = frames_left;
      cb_data->pos = 0;
    } else {
      read = size;
      cb_data->pos += read;
    }

    sf_readf_int(cb_data->sndfile, cursor, read);
    cursor += read;
    size -= read;
  }

  return paContinue;
}

void pa_error(PaError err) {
  if (err != paNoError) {
    fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
    exit(1);
  }
}

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

void toggle_pause() {
  if (Pa_IsStreamStopped(stream)) {
    pause_state = PLAYING;
    Pa_StartStream(stream);
  } else {
    pause_state = PAUSED;
    Pa_StopStream(stream);
  }

  show_song_info();
}

// TODO use a callback?
void seek_mseconds(int n) {
  int clamped_seconds = MIN(MAX(n, 0), c_song.len);

  c_song.time = clamped_seconds;

  if (pause_state == PLAYING) {
    Pa_StopStream(stream);
  }

  a_dat.pos = (clamped_seconds / MILLIS) * a_dat.sf_info.samplerate;
  sf_seek(a_dat.sndfile, a_dat.pos, SEEK_SET);

  if (pause_state == PLAYING) {
    Pa_StartStream(stream);
  }

  show_progress_bar();
  show_song_info();
}

void set_mark(int n) {
  c_song.mark = n;
  show_song_info();
  show_progress_bar();
}

void set_tempo(float f) {
  c_song.tempo = MAX(0, f);
  a_dat.sf_info.samplerate *= c_song.tempo;
  show_song_info();
}

// Prints a help menu with all commands.
void show_help() {
  clear();
  in_help = 1;

  printw_center_x(1, max_col, "cscribe help:\n\n");

  printw("': Jump to mark\n");
  printw("<: Decrease tempo\n");
  printw(">: Increase tempo\n");
  printw("h: Show / exit this help menu\n");
  printw("j: Back 2 seconds\n");
  printw("k: Forward 2 seconds\n");
  printw("m: Create mark\n");
  printw("o: Open file\n");
  printw("p: Pause\n");
  printw("q: Quit cscribe\n");

  refresh();

  getch();

  in_help = 0;
  show_main();
}

void show_greeting() {
  printw_center_x(1, max_col, welcome_msg);

  if (c_song.name == NULL) {
    printw("Type ");
    addch('o' | A_BOLD);
    printw(" to open an audio file.\n");
  } else {
    show_song_info();
  }
}

void show_modeline() {
  mvprintw(max_row - 1, 0, mode_line);
}

// Displays the main screen.
void* show_main(void* args) {
  int ch;

  if (!curses_on) {
    init_curses();
    usleep(100 * MILLIS);
  }

  // TODO We should use a separate window for the modeline.
  if (mode_line == NULL) {
    mode_line = "Type h for the list of all commands.";
  }

  clear();
  show_greeting();
  show_modeline();
  show_progress_bar();

  while (!quit && (ch = getch())) {
    if (redraw_flag) {
      redraw_flag = 0;
      show_main(NULL);
    }

    switch(ch) {
      case '\'':
        seek_mseconds(c_song.mark);
        break;
      case '<':
        set_tempo(c_song.tempo - TEMPO_D);
        break;
      case '>':
        set_tempo(c_song.tempo + TEMPO_D);
        break;
      case 'h':
        show_help();
        break;
      case 'j':
        seek_mseconds(c_song.time - TIME_SKIP_D);
        break;
      case 'k':
        seek_mseconds(c_song.time + TIME_SKIP_D);
        break;
      case 'm':
        set_mark(c_song.time);
        break;
      case 'p':
        toggle_pause();
        break;
      case 'q':
        quit = 1;
        return NULL;
    }
  }

  return NULL;
}

// Places a progress bar starting at row and col.
// progress is between 0 and 1.
void show_progress_bar() {
  int pos = 0;
  int mark_pos = (float) c_song.mark / c_song.len * pbar.len;

  pbar.col = max_col / 4;
  pbar.row = max_row / 2;
  pbar.len = max_col / 2;
  pbar.progress = (float) c_song.time / c_song.len;

  mvaddch(pbar.row, pbar.col, '[');

  while ((float) pos++ / pbar.len < pbar.progress) {
    mvaddch(pbar.row, pbar.col+pos, '=');
  }

  while (pos++ < pbar.len) {
    mvaddch(pbar.row, pbar.col+pos, ' ');
  }

  if (c_song.mark != 0) {
    mvaddch(pbar.row, pbar.col + mark_pos + 1, '*');
  }

  mvaddch(pbar.row, pbar.col+pos-1, ']');

  refresh();
}

void show_song_info() {
  int curr_seconds = c_song.time / MILLIS;
  int mark_seconds = c_song.mark / MILLIS;
  int total_seconds = c_song.len / MILLIS;

  char* state_str = audio_states[pause_state];

  printw_center_x(max_row / 2 - 2, max_col, "%s - %-7s",
                  basename(c_song.name), state_str);

  printw_center_x(max_row / 2 + 2, max_col, "%d:%02d / %d:%02d | x%.2f",
                  curr_seconds / 60, curr_seconds % 60,
                  total_seconds / 60, total_seconds % 60,
                  c_song.tempo);

  if (c_song.mark != 0) {
    printw_center_x(max_row / 2 + 6, max_col, "(*) mark set at %d:%02d",
                    mark_seconds / 60, mark_seconds % 60);
  }
}

void* init_audio(void* args) {
  PaStreamParameters out_params;

  a_dat.pos = 0;
  a_dat.sf_info.format = 0;
  a_dat.sndfile = sf_open(c_song.name, SFM_READ, &a_dat.sf_info);

  if (!a_dat.sndfile) {
    fprintf(stderr, "Couldn't open file %s\n", c_song.name);
    return NULL;
  }

  c_song.len = (MILLIS * (float) a_dat.sf_info.frames)
    / a_dat.sf_info.samplerate;

  Pa_Initialize();
  pa_on = 1;

  out_params.device = Pa_GetDefaultOutputDevice();
  out_params.channelCount = a_dat.sf_info.channels;
  out_params.suggestedLatency = 0.2;
  out_params.sampleFormat = paInt32;
  out_params.hostApiSpecificStreamInfo = 0;

  Pa_OpenStream(&stream, 0, &out_params, a_dat.sf_info.samplerate,
                      paFramesPerBufferUnspecified, paNoFlag, pa_callback,
                      &a_dat);

  Pa_StartStream(stream);

  while (!quit) {
    if (Pa_IsStreamActive(stream)) {
      Pa_Sleep(SLEEP_MILLIS_D);
      c_song.time += SLEEP_MILLIS_D;

      if (!in_help) {
        show_progress_bar();
        show_song_info();
      }
    }
  }

  Pa_StopStream(stream);
  quit = 1;

  return a_dat.sndfile;
}

void init_curses() {
  initscr();
  cbreak();
  noecho();
  curs_set(0);
  keypad(stdscr, TRUE);
  getmaxyx(stdscr, max_row, max_col);

  curses_on = 1;
}

void cleanup() {
  if (pa_on) Pa_Terminate();
  if (curses_on) endwin();
}

int main(int argc, char* argv[])
{
  pthread_t curses_thread, audio_thread;
  void* audio_ret;

  atexit(cleanup);

  if (argc == 2) {
    char* audio_name = strdup(argv[1]);

    c_song.name = audio_name;
    c_song.tempo = 1.0;
  } else {
    fprintf(stderr, "cscribe <audio_file>\n");
    exit(1);
  }

  pthread_create(&audio_thread, NULL, init_audio, NULL);
  pthread_create(&curses_thread, NULL, show_main, NULL);

  pthread_join(audio_thread, &audio_ret);

  if (!audio_ret) {
    exit(1);
  }

  pthread_join(curses_thread, NULL);

  return 0;
}
