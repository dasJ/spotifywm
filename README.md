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
LD_PRELOAD=/usr/lib/libcurl.so.3:/path/to/spotifywm.so /path/to/spotify/binary
```
Note that libcurl needs to be preloaded for Arch Linux.
I don't know about other distributions.

Under Arch Linux, do not run the wrapper script in /usr/bin, it will override `LD_PRELOAD`.
Use `/usr/share/spotify/spotify` instead.
