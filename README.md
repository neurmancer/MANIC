# MANIC

> Yeah basically a MANIM clone I'm not that creative to come up with this idea myself so I shamelessly stole the idea :/


## Table of Contents

- [YOU ARE HERE](#table-of-contents)
- [What is Where](#file-tree)
- [Dependencies](#dependencies)
- [Compile?](#compiling-and-shit)
- [Use the thing](#using-the-thing)
- [MIT](#license)

## File Tree

```text
MANIC/
├── Makefile
├── include/manic.h         public C API
├── src/                    separately compiled library and CLI sources
├── examples/               example scene builders
├── tests/                  local tests (ignored)
├── build/                  generated executable, objects, and static library
└── LICENSE
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


## Using the Thing

`make` gives you the CLI in `build/manic` and a library in `build/libmanic.a`.
Want to make your own scenes? Include `manic.h` and link the library like this:

```sh
cc -std=c99 -Iinclude your_scene.c build/libmanic.a \
  $(pkg-config --libs raylib glu) -lm -o your_scene
```

Start scenes and paths with `{0}`, then clean up your mess with `scene_destroy`
and `path_destroy`. `scene_add` copies paths for you. The structs are public for
now, so don't count on the ABI staying the same forever.

Morphing figures out which paths go together once per animation, and following
a curve caches a 128-interval approximation of its length. Seeking saves scene
snapshots at action boundaries it has already visited, then replays from the
closest one. More actions and objects mean more snapshot memory. Adding,
removing, or collecting objects clears those snapshots. Call
`scene_update(scene, 0)` before seeking to initialize the timeline. Directly
editing animated fields afterward won't update the snapshots, so use scene and
timeline operations for those changes.

Graphs sample more points where they need them. `scene_function_graph_adaptive`
lets you pick the maximum subdivision depth (0–20) and an error tolerance
relative to the vertical axis range. It returns `-1` if it hits the four million
subdivision limit. No amount of sampling can catch every weird function, though.
If you know where yours breaks, give those sorted, interior x coordinates to
`scene_function_graph_with_breaks` and it won't draw across them. More initial
samples, depth, or a smaller tolerance can help with finer details.

## LICENSE

> IT'S MIT guys go feral with it [LICENSE](LICENSE)
