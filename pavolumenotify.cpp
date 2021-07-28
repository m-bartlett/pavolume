#include <getopt.h>
#include <libnotify/notify.h>
#include <locale.h>
#include <math.h>
#include <memory.h>
#include <pulse/pulseaudio.h>
#include <stdbool.h>
#include <stddef.h> // wchar_t
#include <stdio.h>
#include <string.h>
#include <uchar.h>
#include <unistd.h> // usleep
#include <wchar.h>
#include <stdint.h>

#define PROGRESS_SYMBOLS_LITERAL " ▎▍▌▋▊▉█"
// #define PROGRESS_SYMBOLS_LITERAL "⣉⣏⣿"

#define ENCLOSING_BAR_SYMBOLS_LITERAL "││"
// #define ENCLOSING_BAR_SYMBOLS_LITERAL "┥┝"

#define VOLUME_MUTE_SYMBOL_LITERAL "婢"
#define VOLUME_QUIET_SYMBOL_LITERAL "奄<span font_size=\"3500\"> </span>"
#define VOLUME_NORMAL_SYMBOL_LITERAL "奔"
#define VOLUME_LOUD_SYMBOL_LITERAL "墳"

#define BAR_WIDTH 10

#define NOTIFICATION_TIMEOUT_MS 750
#define NOTIFICATION_CATEGORY "volume"
#define NOTIFICATION_HINT_STRING "synchronous"


// Support strings representing up to 150%
#define BAR_WIDTH_1_5 (BAR_WIDTH*1.5)
#define arrsize(x)  (sizeof(x) / sizeof((x)[0]))

#ifdef ENCLOSING_BAR_SYMBOLS_LITERAL
  constexpr uint8_t  BAR_STRING_WIDTH       = BAR_WIDTH_1_5 + 3;
#else
  constexpr uint8_t  BAR_STRING_WIDTH       = BAR_WIDTH_1_5 + 1;
#endif

constexpr char32_t PROGRESS_SYMBOLS[]     = U"" PROGRESS_SYMBOLS_LITERAL;
constexpr uint8_t  PROGRESS_SYMBOL_SIZE   = arrsize(PROGRESS_SYMBOLS)-1;
constexpr uint8_t  PROGRESS_SYMBOL_SIZE_1 = PROGRESS_SYMBOL_SIZE-1;
constexpr char32_t FULL_PROGRESS_SYMBOL   = PROGRESS_SYMBOLS[PROGRESS_SYMBOL_SIZE-1];
constexpr char32_t EMPTY_PROGRESS_SYMBOL  = PROGRESS_SYMBOLS[0];
constexpr uint32_t TOTAL_PROGRESS_STEPS   = BAR_WIDTH * PROGRESS_SYMBOL_SIZE_1;

size_t to_utf8(char32_t codepoint, char *buf) {
    mbstate_t state{};
    return c32rtomb(buf, codepoint, &state);
}

void percentage2bar(uint8_t percentage, char bar_string_utf8[]) {
  uint32_t  _percentage = (percentage * TOTAL_PROGRESS_STEPS) / 100;
  uint8_t   remainder    = _percentage % PROGRESS_SYMBOL_SIZE_1;
  uint8_t   full_width   = _percentage / PROGRESS_SYMBOL_SIZE_1;
  char32_t  bar_string[BAR_STRING_WIDTH]={0};
  uint8_t   i = 0;
  char32_t *p = bar_string;
  char     *u = bar_string_utf8;
  char      utf8[4];

  // Assemble progress symbols into proportional block UTF-32 string
  #ifdef ENCLOSING_BAR_SYMBOLS_LITERAL
    *p++ = (U"" ENCLOSING_BAR_SYMBOLS_LITERAL)[0];
  #endif

  for (; i<full_width; i++,  p++) {
    *p   = FULL_PROGRESS_SYMBOL;
  }
  if (remainder) {
    *p++ = PROGRESS_SYMBOLS[remainder];
    i++;
  }
  for (; i<BAR_WIDTH; i++,p++) {
    *p   = EMPTY_PROGRESS_SYMBOL;
  }

  #ifdef ENCLOSING_BAR_SYMBOLS_LITERAL
    *p++ = (U"" ENCLOSING_BAR_SYMBOLS_LITERAL)[1];
  #endif

  *p   = U'\0';

  // Convert UTF-32 string to UTF-8 and write to argument buffer
  for (p = bar_string; *p != U'\0'; p++) {
    size_t z = to_utf8(*p, utf8);
    memcpy(u, utf8, z);
    u += z;
  }
  *u = U'\0';
}

