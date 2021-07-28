# pavolumenotify

`pavolumenotify` is a volume control for [PulseAudio](https://www.freedesktop.org/wiki/Software/PulseAudio/) using [libnotify](https://developer.gnome.org/libnotify/) to show a volume meter notification.

![https://user-images.githubusercontent.com/85039141/127269817-79b4f7f2-c5f8-4631-a771-ffa4c72f79ff.mp4]

## Compile and Install

```bash
make
make install
pavolumenotify
```

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

## Build Dependencies

* libpulse
* libnotify
