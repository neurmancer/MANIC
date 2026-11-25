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
} Timeline;

typedef struct {
    Mobject *mobjects;
    int mobject_capacity;
    int mobject_count;
    ManicCamera camera;
    Timeline timeline;
    unsigned long long next_order;
} Scene;

static int scene_slot(const Scene *scene, int handle)
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
