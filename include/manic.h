#ifndef MANIC_H
#define MANIC_H
#include <stddef.h>
#include <limits.h>

#define MANIC_SLOT_BITS 20
#define MANIC_SLOT_MASK ((1u << MANIC_SLOT_BITS) - 1u)
#define MANIC_MAX_GENERATION ((unsigned int)INT_MAX >> MANIC_SLOT_BITS)

typedef struct {
    float x;
    float y;
} ManicVec2;

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} ManicColor;

typedef struct VectorPath VectorPath;
typedef struct MorphCache MorphCache;
typedef struct AnimationCache AnimationCache;
typedef struct TimelineCheckpoint TimelineCheckpoint;
typedef struct TimelineEdit TimelineEdit;
typedef struct SceneSnapshot SceneSnapshot;
typedef struct SceneUpdater SceneUpdater;

typedef enum {
    MOBJECT_CIRCLE,
    MOBJECT_LINE,
    MOBJECT_RECTANGLE,
    MOBJECT_GROUP,
    MOBJECT_PATH,
    MOBJECT_TRACKER
} MobjectType;

typedef enum {
    PATH_CAP_ROUND,
    PATH_CAP_BUTT,
    PATH_CAP_SQUARE
} PathCap;

typedef enum {
    PATH_JOIN_ROUND,
    PATH_JOIN_BEVEL,
    PATH_JOIN_MITER
} PathJoin;

typedef enum {
    PATH_FILL_EVEN_ODD,
    PATH_FILL_NONZERO
} PathFillRule;

typedef struct {
    MobjectType type;
    ManicVec2 position;
    ManicVec2 end;
    ManicVec2 size;
    float radius;
    float stroke_width;
    ManicColor color;
    int parent;
    int active;
    int removed;
    int layer;
    unsigned int generation;
    int reusable;
    unsigned long long order;
    float group_scale;
    float group_rotation;
    VectorPath *path;
    ManicColor fill_color;
    float path_scale;
    float line_scale;
    unsigned char path_fill_alpha;
    PathCap path_cap;
    PathJoin path_join;
    PathFillRule path_fill_rule;
    float rotation;
    float path_progress;
    const VectorPath *morph_source;
    const VectorPath *morph_target;
    float morph_alpha;
    const MorphCache *morph_cache;
    double tracker_value;
    unsigned int bindings_active;
} Mobject;

typedef enum {
    ANIMATION_MOVE,
    ANIMATION_FADE,
    ANIMATION_SCALE,
    ANIMATION_COLOR,
    ANIMATION_ROTATE,
    ANIMATION_REVEAL,
    ANIMATION_FOLLOW_CURVE,
    ANIMATION_MORPH,
    ANIMATION_FOLLOW_PATH,
    ANIMATION_TRACKER
} AnimationType;

typedef enum {
    EASING_SMOOTH,
    EASING_LINEAR,
    EASING_EASE_IN,
    EASING_EASE_OUT
} Easing;

typedef struct {
    AnimationType type;
    Easing easing;
    int mobject_index;
    ManicVec2 target_position;
    ManicColor target_color;
    unsigned char target_opacity;
    float scale_factor;
    float target_rotation;
    float target_progress;
    int curve_degree;
    ManicVec2 curve_points[4];
    int target_mobject_index;
    float duration;
    float offset;
    const VectorPath *motion_path;
    double motion_tolerance;
    double target_value;
    double precise_duration;
    int has_precise_duration;
} Animation;

typedef enum {
    ACTION_PLAY,
    ACTION_WAIT,
    ACTION_SHOW,
    ACTION_HIDE,
    ACTION_PARENT,
    ACTION_LAYER,
    ACTION_BINDING
} ActionType;

typedef struct {
    ActionType type;
    Animation *animations;
    Mobject *starting_mobjects;
    unsigned char *animation_started;
    AnimationCache *caches;
    int animation_count;
    float duration;
    double precise_duration;
    double duration_uncertainty;
    int has_precise_duration;
    int node;
    int parent;
    int layer;
    int binding;
    int enabled;
} SceneAction;

typedef struct {
    float frame_height;
    ManicColor background;
    ManicVec2 position;
} ManicCamera;

typedef struct {
    SceneAction *actions;
    int action_capacity;
    int action_count;
    int current_action;
    double action_elapsed;
    int action_started;
    Mobject *initial_mobjects;
    int initial_count;
    int initialized;
    TimelineCheckpoint *checkpoints;
    int checkpoint_count;
    int checkpoint_capacity;
    TimelineEdit *edits;
    int edit_count;
    int edit_capacity;
    int edit_cursor;
    double elapsed;
} Timeline;

