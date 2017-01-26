# aminogfx-gl

AminoGfx implementation for OpenGL 2 / OpenGL ES 2. Node.js based animation framework supporting images, texts, primitives, 3D transformations and realtime animations. Basic video support on Raspberry Pi (beta feature).

## Platforms

* macOS
* Raspberry Pi

## Requirements

In order to build the native components a couple of libraries and tools are needed.

* Node.js 4.x or 7.x
 * There is a bug in Node.js v6.9.1 (see https://github.com/nodejs/node/issues/9288).
* Freetype 2.7.
* libpng.
* libjpeg.

### macOS

* GLFW 3.2.
* FFMPEG

MacPorts setup:

```
sudo port install glfw freetype ffmpeg
```

Homebrew setup:

```
brew install pkg-config
brew tap homebrew/versions
brew install glfw3
brew install freetype
```

### Raspberry Pi

* libjpeg-dev
* libav
* Raspbian (other Linux variants should work too)

Setup:

```
sudo apt-get install libjpeg-dev libavformat-dev
```

## Installation

```
npm install
```

## Build

During development you'll want to rebuild the source constantly:

```
npm install --build-from-source
```

Or use:

```
./rebuild.sh
```

## Demo

```
node demos/circle.js
```

Example of all supported features are in the demos subfolder.