void send_notification(char *summary, char *body) {
  notify_init(summary);
  NotifyNotification *notification = notify_notification_new(summary, body, NULL);
  notify_notification_set_category(notification, NOTIFICATION_CATEGORY);
  notify_notification_set_hint(
    notification,
    NOTIFICATION_HINT_STRING,
    g_variant_new_string(NOTIFICATION_CATEGORY)
  );
  notify_notification_set_timeout(notification, NOTIFICATION_TIMEOUT_MS);
  notify_notification_show(notification, NULL);
  g_object_unref(notification);
  notify_uninit();
}

#define FORMAT "%s"

static pa_mainloop *mainloop = NULL;
static pa_mainloop_api *mainloop_api = NULL;
static pa_context *context = NULL;
int retval = EXIT_SUCCESS;

typedef struct Command {
  char *format;
  bool is_delta_volume;
  bool is_mute_off;
  bool is_mute_on;
  bool is_mute_toggle;
  bool is_snoop;
  int volume;
} Command;


static void wait_loop(pa_operation *op) {
  while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
    if (pa_mainloop_iterate(mainloop, 1, &retval) < 0) break;
  pa_operation_unref(op);
}

static int constrain_volume(int volume) { if (volume < 0) return 0; return volume; }
static int normalize(pa_volume_t volume) { return (int) round(volume * 100.0 / PA_VOLUME_NORM); }
static pa_volume_t denormalize(int volume) { return (pa_volume_t) round(volume * PA_VOLUME_NORM / 100); }

static void
set_volume( pa_context *c,
            const pa_sink_info *i,
            __attribute__((unused)) int eol,
            void *userdata
          ) {
  if (i == NULL) return;

  Command *command = (Command *) userdata;
  if (command->is_mute_on)  pa_context_set_sink_mute_by_index(c, i->index, 1, NULL, NULL);
  if (command->is_mute_off) pa_context_set_sink_mute_by_index(c, i->index, 0, NULL, NULL);
  if (command->is_mute_toggle) pa_context_set_sink_mute_by_index(c, i->index, !i->mute, NULL, NULL);
  if (command->volume == -1 && !command->is_delta_volume) return;

  // Turn muting off on any volume change, unless muting was specifically turned on or toggled.
  // if (!command->is_mute_on && !command->is_mute_toggle)
    // pa_context_set_sink_mute_by_index(c, i->index, 0, NULL, NULL);

  pa_cvolume *cvolume = (pa_cvolume*)&i->volume;
  int new_volume = (
    command->is_delta_volume ?
    normalize(pa_cvolume_avg(cvolume)) + command->volume :
    command->volume
  );

  pa_cvolume *new_cvolume = (
    pa_cvolume_set(
      cvolume,
      i->volume.channels,
      denormalize(constrain_volume(new_volume))
    )
  );

  pa_context_set_sink_volume_by_index(c, i->index, new_cvolume, NULL, NULL);
}

static void
get_server_info( __attribute__((unused)) pa_context *c,
                 const pa_server_info *i,
                 void *userdata
               ) {
  if (i == NULL) return;
  strncpy((char*)userdata, i->default_sink_name, 255);
}

static void
print_volume( __attribute__((unused)) pa_context *c,
              const pa_sink_info *i,
              __attribute__((unused)) int eol,
               void *userdata
            ) {
  if (i == NULL) return;

  Command *command = (Command *) userdata;
  char output[4] = "---";
  auto volume = normalize(pa_cvolume_avg(&(i->volume)));
  if (!i->mute) snprintf(output, 4, "%d", volume);
  printf(command->format, output);
  printf("\n");
  fflush(stdout);

  char bar_string[BAR_STRING_WIDTH*4];
  percentage2bar(volume, bar_string);

  if (i->mute)
    send_notification(bar_string, (char*) u8"" VOLUME_MUTE_SYMBOL_LITERAL);
  else {
    uint8_t loudness = volume/33;
    switch(loudness) {
      case 0: send_notification(bar_string, (char*) u8"" VOLUME_QUIET_SYMBOL_LITERAL); break;
      case 1: send_notification(bar_string, (char*) u8"" VOLUME_NORMAL_SYMBOL_LITERAL); break;
      case 2:
      default: send_notification(bar_string, (char*) u8"" VOLUME_LOUD_SYMBOL_LITERAL); break;
    }
  }
}

