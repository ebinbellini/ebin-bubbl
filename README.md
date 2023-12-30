# ebin-bubbl

This is a system info popup program. It can show volume %, brightness %,
mic on/off, sound on/off, battery saving mode on/off. It can easily be
extended to show other things. Below is a demonstration of how it looks.

![example of ebin-bubbl being used](./example.png)

## Building

Dependencies:
- Xlib
- Xft
- X shape extension
- Imlib2
- freetype2

Install dependecies with (arch based):

    pacman -S libx11 libxft libxext imlib2 freetype2


Build with

    make

and install with

    make install


## Running

Show 50% volume:

    ebin-bubbl --volume 50

Show 100% brightness:

    ebin-bubbl --brightness 100

Show mic muted:

    ebin-bubbl --mic 1

Show sound on:

    ebin-bubbl --sound 0

Show power saving mode on

    ebin-bubbl --power-mode 0

## Configuring

You can configure the color scheme by editing the COLOR section of the config.h
file. You can also add new icons in the ICONS section.

You *must* change the path to the icon directory by editing the config.h file.
I might try to fix this in the future by embedding the icons in the executable
somehow.
