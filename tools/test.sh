#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
: "${CC:=cc}"
: "${PKG_CONFIG:=pkg-config}"
: "${CFLAGS:=-O2 -std=c99 -Wall -Wextra -Wpedantic}"
test_dir=build/tests
mkdir -p "$test_dir"
lib_flags=$($PKG_CONFIG --cflags --libs raylib glu)

compile_and_run()
{
    name=$1
    shift
    if [ ! -f "tests/$name.c" ]; then
        printf 'Missing ignored local test: tests/%s.c\n' "$name" >&2
        exit 1
    fi
    printf 'Testing %s\n' "$name"
    $CC $CFLAGS -D_POSIX_C_SOURCE=200809L -Iinclude -Isrc "tests/$name.c" \
        build/examples/*.o build/libmanic.a $lib_flags -lm "$@" -o "$test_dir/$name"
    "$test_dir/$name"
}

for name in access_lifetime_test animation_precision_test bezier_bounds_test checkpoint_bound_test \
    config_test frame_boundary_test generation_test integration_test labels_test math_animation_test \
    morph_match_test morph_stress_test morph_winding_test path_timeline_test regression \
    reuse_morph_test reuse_test reused_axes_test roadmap_test timing_compare_test \
    stage1_motion_test stage1_morph_test stage1_arc_stress_test stage2_math_test stage2_property_test stage2_scene_test \
    stage3_replay_test stage3_consistency_test stage3_example_test; do
    compile_and_run "$name"
done
compile_and_run export_recovery_test \
    -Wl,--wrap=link -Wl,--wrap=SetConfigFlags -Wl,--wrap=InitWindow -Wl,--wrap=IsWindowReady \
    -Wl,--wrap=LoadRenderTexture -Wl,--wrap=IsRenderTextureValid -Wl,--wrap=BeginTextureMode \
    -Wl,--wrap=EndTextureMode -Wl,--wrap=scene_render -Wl,--wrap=LoadImageFromTexture \
    -Wl,--wrap=IsImageValid -Wl,--wrap=ImageFlipVertical -Wl,--wrap=ExportImage \
    -Wl,--wrap=UnloadImage -Wl,--wrap=UnloadRenderTexture -Wl,--wrap=CloseWindow

if [ "${MANIC_TEST_GRAPHICS:-0}" = 1 ]; then
    compile_and_run export_real_recovery_test -Wl,--wrap=ExportImage -Wl,--wrap=link
    compile_and_run video_roundtrip_test
else
    printf 'Graphics and video tests skipped; set MANIC_TEST_GRAPHICS=1 with a working display and ffmpeg.\n'
fi
