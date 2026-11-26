#include "internal.h"
#include <string.h>

struct TimelineCheckpoint {
    double time;
    int action;
    Mobject *mobjects;
};

void timeline_invalidate_checkpoints(Timeline *timeline)
{
    for(int i = 0; i < timeline->checkpoint_count; i++) {free(timeline->checkpoints[i].mobjects);}
    free(timeline->checkpoints);
    timeline->checkpoints = NULL;
    timeline->checkpoint_count = 0;
    timeline->checkpoint_capacity = 0;
}

static void timeline_checkpoint(Timeline *timeline, const Scene *scene)
{
    TimelineCheckpoint *items;
    Mobject *snapshot;

    if(timeline->checkpoint_count &&
        timeline->checkpoints[timeline->checkpoint_count - 1].action >= timeline->current_action) {return;}
    if(timeline->checkpoint_count == timeline->checkpoint_capacity) {
        int capacity;

        if(timeline->checkpoint_capacity > INT_MAX / 2) {return;}
        capacity = timeline->checkpoint_capacity ? timeline->checkpoint_capacity * 2 : 8;
        if(capacity < timeline->checkpoint_capacity || (size_t)capacity > SIZE_MAX / sizeof(*items)) {return;}
        items = realloc(timeline->checkpoints, (size_t)capacity * sizeof(*items));
        if(!items) {return;}
        timeline->checkpoints = items;
        timeline->checkpoint_capacity = capacity;
    }
    snapshot = scene->mobject_count ? malloc((size_t)scene->mobject_count * sizeof(*snapshot)) : NULL;
    if(scene->mobject_count && !snapshot) {return;}
    if(snapshot) {memcpy(snapshot, scene->mobjects, (size_t)scene->mobject_count * sizeof(*snapshot));}
    timeline->checkpoints[timeline->checkpoint_count++] =
        (TimelineCheckpoint){timeline->elapsed, timeline->current_action, snapshot};
}

static int timeline_reserve_action(Timeline *timeline)
{
    int capacity;
    SceneAction *actions;

    if(timeline->action_count < timeline->action_capacity) {return(1);}
    if(timeline->action_count == INT_MAX) {return(0);}
    capacity = timeline->action_capacity ? timeline->action_capacity : 8;
    if(timeline->action_capacity) {
        capacity = timeline->action_capacity > INT_MAX / 2 ? INT_MAX : timeline->action_capacity * 2;
    }
    if((size_t)capacity > SIZE_MAX / sizeof(*actions)) {return(0);}
    actions = realloc(timeline->actions, (size_t)capacity * sizeof(*actions));
    if(actions == NULL) {return(0);}
    timeline->actions = actions;
    timeline->action_capacity = capacity;
    return(1);
}

