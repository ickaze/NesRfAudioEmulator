# Third-party notices

## FCEUmm (libretro port)

Source: https://github.com/libretro/libretro-fceumm
Pinned commit: 7a542dab1e87679921962a9f056186eca425c0c2
Local source: third_party/fceumm (including its libretro-common snapshot).

FCEUmm derives from FCE Ultra and is distributed under GPL-2.0-or-later.
The full license is in LICENSE.txt and third_party/fceumm/Copying.
Original authors, copyright and licensing notices remain in the source files;
see also third_party/fceumm/Authors. libretro-common and other incorporated
files retain their respective notices. No upstream source files were modified.
The CMake and Visual Studio build descriptions alongside the snapshot are
local additions. HD pack and NTSC filter support are not compiled in this build.

The application frontend and RF integration in this distribution are provided
under GPL-2.0-or-later. This ZIP includes the corresponding source and build
files for the statically linked application. ROM images and BIOS are not included.
