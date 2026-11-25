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

typedef enum {
    MOBJECT_CIRCLE,
    MOBJECT_LINE,
    MOBJECT_RECTANGLE,
    MOBJECT_GROUP,
    MOBJECT_PATH
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
} Mobject;

typedef enum {
    ANIMATION_MOVE,
    ANIMATION_FADE,
    ANIMATION_SCALE,
    ANIMATION_COLOR,
    ANIMATION_ROTATE,
    ANIMATION_REVEAL,
    ANIMATION_FOLLOW_CURVE,
    ANIMATION_MORPH
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
} Animation;

typedef enum {
    ACTION_PLAY,
    ACTION_WAIT,
    ACTION_SHOW,
    ACTION_HIDE
} ActionType;

typedef struct {
    ActionType type;
    Animation *animations;
    Mobject *starting_mobjects;
    unsigned char *animation_started;
    AnimationCache *caches;
    int animation_count;
    float duration;
    int node;
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
    double elapsed;
} Timeline;

typedef struct {
    Mobject *mobjects;
    int mobject_capacity;
    int mobject_count;
    ManicCamera camera;
    Timeline timeline;
    unsigned long long next_order;
} Scene;


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
Animation animation_morph_to(int, int, float);
Animation animation_with_offset(Animation, float);
Mobject *scene_get(Scene *, int);
int scene_add(Scene *, Mobject);
int scene_set_parent(Scene *, int, int);
int scene_remove(Scene *, int);
int scene_set_layer(Scene *, int, int);
int scene_collect_removed(Scene *);
void scene_destroy(Scene *);
int timeline_play_many(Timeline *, const Scene *, const Animation *, int);
int timeline_wait(Timeline *, float);
int timeline_update(Timeline *, Scene *, double);
int timeline_seek(Timeline *, Scene *, double);
int scene_schedule_add(Scene *, int);
int scene_schedule_remove(Scene *, int);
int scene_schedule_show(Scene *, int);
int scene_play_many(Scene *, const Animation *, int);
int scene_play(Scene *, Animation);
int scene_wait(Scene *, float);
int scene_update(Scene *, double);
RenderConfig render_config_default(void);
int render_config_parse(RenderConfig *, int, char **);
int scene_preview(Scene *, const RenderConfig *);
int scene_frame_intervals(const Scene *, int, unsigned long long *);
int scene_export_frames(Scene *, const RenderConfig *);
int video_output_available(const char *);
int encode_video(const RenderConfig *);
int axes_point(const MathAxes *, double, double, ManicVec2 *);
int scene_number_line(Scene *, ManicVec2, ManicVec2, float, float, float, float, ManicColor);
int scene_axes(Scene *, MathAxes *, ManicVec2, float, ManicColor);
int scene_function_graph(Scene *, const MathAxes *, MathFunction, void *, int, ManicColor);
int scene_function_graph_adaptive(Scene *, const MathAxes *, MathFunction, void *, int, GraphSampling, ManicColor);
int scene_function_graph_with_breaks(Scene *, const MathAxes *, MathFunction, void *, int, GraphSampling, const double *, size_t, ManicColor);
int scene_number_label(Scene *, double, ManicVec2, float, ManicColor);
int scene_axes_labels(Scene *, const MathAxes *, float, float, ManicColor);
int scene_number_line_labels(Scene *, int, ManicVec2, ManicVec2, float, float, float, float, float, ManicColor);

#endif