typedef struct {
    Mobject *mobjects;
    int mobject_capacity;
    int mobject_count;
    ManicCamera camera;
    Timeline timeline;
    unsigned long long next_order;
    SceneUpdater *updaters;
    int updater_count;
    int updater_capacity;
} Scene;

typedef int (*ScenePositionUpdater)(double, double, const void *, ManicVec2 *);
typedef int (*SceneScalarUpdater)(double, double, const void *, double *);

typedef enum {
    SCENE_BIND_POSITION,
    SCENE_BIND_ROTATION,
    SCENE_BIND_OPACITY
} SceneBindingType;


typedef struct {
    int degree;
    ManicVec2 points[4];
    int starts_subpath;
    int closes_subpath;
} Bezier;

typedef struct {
    ManicVec2 min;
    ManicVec2 max;
} PathBounds;

struct VectorPath {
    Bezier *segments;
    size_t count;
    size_t capacity;
    ManicVec2 current;
    ManicVec2 subpath_start;
    int has_current;
    int new_subpath;
    int closed;
};


typedef struct {
    int group;
    float x_min;
    float x_max;
    float y_min;
    float y_max;
    float width;
    float height;
} MathAxes;

typedef double (*MathFunction)(double x, void *context);
typedef int (*MathParametric)(double t, void *context, double *x, double *y);

typedef struct {
    int enabled;
    double x_min;
    double x_max;
    double y_min;
    double y_max;
} GraphClip;

typedef enum {
    NUMBER_GENERAL,
    NUMBER_FIXED,
    NUMBER_SCIENTIFIC
} NumberNotation;

typedef enum {
    NUMBER_ALIGN_LEFT,
    NUMBER_ALIGN_CENTER,
    NUMBER_ALIGN_RIGHT
} NumberAlignment;

typedef struct {
    NumberNotation notation;
    NumberAlignment alignment;
    int precision;
    int show_plus;
} NumberFormat;


typedef struct {
    int max_depth;
    double tolerance;
} GraphSampling;


typedef struct {
    int width;
    int height;
    int fps;
    ManicColor background;
    const char *frames_directory;
    const char *scene_name;
    int supersample;
    const char *video_path;
    unsigned long long video_intervals;
} RenderConfig;


