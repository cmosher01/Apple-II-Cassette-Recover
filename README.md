# Apple-II-Cassette-Recover

Recover data from Apple ][ cassette image WAVE files.

Copyright © 2019, Christopher Alan Mosher, Shelton, CT, USA. <cmosher01@gmail.com>

This software is released under the GPLv3 licence.

### Build

Supports Linux, Mac, and Windows.
Requires [SDL2](https://www.libsdl.org/download-2.0.php) development library for your platform.

```shell
$ qmake
$ make
```

### Run

```shell
$ a2cassre input.wav output.wav
```

### Description

Apple-II-Cassette-Recover attempts to recover all the data from a WAVE format image
of a cassette tape in Apple ][ SAVE format.

`{ HEADER SYNC DATA } ...`

It does not try to preserve non-data areas of the input wave file, but just extracts
them. It creates a new clean WAVE file, with a completely rebuilt tape signal.

See the [wiki](https://github.com/cmosher01/Apple-II-Cassette-Recover/wiki) for examples.
