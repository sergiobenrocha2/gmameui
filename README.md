GMAMEUI
=======

*It's MAME! On Linux!*

GMAMEUI is a front-end program that helps you run MAME on non-Windows platforms, allowing you to run your arcade games quickly and easily.

GMAMEUI is a fork of the defunct [GXMame]("http://gxmame.sourceforge.net/") project.

It contains a number of enhancements over GXMame:

* Support for SDLMame
* Support for more recent versions of MAME (currently supporting up to v0.123)
* Support for the recent features introduced to MAME (the last version supported by GXMame was 0.95)
* Migration to Glade for UI, allowing easier maintenance
* A substantial number of bug fixes and UI improvements over GXMame

Installing
==========

To install from source, you will need the following packages:

autotools-dev, zlib1g-dev, libexpat1-dev, libgtk2.0-dev, libglade2-dev, intltool, libarchive-dev, libvte-dev, libzip-dev, libgtkimageview-dev, gnome-doc-utils

Then:
```sh
git clone https://github.com/sergiobenrocha2/gmameui.git gmameui
cd gmameui/
sh autogen.sh
./configure
make
make install  # as root
```
