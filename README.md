# spotifywm

Makes spotify more friendly to window managers by settings a class name before opening the window.
This allows WMs like i3 to correctly discover the window and fit it into a prepared layout.

Inspired by [steamwm](https://github.com/dscharrer/steamwm).

# Installation

```
$ make
```

# Usage

```
LD_PRELOAD=/path/to/spotifywm.so /path/to/spotify/binary
```

Under Arch Linux, do not run the wrapper script in /usr/bin, it will override `LD_PRELOAD`.
Use `/usr/share/spotify/spotify` instead.
