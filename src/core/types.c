#include <raylib.h>

#define SCENE_MAX_MOBJECTS 128
#define SCENE_MAX_ACTIONS 256
#define SCENE_MAX_PARALLEL_ANIMATIONS 16

typedef enum {
    MOBJECT_CIRCLE,
    MOBJECT_LINE,
    MOBJECT_RECTANGLE
} MobjectType;

typedef struct {
    MobjectType type;
    Vector2 position;
    Vector2 end;
    Vector2 size;
    float radius;
    float stroke_width;
    Color color;
} Mobject;

typedef enum {
    ANIMATION_MOVE,
    ANIMATION_FADE,
    ANIMATION_SCALE,
    ANIMATION_COLOR
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
    Vector2 target_position;
    Color target_color;
    unsigned char target_opacity;
    float scale_factor;
    float duration;
} Animation;

typedef enum {
    ACTION_PLAY,
    ACTION_WAIT
} ActionType;

typedef struct {
    ActionType type;
    Animation animations[SCENE_MAX_PARALLEL_ANIMATIONS];
    int animation_count;
    float duration;
} SceneAction;

typedef struct {
    float frame_height;
    Color background;
    Vector2 position;
} ManicCamera;

typedef struct {
    Mobject mobjects[SCENE_MAX_MOBJECTS];
    int mobject_count;
    SceneAction actions[SCENE_MAX_ACTIONS];
    int action_count;
    int current_action;
    float action_elapsed;
    int action_started;
    Mobject starting_mobjects[SCENE_MAX_PARALLEL_ANIMATIONS];
    ManicCamera camera;
} Scene;