int timeline_play_many(Timeline *timeline, const Scene *scene, const Animation *animations, int count)
{
    SceneAction action = {0};

    if(count <= 0 || animations == NULL || (size_t)count > SIZE_MAX / sizeof(*action.animations) ||
        (size_t)count > SIZE_MAX / sizeof(*action.starting_mobjects)) {return(0);}
    action.type = ACTION_PLAY;
    for(int i = 0; i < count; i++) {
        Animation animation = animations[i];
        int slot = scene_slot(scene, animation.mobject_index);
        int target = scene_slot(scene, animation.target_mobject_index);

        if(slot < 0 || scene->mobjects[slot].removed ||
            !isfinite(animation.duration) || animation.duration < 0.0f ||
            !isfinite(animation.offset) || animation.offset < 0.0f ||
            !isfinite(animation.offset + animation.duration) ||
            animation.type < ANIMATION_MOVE || animation.type > ANIMATION_MORPH ||
            (scene->mobjects[slot].type == MOBJECT_GROUP &&
                animation.type != ANIMATION_MOVE && animation.type != ANIMATION_SCALE &&
                animation.type != ANIMATION_ROTATE && animation.type != ANIMATION_FOLLOW_CURVE) ||
            (animation.type == ANIMATION_ROTATE && !isfinite(animation.target_rotation)) ||
            (animation.type == ANIMATION_REVEAL &&
                (!isfinite(animation.target_progress) || animation.target_progress < 0.0f ||
                 animation.target_progress > 1.0f)) ||
            (animation.type == ANIMATION_FOLLOW_CURVE &&
                (animation.curve_degree < 1 || animation.curve_degree > 3)) ||
            (animation.type == ANIMATION_MORPH &&
                (scene->mobjects[slot].type != MOBJECT_PATH ||
                 target < 0 || !scene->mobjects[target].active ||
                 scene->mobjects[target].type != MOBJECT_PATH ||
                 !path_morphable(scene->mobjects[slot].path,
                     scene->mobjects[target].path))) ||
            animation.easing < EASING_SMOOTH || animation.easing > EASING_EASE_OUT ||
            (animation.type == ANIMATION_SCALE &&
                (!isfinite(animation.scale_factor) || animation.scale_factor < 0.0f)) ||
            (animation.type == ANIMATION_MOVE &&
                (!isfinite(animation.target_position.x) || !isfinite(animation.target_position.y)))) {
            return(0);
        }
        if(animation.type == ANIMATION_FOLLOW_CURVE) {
            for(int point = 0; point <= animation.curve_degree; point++) {
                if(!vector_valid(animation.curve_points[point])) {return(0);}
            }
        }
        for(int j = 0; j < i; j++) {
            Animation previous = animations[j];

            if(previous.mobject_index == animation.mobject_index &&
                (previous.type == animation.type ||
                    ((previous.type == ANIMATION_MOVE && animation.type == ANIMATION_FOLLOW_CURVE) ||
                     (previous.type == ANIMATION_FOLLOW_CURVE && animation.type == ANIMATION_MOVE)) ||
                    (scene->mobjects[slot].type == MOBJECT_LINE &&
                        (((previous.type == ANIMATION_MOVE || previous.type == ANIMATION_FOLLOW_CURVE) &&
                          animation.type == ANIMATION_SCALE) ||
                         (previous.type == ANIMATION_SCALE &&
                          (animation.type == ANIMATION_MOVE || animation.type == ANIMATION_FOLLOW_CURVE)))))) {
                float previous_end = previous.offset + previous.duration;
                float animation_end = animation.offset + animation.duration;

                if(previous.offset == animation.offset ||
                    (previous_end > animation.offset && animation_end > previous.offset)) {return(0);}
            }
        }
        if(animation.offset + animation.duration > action.duration) {
            action.duration = animation.offset + animation.duration;
        }
    }
    action.animations = malloc((size_t)count * sizeof(*action.animations));
    action.starting_mobjects = malloc((size_t)count * sizeof(*action.starting_mobjects));
    action.caches = calloc((size_t)count, sizeof(*action.caches));
    action.animation_started = calloc((size_t)count, sizeof(*action.animation_started));
    if(action.animations == NULL || action.starting_mobjects == NULL || action.animation_started == NULL || action.caches == NULL ||
        !timeline_reserve_action(timeline)) {
        free(action.animations);
        free(action.starting_mobjects);
        free(action.animation_started);
        free(action.caches);
        return(0);
    }
    memcpy(action.animations, animations, (size_t)count * sizeof(*animations));
    for(int i = 0; i < count; i++) {
        if(animations[i].type == ANIMATION_FOLLOW_CURVE) {
            Bezier curve = {.degree = animations[i].curve_degree};
            memcpy(curve.points, animations[i].curve_points, sizeof(curve.points));
            action.caches[i].arc_lengths = malloc(sizeof(*action.caches[i].arc_lengths));
            if(!action.caches[i].arc_lengths || !bezier_arc_table(&curve, action.caches[i].arc_lengths)) {
                for(int j = 0; j <= i; j++) {free(action.caches[j].arc_lengths);}
                free(action.caches);
                free(action.animations);
                free(action.starting_mobjects);
                free(action.animation_started);
                return(0);
            }
        }
    }
    action.animation_count = count;
    timeline->actions[timeline->action_count++] = action;
    return(1);
}

