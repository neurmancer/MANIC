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

typedef enum {
    UPDATER_POSITION = SCENE_BIND_POSITION,
    UPDATER_ROTATION = SCENE_BIND_ROTATION,
    UPDATER_OPACITY = SCENE_BIND_OPACITY
} SceneUpdaterType;

struct SceneUpdater {
    int node;
    int tracker;
    SceneUpdaterType type;
    ScenePositionUpdater position;
    SceneScalarUpdater scalar;
    const void *context;
    void *owned_context;
    double line_dx;
    double line_dy;
    double line_scale;
};

struct MorphCache {
    const VectorPath *source;
    const VectorPath *target;
    VectorPath left;
    VectorPath right;
    int expanded;
    struct MorphCache *next;
};

typedef struct {
    Bezier curve;
    double lengths[129];
} ArcCache;

typedef struct PathArcCache PathArcCache;

struct AnimationCache {
    ArcCache *arc_lengths;
    PathArcCache *path_arc;
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
PathArcCache *path_arc_create(const VectorPath *, double);
void path_arc_destroy(PathArcCache *);
int path_arc_point(const PathArcCache *, double, ManicVec2 *);
MorphCache *morph_cache_create(const VectorPath *, const VectorPath *);
int path_interpolate_cached(const MorphCache *, float, VectorPath *);
void animation_interpolate(const Animation *, const Mobject *, Mobject *, const Scene *, const AnimationCache *, double);
double animation_duration(const Animation *);
double timeline_action_duration(const SceneAction *);
void timeline_invalidate_checkpoints(Timeline *);
typedef enum {
    EDIT_PARENT,
    EDIT_LAYER,
    EDIT_TRACKER,
    EDIT_BINDING
} TimelineEditType;

int timeline_record_edit(Timeline *, TimelineEditType, int, int, double);
int timeline_reserve_edits(Timeline *, int);
void timeline_retract_edit(Timeline *);
int timeline_binding_has_edits(const Timeline *, int, SceneUpdaterType);
int scene_parent_valid(const Scene *, int, int);
int scene_apply_updaters(Scene *);
int scene_updater_conflicts(SceneUpdaterType, AnimationType);
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
int plane_scene_build(Scene *);
int tracker_scene_build(Scene *);

#endif
