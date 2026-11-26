#ifndef MANIC_INTERNAL_H
#define MANIC_INTERNAL_H
#include "manic.h"
#include <raylib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static inline int scene_slot(const Scene *scene, int handle)
{
    unsigned int slot;
    unsigned int generation;

    if(handle < 0) {return(-1);}
    slot = (unsigned int)handle & MANIC_SLOT_MASK;
    generation = (unsigned int)handle >> MANIC_SLOT_BITS;
    if(slot >= (unsigned int)scene->mobject_count || scene->mobjects[slot].generation != generation ||
        scene->mobjects[slot].reusable) {return(-1);}
    return((int)slot);
}

struct MorphCache {
    const VectorPath *source;
    const VectorPath *target;
    VectorPath left;
    VectorPath right;
    struct MorphCache *next;
};

typedef struct {
    Bezier curve;
    double lengths[129];
} ArcCache;

struct AnimationCache {
    ArcCache *arc_lengths;
    MorphCache *morphs;
};

typedef struct {
    ManicVec2 *points;
    size_t count;
    int closed;
} TessContour;

typedef struct {
    ManicVec2 *vertices;
    size_t count;
    size_t capacity;
} TessMesh;

int vector_valid(ManicVec2);
ManicVec2 vector_lerp(ManicVec2, ManicVec2, double);
int bezier_arc_table(const Bezier *, ArcCache *);
double bezier_arc_lookup(const ArcCache *, double);
MorphCache *morph_cache_create(const VectorPath *, const VectorPath *);
int path_interpolate_cached(const MorphCache *, float, VectorPath *);
void animation_interpolate(const Animation *, const Mobject *, Mobject *, const Scene *, const AnimationCache *, float);
void timeline_invalidate_checkpoints(Timeline *);
Vector2 raylib_vector(ManicVec2);
Color raylib_color(ManicColor);
ManicVec2 camera_to_screen(const ManicCamera *, ManicVec2, int, int);
ManicVec2 camera_world_point(const Mobject *, int, ManicVec2);
int tessellate_contours(const TessContour *, size_t, PathFillRule, TessMesh *);
int camera_render_path(const ManicCamera *, const Mobject *, int, const Mobject *, int, int, float);
int scene_render(const Scene *, int, int, float);
RenderConfig render_config_default(void);
int render_config_parse(RenderConfig *, int, char **);
int scene_preview(Scene *, const RenderConfig *);
int scene_frame_intervals(const Scene *, int, unsigned long long *);
int scene_export_frames(Scene *, const RenderConfig *);
int video_output_available(const char *);
int encode_video(const RenderConfig *);
int first_scene_build(Scene *);
int benchmark_scene_build(Scene *);
int paths_scene_build(Scene *);
int animations_scene_build(Scene *);
int math_scene_build(Scene *);

#endif