int timeline_wait(Timeline *timeline, float duration)
{
    SceneAction action = {0};

    if(!isfinite(duration) || duration < 0.0f || !timeline_reserve_action(timeline)) {return(0);}
    action.type = ACTION_WAIT;
    action.duration = duration;
    timeline->actions[timeline->action_count++] = action;
    return(1);
}

static int timeline_visibility(Scene *scene, int node, int show, int initially_hidden)
{
    SceneAction action = {0};
    Timeline *timeline = &scene->timeline;

    int slot = scene_slot(scene, node);

    if(slot < 0 || scene->mobjects[slot].removed || !timeline_reserve_action(timeline)) {return(0);}
    action.type = show ? ACTION_SHOW : ACTION_HIDE;
    action.node = node;
    timeline->actions[timeline->action_count++] = action;
    if(initially_hidden) {
        timeline_invalidate_checkpoints(timeline);
        scene->mobjects[slot].active = 0;
        if(timeline->initialized) {timeline->initial_mobjects[slot].active = 0;}
    }
    return(1);
}

int scene_schedule_add(Scene *scene, int node)
{
    return(timeline_visibility(scene, node, 1, 1));
}

int scene_schedule_remove(Scene *scene, int node)
{
    return(timeline_visibility(scene, node, 0, 0));
}

int scene_schedule_show(Scene *scene, int node)
{
    return(timeline_visibility(scene, node, 1, 0));
}

static int timeline_apply_action(SceneAction *action, Scene *scene, double elapsed)
{
    for(int pass = 0; pass < 2; pass++) {
        for(int i = 0; i < action->animation_count; i++) {
            Animation *animation = &action->animations[i];
            float alpha;
            int slot = scene_slot(scene, animation->mobject_index);

            if(slot < 0 || scene->mobjects[slot].removed) {action->animation_started[i] = 2; continue;}
            if(action->animation_started[i] == 2 || elapsed < animation->offset ||
                (pass == 0 && action->animation_started[i] == 0) ||
                (pass == 1 && action->animation_started[i] != 0)) {continue;}
            if(!action->animation_started[i]) {
                if(animation->type == ANIMATION_MORPH) {
                    const Mobject *start = &scene->mobjects[slot];
                    const VectorPath *source = start->morph_alpha >= 1.0f && start->morph_target ? start->morph_target : start->path;
                    int target_slot = scene_slot(scene, animation->target_mobject_index);
                    if(target_slot >= 0) {
                        const VectorPath *target = scene->mobjects[target_slot].path;
                        MorphCache *item = action->caches[i].morphs;
                        while(item && (item->source != source || item->target != target)) {item = item->next;}
                        if(!item) {
                            item = morph_cache_create(source, target);
                            if(!item) {return(0);}
                            item->next = action->caches[i].morphs;
                            action->caches[i].morphs = item;
                        }
                    }
                }
                action->starting_mobjects[i] = scene->mobjects[slot];
                action->animation_started[i] = 1;
            }
            alpha = animation->duration > 0.0f ? (elapsed - animation->offset) / animation->duration : 1.0f;
            if(elapsed >= animation->offset + animation->duration) {alpha = 1.0f;}
            if(scene->mobjects[slot].active) {
                animation_interpolate(animation, &action->starting_mobjects[i],
                    &scene->mobjects[slot], scene, &action->caches[i], alpha);
            }
            if(alpha >= 1.0f) {action->animation_started[i] = 2;}
        }
    }
    return(1);
}