static int
init_context(pa_context *c, int retval) {
  pa_context_connect(c, NULL, PA_CONTEXT_NOFLAGS, NULL);
  pa_context_state_t state;
  while (state = pa_context_get_state(c), true) {
    if (state == PA_CONTEXT_READY)  return 0;
    if (state == PA_CONTEXT_FAILED) return 1;
    pa_mainloop_iterate(mainloop, 1, &retval);
  }
  return 0;
}


static int
quit(int new_retval) {
  // Only set `retval` if it hasn't been changed elsewhere (such as by PulseAudio in `pa_mainloop_iterate()`).
  if (retval == EXIT_SUCCESS) retval = new_retval;
  if (context) pa_context_unref(context);
  if (mainloop_api) mainloop_api->quit(mainloop_api, retval);
  if (mainloop) {
    pa_signal_done();
    pa_mainloop_free(mainloop);
  }
  return retval;
}


static int usage() {
  fprintf(stderr, "pavolumenotify [-h] [-m [on|off|toggle] [-v [+|-]number]\n");
  return quit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {

  // setlocale(LC_CTYPE, "");
  // setlocale(LC_ALL, "en_US.UTF-8");
  setlocale(LC_ALL, "en_US.utf8");

  Command command = {
    .format = (char*)FORMAT,
    .is_delta_volume = false,
    .is_mute_off = false,
    .is_mute_on = false,
    .is_mute_toggle = false,
    .is_snoop = false,
    .volume = -1,
  };

  static const struct option long_options[] = {
    {"help",   0, NULL, 'h'},
    {"volume", 1, NULL, 'v'},
    {"mute",   1, NULL, 'm'},
    {NULL,     0, NULL, 0}
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "-hm:v:", long_options, NULL) ) != -1) {
    switch (opt) {
      case 'h': // help
        return usage();
      case 'm': // mute
        command.is_mute_off = strcmp("off", optarg) == 0;
        command.is_mute_on = strcmp("on", optarg) == 0;
        command.is_mute_toggle = strcmp("toggle", optarg) == 0;
        if (!(command.is_mute_off || command.is_mute_on || command.is_mute_toggle)) {
          return usage();
        }
        break;
      case 'v': //volume
        command.volume = (int) strtol(optarg, NULL, 10);
        if (command.volume == 0 && '0' != optarg[0]) {
          // If `strtol` converted the `optarg` to 0, but the argument didn't begin with a '0'
          // then it must not have been numeric.
          return usage();
        }
        if (optarg[0] == '-' || optarg[0] == '+') command.is_delta_volume = true;
        break;
      default:
        return usage();
    }
  }

  mainloop = pa_mainloop_new();
  if (!mainloop) {
    fprintf(stderr, "Could not create PulseAudio main loop\n");
    return quit(EXIT_FAILURE);
  }

  mainloop_api = pa_mainloop_get_api(mainloop);
  if (pa_signal_init(mainloop_api) != 0) {
    fprintf(stderr, "Could not initialize PulseAudio UNIX signal subsystem\n");
    return quit(EXIT_FAILURE);
  }

  context = pa_context_new(mainloop_api, argv[0]);
  if (!context || init_context(context, retval) != 0) {
    fprintf(stderr, "Could not initialize PulseAudio context\n");
    return quit(EXIT_FAILURE);
  }

  char *default_sink_name[256];
  wait_loop(pa_context_get_server_info(context, get_server_info, &default_sink_name));
  wait_loop(pa_context_get_sink_info_by_name(context, (char *) default_sink_name, set_volume, &command));
  wait_loop(pa_context_get_sink_info_by_name(context, (char *) default_sink_name, print_volume, &command));

  return quit(retval);
}