Mobject circle_create(ManicVec2, float, ManicColor);
Mobject line_create(ManicVec2, ManicVec2, ManicColor);
Mobject rectangle_create(ManicVec2, float, float, ManicColor);
Mobject group_create(ManicVec2);
Mobject path_mobject(VectorPath *, ManicVec2, ManicColor, ManicColor, float);
int bezier_evaluate(const Bezier *, double, ManicVec2 *);
int bezier_subdivide(const Bezier *, double, Bezier *, Bezier *);
int bezier_bounds(const Bezier *, PathBounds *);
double bezier_arc_parameter(const Bezier *, double);
int path_move_to(VectorPath *, ManicVec2);
int path_line_to(VectorPath *, ManicVec2);
int path_quadratic_to(VectorPath *, ManicVec2, ManicVec2);
int path_cubic_to(VectorPath *, ManicVec2, ManicVec2, ManicVec2);
int path_close(VectorPath *);
int path_clone(VectorPath *, const VectorPath *);
int path_compatible(const VectorPath *, const VectorPath *);
int path_bounds(const VectorPath *, PathBounds *);
void path_destroy(VectorPath *);
int path_rectangle(VectorPath *, float, float);
int path_circle(VectorPath *, float);
int path_morphable(const VectorPath *, const VectorPath *);
int path_interpolate(const VectorPath *, const VectorPath *, float, VectorPath *);
Animation animation_move_to(int, ManicVec2, float);
Animation animation_fade_to(int, unsigned char, float);
Animation animation_scale(int, float, float);
Animation animation_color_to(int, ManicColor, float);
Animation animation_with_easing(Animation, Easing);
Animation animation_rotate_to(int, float, float);
Animation animation_reveal_to(int, float, float);
Animation animation_follow_curve(int, Bezier, float);
Animation animation_follow_path(int, const VectorPath *, double, float);
Animation animation_morph_to(int, int, float);
Animation animation_tracker_to(int, double, float);
Animation animation_with_offset(Animation, float);
Animation animation_with_duration_precise(Animation, double);
Mobject *scene_get(Scene *, int);
int scene_read(const Scene *, int, Mobject *);
SceneSnapshot *scene_snapshot_create(const Scene *, int);
const Mobject *scene_snapshot_get(const SceneSnapshot *);
int scene_snapshot_refresh(SceneSnapshot *, const Scene *, int);
void scene_snapshot_destroy(SceneSnapshot *);
int scene_add(Scene *, Mobject);
int scene_add_child(Scene *, int, Mobject);
int scene_group(Scene *, ManicVec2, const int *, int);
int scene_tracker(Scene *, double);
int scene_tracker_value(const Scene *, int, double *);
int scene_tracker_set(Scene *, int, double);
int scene_bind_position(Scene *, int, int, ScenePositionUpdater, const void *);
int scene_bind_position_copy(Scene *, int, int, ScenePositionUpdater, const void *, size_t);
int scene_unbind_position(Scene *, int);
int scene_bind_rotation(Scene *, int, int, SceneScalarUpdater, const void *);
int scene_bind_rotation_copy(Scene *, int, int, SceneScalarUpdater, const void *, size_t);
int scene_unbind_rotation(Scene *, int);
int scene_bind_opacity(Scene *, int, int, SceneScalarUpdater, const void *);
int scene_bind_opacity_copy(Scene *, int, int, SceneScalarUpdater, const void *, size_t);
int scene_unbind_opacity(Scene *, int);
int scene_set_parent(Scene *, int, int);
int scene_remove(Scene *, int);
int scene_set_layer(Scene *, int, int);
int scene_collect_removed(Scene *);
void scene_destroy(Scene *);
int timeline_play_many(Timeline *, const Scene *, const Animation *, int);
int timeline_wait(Timeline *, float);
int timeline_wait_precise(Timeline *, double);
int timeline_update(Timeline *, Scene *, double);
int timeline_seek(Timeline *, Scene *, double);
int scene_rewind(Scene *);
int scene_schedule_add(Scene *, int);
int scene_schedule_remove(Scene *, int);
int scene_schedule_show(Scene *, int);
int scene_schedule_parent(Scene *, int, int);
int scene_schedule_layer(Scene *, int, int);
int scene_schedule_binding(Scene *, int, SceneBindingType, int);
int scene_set_binding(Scene *, int, SceneBindingType, int);
int scene_play_many(Scene *, const Animation *, int);
int scene_play_staggered(Scene *, const Animation *, int, float);
int scene_play_sequence(Scene *, const Animation *, int);
int scene_play(Scene *, Animation);
int scene_wait(Scene *, float);
int scene_wait_precise(Scene *, double);
int scene_update(Scene *, double);
RenderConfig render_config_default(void);
int render_config_parse(RenderConfig *, int, char **);
int scene_preview(Scene *, const RenderConfig *);
int scene_frame_intervals(const Scene *, int, unsigned long long *);
int scene_export_frames(Scene *, const RenderConfig *);
int video_output_available(const char *);
int encode_video(const RenderConfig *);
int axes_point(const MathAxes *, double, double, ManicVec2 *);
int axes_coordinates(const MathAxes *, ManicVec2, double *, double *);
GraphClip graph_clip_axes(const MathAxes *);
int path_parametric_graph(VectorPath *, const MathAxes *, MathParametric, void *, double, double,
    int, GraphSampling, GraphClip, const double *, size_t);
int scene_parametric_graph(Scene *, const MathAxes *, MathParametric, void *, double, double,
    int, GraphSampling, GraphClip, const double *, size_t, ManicColor);
int scene_function_graph_config(Scene *, const MathAxes *, MathFunction, void *, int,
    GraphSampling, GraphClip, const double *, size_t, ManicColor);
int scene_number_plane(Scene *, MathAxes *, ManicVec2, float, float, ManicColor, ManicColor);
int scene_dot(Scene *, ManicVec2, float, ManicColor);
int scene_arrow(Scene *, ManicVec2, ManicVec2, float, ManicColor);
int scene_tangent_indicator(Scene *, ManicVec2, ManicVec2, float, ManicColor);
int scene_number_line(Scene *, ManicVec2, ManicVec2, float, float, float, float, ManicColor);
int scene_axes(Scene *, MathAxes *, ManicVec2, float, ManicColor);
int scene_function_graph(Scene *, const MathAxes *, MathFunction, void *, int, ManicColor);
int scene_function_graph_adaptive(Scene *, const MathAxes *, MathFunction, void *, int, GraphSampling, ManicColor);
int scene_function_graph_with_breaks(Scene *, const MathAxes *, MathFunction, void *, int, GraphSampling, const double *, size_t, ManicColor);
int scene_number_label(Scene *, double, ManicVec2, float, ManicColor);
int scene_number_label_formatted(Scene *, double, ManicVec2, float, ManicColor, NumberFormat);
int scene_axes_labels(Scene *, const MathAxes *, float, float, ManicColor);
int scene_number_line_labels(Scene *, int, ManicVec2, ManicVec2, float, float, float, float, float, ManicColor);

#endif