int timeline_update(Timeline *timeline, Scene *scene, double dt)
{
    if(!isfinite(dt) || dt < 0.0f) {return(0);}
    if(!timeline->initialized) {
        if(scene->mobject_count > 0) {
            timeline->initial_mobjects = malloc((size_t)scene->mobject_count * sizeof(*timeline->initial_mobjects));
            if(timeline->initial_mobjects == NULL) {return(0);}
            memcpy(timeline->initial_mobjects, scene->mobjects,
                (size_t)scene->mobject_count * sizeof(*scene->mobjects));
        }
        timeline->initial_count = scene->mobject_count;
        timeline->initialized = 1;
    }

    while(timeline->current_action < timeline->action_count) {
        SceneAction *action = &timeline->actions[timeline->current_action];
        double remaining;
        double advance;

        if(!timeline->action_started) {
            if(action->type == ACTION_SHOW || action->type == ACTION_HIDE) {
                int slot = scene_slot(scene, action->node);

                if(slot >= 0 && !scene->mobjects[slot].removed) {
                    scene->mobjects[slot].active = action->type == ACTION_SHOW;
                }
            }
            for(int i = 0; i < action->animation_count; i++) {action->animation_started[i] = 0;}
            timeline->action_started = 1;
            if(!timeline_apply_action(action, scene, 0.0f)) {return(0);}
        }
        remaining = action->duration - timeline->action_elapsed;
        advance = dt < remaining ? dt : remaining;
        for(int i = 0; i < action->animation_count; i++) {
            float offset = action->animations[i].offset;

            if(!action->animation_started[i] && offset > timeline->action_elapsed &&
                offset - timeline->action_elapsed < advance) {
                advance = offset - timeline->action_elapsed;
            }
        }
        timeline->action_elapsed += advance;
        timeline->elapsed += advance;
        dt -= advance;
        if(!timeline_apply_action(action, scene, timeline->action_elapsed)) {return(0);}
        if(timeline->action_elapsed < action->duration) {
            if(dt > 0.0f && advance > 0.0f) {continue;}
            break;
        }
        timeline->current_action++;
        timeline->action_elapsed = 0.0f;
        timeline->action_started = 0;
        timeline_checkpoint(timeline, scene);
    }
    return(1);
}


int timeline_seek(Timeline *timeline, Scene *scene, double time)
{
    if(!isfinite(time) || time < 0.0f || !timeline->initialized ||
        timeline->initial_count != scene->mobject_count) {return(0);}
    int low = 0, high = timeline->checkpoint_count;
    while(low < high) {
        int mid = low + (high - low) / 2;
        if(timeline->checkpoints[mid].time <= time) {low = mid + 1;}
        else {high = mid;}
    }
    const TimelineCheckpoint *checkpoint = low ? &timeline->checkpoints[low - 1] : NULL;
    const Mobject *snapshot = checkpoint ? checkpoint->mobjects : timeline->initial_mobjects;
    for(int i = 0; i < scene->mobject_count; i++) {
        int removed = scene->mobjects[i].removed;
        int parent = scene->mobjects[i].parent;
        int layer = scene->mobjects[i].layer;
        scene->mobjects[i] = snapshot[i];
        scene->mobjects[i].removed = removed;
        if(removed) {scene->mobjects[i].active = 0;}
        scene->mobjects[i].parent = parent;
        scene->mobjects[i].layer = layer;
    }
    timeline->current_action = checkpoint ? checkpoint->action : 0;
    timeline->elapsed = checkpoint ? checkpoint->time : 0;
    timeline->action_elapsed = 0;
    timeline->action_started = 0;
    return(timeline_update(timeline, scene, time - timeline->elapsed));
}

int scene_play_many(Scene *scene, const Animation *animations, int count)
{
    return(timeline_play_many(&scene->timeline, scene, animations, count));
}

int scene_play(Scene *scene, Animation animation)
{
    return(scene_play_many(scene, &animation, 1));
}

int scene_wait(Scene *scene, float duration)
{
    return(timeline_wait(&scene->timeline, duration));
}

int scene_update(Scene *scene, double dt)
{
    return(timeline_update(&scene->timeline, scene, dt));
}
