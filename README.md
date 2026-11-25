# MANIC

> Yeah basically a MANIM clone I'm not that creative to come up with this idea myself so I shamelessly stole the idea :/


## Table of Contents

- [YOU ARE HERE](#table-of-contents)
- [What is Where](#file-tree)
- [Dependencies](#dependencies)
- [Compile?](#compiling-and-shit)
- [MIT](#license)

## File Tree

```text
MANIC/
├── Makefile
├── LICENSE
├── README.md
├── examples/
│   └── first_scene.c
├── src/
│   ├── main.c
│   ├── animation/
│   │   └── animation.c
│   ├── camera/
│   │   └── camera.c
│   ├── core/
│   │   └── types.c
│   ├── mobject/
│   │   └── geometry.c
│   └── scene/
│       └── scene.c
├── build/
├── manim/                  (ignored (yeah I AM COPYING SHIT SO WHAT?))
├── tests/
└── NOTES.md                
```


## Dependencies

- A C99 compiler, `make`, and `pkg-config` (or a compatible `pkgconf` command).
- raylib and its development files, discoverable as `raylib` by `pkg-config`.
- OpenGL and GLU development files, with GLU discoverable as `glu` by `pkg-config`.
- The C math library, linked with `-lm`.

Preview and PNG export need a working graphics context. Video export additionally
requires `ffmpeg` with the `libx264` encoder; PNG export does not require FFmpeg.

## Compiling and Shit

```sh
make all #that's it lmfao
```


## LICENSE

> IT'S MIT guys go feral with it [LICENSE](LICENSE)
