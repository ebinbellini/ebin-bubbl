# ebin-bubbl

This is a system info popup program. It can show volume %, brightness %,
mic on/off, sound on/off, battery saving mode on/off. It can easily be
extended to show other things.

## Building

Dependencies:
- Xlib
- Xft
- X shape extension
- Imlib2
- freetype2

        yay -S libx11 libxft libxext imlib2 freetype2


Build with

        make

and install with

        make install


## Running

Show 50% volume:

        ebin-bubbl --volume 50

Show 100% brightness:

        ebin-bubbl --brightness 50

Show mic muted:

        ebin-bubbl --mic-mute 1

Show sound on:

        ebin-bubbl --sound-mute 0

Show power saving mode on

        ebin-bubbl --power-mode 0

