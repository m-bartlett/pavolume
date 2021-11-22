---

# 🚨 Archived 🚨 

This project has been deprecated in favor of [pavol-dunst](https://github.com/m-bartlett/pavol-dunst) which uses native features in `dunst` to show better graphical progress bars instead of tacky unicode progress bars. Since I targetted this project at using dunst anyway, this spiritual successor will benefit from dunst-native features.

---

# pavolumenotify

`pavolumenotify` is a volume control utility for [PulseAudio](https://www.freedesktop.org/wiki/Software/PulseAudio/) using [libnotify](https://developer.gnome.org/libnotify/) to show a volume meter notification.

## Video Demo
https://user-images.githubusercontent.com/85039141/127269907-95deea8e-6af2-4f13-aaef-60d7917bf676.mp4

## Configuration
The preprocessor definitions at the top of the .cpp file can be modified to the user's liking and then recompiled
```C
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

// I use this for custom dunst styling
#define NOTIFICATION_CATEGORY "volume"

// Necessary to re-use the same libnotify notification
#define NOTIFICATION_HINT_STRING "synchronous"
```

Here is my `dunstrc` specification for the `volume` notification category as shown in the demo video:
```ini
[volume]
    category = volume
    format = <span font="VictorMono Nerd Font 20">%b </span><span rise="5000" font="DejaVu Sans Mono 11">%s</span><span font_size="8000"> </span>
```

## Compile and Install

```bash
make
make install
pavolumenotify
```

## Build Dependencies

* libpulse
* libnotify

## Options


Flag|Long flag|Description
---|---|---
-h|--help|Print help text
-m|--mute|`on`\|`off`\|`toggle` muting options
-v|--volume|[`-`\|`+`]`number` 0 and 100 inclusive. Optionally prefixed with `-` or `+` to denote a delta.

### Examples

```bash
$ pavolumenotify --help
pavolumenotify [-h] [-m [on|off|toggle] [-v [+|-]number]

$ pavolumenotify -v 50
50
```
