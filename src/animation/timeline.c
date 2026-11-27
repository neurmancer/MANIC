#include "internal.h"
#include <string.h>
#include <float.h>

#define MAX_CHECKPOINTS 32
#define CHECKPOINT_BYTES (8u * 1024u * 1024u)

struct TimelineCheckpoint {
    double time;
    int action;
    int edit_cursor;
    Mobject *mobjects;
};

struct TimelineEdit {
    int action;
    double action_elapsed;
    int started;
    TimelineEditType type;
    int node;
    int integer;
    double value;
};

int timeline_reserve_edits(Timeline *timeline, int extra)
{
    int capacity;
    TimelineEdit *items;

    if(extra < 0 || timeline->edit_count > INT_MAX - extra) {return(0);}
    if(timeline->edit_capacity >= timeline->edit_count + extra) {return(1);}
    capacity = timeline->edit_capacity ? timeline->edit_capacity : 8;
    while(capacity < timeline->edit_count + extra) {
        if(capacity > INT_MAX / 2) {capacity = INT_MAX; break;}
        capacity *= 2;
    }
    if((size_t)capacity > SIZE_MAX / sizeof(*items)) {return(0);}
    items = realloc(timeline->edits, (size_t)capacity * sizeof(*items));
    if(items == NULL) {return(0);}
    timeline->edits = items;
    timeline->edit_capacity = capacity;
    return(1);
}

int timeline_record_edit(Timeline *timeline, TimelineEditType type, int node, int integer, double value)
{
    TimelineEdit edit;
    int index = timeline->edit_cursor;

    if(!timeline_reserve_edits(timeline, 1)) {return(0);}
    edit = (TimelineEdit){timeline->current_action, timeline->action_elapsed, timeline->action_started,
        type, node, integer, value};
    memmove(&timeline->edits[index + 1], &timeline->edits[index],
        (size_t)(timeline->edit_count - index) * sizeof(*timeline->edits));
    timeline->edits[index] = edit;
    timeline->edit_count++;
    timeline->edit_cursor++;
    timeline_invalidate_checkpoints(timeline);
    return(1);
}

int timeline_binding_has_edits(const Timeline *timeline, int node, SceneUpdaterType type)
{
    for(int i = 0; i < timeline->edit_count; i++) {
        const TimelineEdit *edit = &timeline->edits[i];

        if(edit->type == EDIT_BINDING && edit->node == node && edit->integer == (int)type) {return(1);}
    }
    return(0);
}

void timeline_retract_edit(Timeline *timeline)
{
    int index = --timeline->edit_cursor;

    timeline->edit_count--;
    memmove(&timeline->edits[index], &timeline->edits[index + 1],
        (size_t)(timeline->edit_count - index) * sizeof(*timeline->edits));
}

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
    size_t snapshot_size = (size_t)scene->mobject_count * sizeof(*snapshot);
    size_t limit = snapshot_size ? CHECKPOINT_BYTES / snapshot_size : MAX_CHECKPOINTS;

    if(limit > MAX_CHECKPOINTS) {limit = MAX_CHECKPOINTS;}
    if(limit == 0) {return;}
    if(timeline->checkpoint_count &&
        timeline->checkpoints[timeline->checkpoint_count - 1].action >= timeline->current_action) {return;}
    if((size_t)timeline->checkpoint_count >= limit) {
        int kept = 0;

        for(int i = 0; i < timeline->checkpoint_count; i++) {
            if(i % 2 == 0 && i + 1 < timeline->checkpoint_count) {
                timeline->checkpoints[kept++] = timeline->checkpoints[i];
            }
            else {free(timeline->checkpoints[i].mobjects);}
        }
        timeline->checkpoint_count = kept;
    }
    if(timeline->checkpoint_count == timeline->checkpoint_capacity) {
        int capacity;

        if(timeline->checkpoint_capacity > INT_MAX / 2) {return;}
        capacity = timeline->checkpoint_capacity ? timeline->checkpoint_capacity * 2 : 8;
        if((size_t)capacity > limit) {capacity = (int)limit;}
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
        (TimelineCheckpoint){timeline->elapsed, timeline->current_action, timeline->edit_cursor, snapshot};
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

static int timeline_binding_enabled(const Timeline *timeline, const Scene *scene, const SceneUpdater *updater)
{
    int slot = scene_slot(scene, updater->node);
    const Mobject *initial = timeline->initialized ? timeline->initial_mobjects : scene->mobjects;
    int enabled = slot >= 0 && (initial[slot].bindings_active & (1u << updater->type)) != 0;
    int edit_index = 0;

    for(int i = 0; i <= timeline->action_count; i++) {
        while(edit_index < timeline->edit_count && timeline->edits[edit_index].action == i &&
            !timeline->edits[edit_index].started) {
            const TimelineEdit *edit = &timeline->edits[edit_index++];

            if(edit->type == EDIT_BINDING && edit->node == updater->node && edit->integer == (int)updater->type) {
                enabled = edit->value != 0;
            }
        }
        if(i < timeline->action_count) {
            const SceneAction *action = &timeline->actions[i];

            if(action->type == ACTION_BINDING && action->node == updater->node &&
                action->binding == (int)updater->type) {enabled = action->enabled;}
        }
        while(edit_index < timeline->edit_count && timeline->edits[edit_index].action == i) {
            const TimelineEdit *edit = &timeline->edits[edit_index++];

            if(edit->type == EDIT_BINDING && edit->node == updater->node && edit->integer == (int)updater->type) {
                enabled = edit->value != 0;
            }
        }
    }
    return(enabled);
}

double timeline_action_duration(const SceneAction *action)
{
    return(action->has_precise_duration ? action->precise_duration : action->duration);
}

static double animation_end(const Animation *animation)
{
    if(animation->has_precise_duration) {return((double)animation->offset + animation->precise_duration);}
    return((float)(animation->offset + animation->duration));
}

static double duration_half_ulp(double value)
{
    double upper = nextafter(value, INFINITY);
    double lower = nextafter(value, -INFINITY);

    return(0.5 * fmax(isfinite(upper) ? upper - value : 0, value - lower));
}

static double animation_end_uncertainty(const Animation *animation)
{
    double end = animation_end(animation);

    if(!animation->has_precise_duration) {
        float value = (float)end;
        float next = nextafterf(value, INFINITY);
        float previous = nextafterf(value, -INFINITY);

        return(0.5 * fmax(isfinite(next) ? (double)next - value : 0, (double)value - previous));
    }
    {
        double error = duration_half_ulp(animation->precise_duration);

        if(animation->offset > 0) {
            float next = nextafterf(animation->offset, INFINITY);
            float previous = nextafterf(animation->offset, -INFINITY);

            error += 0.5 * fmax(isfinite(next) ? (double)next - animation->offset : 0,
                (double)animation->offset - previous);
            error += duration_half_ulp(end);
        }
        return(error);
    }
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
            !isfinite(animation_duration(&animation)) || animation_duration(&animation) < 0.0 ||
            !isfinite(animation.offset) || animation.offset < 0.0f ||
            !isfinite(animation_end(&animation)) ||
            animation.type < ANIMATION_MOVE || animation.type > ANIMATION_TRACKER ||
            ((scene->mobjects[slot].type == MOBJECT_TRACKER) != (animation.type == ANIMATION_TRACKER)) ||
            (animation.type == ANIMATION_TRACKER && !isfinite(animation.target_value)) ||
            (scene->mobjects[slot].type == MOBJECT_GROUP &&
                animation.type != ANIMATION_MOVE && animation.type != ANIMATION_SCALE &&
                animation.type != ANIMATION_ROTATE && animation.type != ANIMATION_FOLLOW_CURVE &&
                animation.type != ANIMATION_FOLLOW_PATH) ||
            (animation.type == ANIMATION_ROTATE && !isfinite(animation.target_rotation)) ||
            (animation.type == ANIMATION_REVEAL &&
                (!isfinite(animation.target_progress) || animation.target_progress < 0.0f ||
                 animation.target_progress > 1.0f)) ||
            (animation.type == ANIMATION_FOLLOW_CURVE &&
                (animation.curve_degree < 1 || animation.curve_degree > 3)) ||
            (animation.type == ANIMATION_FOLLOW_PATH &&
                (animation.motion_path == NULL || !isfinite(animation.motion_tolerance) ||
                 animation.motion_tolerance <= 0)) ||
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
        for(int j = 0; j < scene->updater_count; j++) {
            if(scene->updaters[j].node == animation.mobject_index &&
                scene_updater_conflicts(scene->updaters[j].type, animation.type) &&
                timeline_binding_enabled(timeline, scene, &scene->updaters[j])) {return(0);}
        }
        if(animation.type == ANIMATION_FOLLOW_CURVE) {
            for(int point = 0; point <= animation.curve_degree; point++) {
                if(!vector_valid(animation.curve_points[point])) {return(0);}
            }
        }
        for(int j = 0; j < i; j++) {
            Animation previous = animations[j];
            int previous_motion = previous.type == ANIMATION_MOVE || previous.type == ANIMATION_FOLLOW_CURVE ||
                previous.type == ANIMATION_FOLLOW_PATH;
            int animation_motion = animation.type == ANIMATION_MOVE || animation.type == ANIMATION_FOLLOW_CURVE ||
                animation.type == ANIMATION_FOLLOW_PATH;

            if(previous.mobject_index == animation.mobject_index &&
                (previous.type == animation.type || (previous_motion && animation_motion) ||
                    (scene->mobjects[slot].type == MOBJECT_LINE &&
                        ((previous_motion && animation.type == ANIMATION_SCALE) ||
                         (previous.type == ANIMATION_SCALE && animation_motion))))) {
                double previous_end = animation_end(&previous);
                double current_end = animation_end(&animation);

                if(previous.offset == animation.offset ||
                    (previous_end > animation.offset && current_end > previous.offset)) {return(0);}
            }
        }
        {
            double end = animation_end(&animation);
            double current = timeline_action_duration(&action);
            double error = animation_end_uncertainty(&animation);
            double current_error = action.has_precise_duration ? action.duration_uncertainty :
                animation_end_uncertainty(&(Animation){.duration = action.duration});

            if(end > current || (end == current && error > current_error)) {
                action.has_precise_duration = 1;
                action.duration = end > FLT_MAX ? FLT_MAX : (float)end;
                action.precise_duration = end;
                action.duration_uncertainty = error;
            }
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
        if(animations[i].type == ANIMATION_FOLLOW_PATH) {
            action.caches[i].path_arc = path_arc_create(animations[i].motion_path, animations[i].motion_tolerance);
            if(!action.caches[i].path_arc) {
                for(int j = 0; j <= i; j++) {
                    free(action.caches[j].arc_lengths);
                    path_arc_destroy(action.caches[j].path_arc);
                }
                free(action.caches);
                free(action.animations);
                free(action.starting_mobjects);
                free(action.animation_started);
                return(0);
            }
            action.animations[i].motion_path = NULL;
        }
        if(animations[i].type == ANIMATION_FOLLOW_CURVE) {
            Bezier curve = {.degree = animations[i].curve_degree};
            memcpy(curve.points, animations[i].curve_points, sizeof(curve.points));
            action.caches[i].arc_lengths = malloc(sizeof(*action.caches[i].arc_lengths));
            if(!action.caches[i].arc_lengths || !bezier_arc_table(&curve, action.caches[i].arc_lengths)) {
                for(int j = 0; j <= i; j++) {
                    free(action.caches[j].arc_lengths);
                    path_arc_destroy(action.caches[j].path_arc);
                }
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

int timeline_wait_precise(Timeline *timeline, double duration)
{
    SceneAction action = {0};

    if(!isfinite(duration) || duration < 0 || !timeline_reserve_action(timeline)) {return(0);}
    action.type = ACTION_WAIT;
    action.duration = duration > FLT_MAX ? FLT_MAX : (float)duration;
    action.precise_duration = duration;
    action.duration_uncertainty = duration_half_ulp(duration);
    action.has_precise_duration = 1;
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

int scene_schedule_parent(Scene *scene, int child, int parent)
{
    SceneAction action = {0};
    int child_slot, parent_slot;

    if(scene == NULL || (child_slot = scene_slot(scene, child)) < 0 || scene->mobjects[child_slot].removed ||
        parent < -1 || child == parent) {return(0);}
    parent_slot = parent < 0 ? -1 : scene_slot(scene, parent);
    if(parent >= 0 && (parent_slot < 0 || scene->mobjects[parent_slot].removed ||
        scene->mobjects[parent_slot].type != MOBJECT_GROUP)) {return(0);}
    if(!timeline_reserve_action(&scene->timeline)) {return(0);}
    action.type = ACTION_PARENT;
    action.node = child;
    action.parent = parent;
    scene->timeline.actions[scene->timeline.action_count++] = action;
    return(1);
}

int scene_schedule_layer(Scene *scene, int node, int layer)
{
    SceneAction action = {0};
    int slot;

    if(scene == NULL || (slot = scene_slot(scene, node)) < 0 || scene->mobjects[slot].removed ||
        !timeline_reserve_action(&scene->timeline)) {return(0);}
    action.type = ACTION_LAYER;
    action.node = node;
    action.layer = layer;
    scene->timeline.actions[scene->timeline.action_count++] = action;
    return(1);
}

int scene_schedule_binding(Scene *scene, int node, SceneBindingType binding, int enabled)
{
    SceneAction action = {0};
    int slot, found = 0;

    if(scene == NULL || (slot = scene_slot(scene, node)) < 0 || scene->mobjects[slot].removed ||
        binding < SCENE_BIND_POSITION || binding > SCENE_BIND_OPACITY || (enabled != 0 && enabled != 1)) {return(0);}
    for(int i = 0; i < scene->updater_count; i++) {
        if(scene->updaters[i].node == node && (int)scene->updaters[i].type == (int)binding) {found = 1; break;}
    }
    if(!found || !timeline_reserve_action(&scene->timeline)) {return(0);}
    action.type = ACTION_BINDING;
    action.node = node;
    action.binding = binding;
    action.enabled = enabled;
    scene->timeline.actions[scene->timeline.action_count++] = action;
    return(1);
}

static int timeline_apply_action(SceneAction *action, Scene *scene, double elapsed)
{
    for(int pass = 0; pass < 2; pass++) {
        for(int i = 0; i < action->animation_count; i++) {
            Animation *animation = &action->animations[i];
            double alpha;
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
            alpha = animation_duration(animation) > 0 ?
                (elapsed - animation->offset) / animation_duration(animation) : 1;
            if(elapsed >= animation_end(animation)) {alpha = 1;}
            if(scene->mobjects[slot].active) {
                animation_interpolate(animation, &action->starting_mobjects[i],
                    &scene->mobjects[slot], scene, &action->caches[i], alpha);
            }
            if(alpha >= 1.0f) {action->animation_started[i] = 2;}
        }
        if(!scene_apply_updaters(scene)) {return(0);}
    }
    return(1);
}

static int timeline_apply_edits(Timeline *timeline, Scene *scene)
{
    while(timeline->edit_cursor < timeline->edit_count) {
        const TimelineEdit *edit = &timeline->edits[timeline->edit_cursor];
        int slot;

        if(edit->action > timeline->current_action ||
            (edit->action == timeline->current_action && edit->action_elapsed > timeline->action_elapsed) ||
            (edit->action == timeline->current_action && edit->action_elapsed == timeline->action_elapsed &&
             edit->started && !timeline->action_started)) {break;}
        slot = scene_slot(scene, edit->node);
        if(slot >= 0 && !scene->mobjects[slot].removed) {
            if(edit->type == EDIT_PARENT && scene_parent_valid(scene, edit->node, edit->integer)) {
                scene->mobjects[slot].parent = edit->integer;
            }
            else if(edit->type == EDIT_LAYER) {scene->mobjects[slot].layer = edit->integer;}
            else if(edit->type == EDIT_TRACKER && scene->mobjects[slot].type == MOBJECT_TRACKER) {
                scene->mobjects[slot].tracker_value = edit->value;
            }
            else if(edit->type == EDIT_BINDING) {
                unsigned int mask = 1u << edit->integer;

                if(edit->value != 0) {scene->mobjects[slot].bindings_active |= mask;}
                else {scene->mobjects[slot].bindings_active &= ~mask;}
            }
        }
        timeline->edit_cursor++;
        if(!scene_apply_updaters(scene)) {return(0);}
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
    if(!scene_apply_updaters(scene) || !timeline_apply_edits(timeline, scene)) {return(0);}

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
            if(action->type == ACTION_PARENT && scene_parent_valid(scene, action->node, action->parent)) {
                scene->mobjects[scene_slot(scene, action->node)].parent = action->parent;
            }
            if(action->type == ACTION_LAYER) {
                int slot = scene_slot(scene, action->node);

                if(slot >= 0 && !scene->mobjects[slot].removed) {scene->mobjects[slot].layer = action->layer;}
            }
            if(action->type == ACTION_BINDING) {
                int slot = scene_slot(scene, action->node);

                if(slot >= 0 && !scene->mobjects[slot].removed) {
                    unsigned int mask = 1u << action->binding;

                    if(action->enabled) {scene->mobjects[slot].bindings_active |= mask;}
                    else {scene->mobjects[slot].bindings_active &= ~mask;}
                }
            }
            for(int i = 0; i < action->animation_count; i++) {action->animation_started[i] = 0;}
            timeline->action_started = 1;
            if(!timeline_apply_action(action, scene, 0.0f) || !timeline_apply_edits(timeline, scene)) {return(0);}
            if(dt == 0 && timeline_action_duration(action) > 0) {break;}
        }
        remaining = timeline_action_duration(action) - timeline->action_elapsed;
        advance = dt < remaining ? dt : remaining;
        for(int i = 0; i < action->animation_count; i++) {
            float offset = action->animations[i].offset;

            if(!action->animation_started[i] && offset > timeline->action_elapsed &&
                offset - timeline->action_elapsed < advance) {
                advance = offset - timeline->action_elapsed;
            }
        }
        if(timeline->edit_cursor < timeline->edit_count) {
            const TimelineEdit *edit = &timeline->edits[timeline->edit_cursor];

            if(edit->action == timeline->current_action && edit->action_elapsed > timeline->action_elapsed &&
                edit->action_elapsed - timeline->action_elapsed < advance) {
                advance = edit->action_elapsed - timeline->action_elapsed;
            }
        }
        timeline->action_elapsed += advance;
        timeline->elapsed += advance;
        dt -= advance;
        if(!timeline_apply_action(action, scene, timeline->action_elapsed) ||
            !timeline_apply_edits(timeline, scene)) {return(0);}
        if(timeline->action_elapsed < timeline_action_duration(action)) {
            if(dt > 0.0f && advance > 0.0f) {continue;}
            break;
        }
        timeline->current_action++;
        timeline->action_elapsed = 0.0f;
        timeline->action_started = 0;
        timeline_checkpoint(timeline, scene);
        if(!timeline_apply_edits(timeline, scene)) {return(0);}
    }
    return(1);
}


static void timeline_restore_snapshot(Scene *scene, const Mobject *snapshot)
{
    for(int i = 0; i < scene->mobject_count; i++) {
        int removed = scene->mobjects[i].removed;

        scene->mobjects[i] = snapshot[i];
        scene->mobjects[i].removed = removed;
        if(removed) {scene->mobjects[i].active = 0;}
    }
    for(int i = 0; i < scene->mobject_count; i++) {
        int handle = (int)((scene->mobjects[i].generation << MANIC_SLOT_BITS) | (unsigned int)i);
        int parent = scene->mobjects[i].parent;

        if(!scene->mobjects[i].removed && parent >= 0 && !scene_parent_valid(scene, handle, parent)) {
            scene->mobjects[i].parent = -1;
        }
    }
}

int scene_rewind(Scene *scene)
{
    Timeline *timeline;

    if(scene == NULL) {return(0);}
    timeline = &scene->timeline;
    if(!timeline->initialized) {return(1);}
    if(timeline->initial_count != scene->mobject_count) {return(0);}
    timeline_restore_snapshot(scene, timeline->initial_mobjects);
    timeline->current_action = 0;
    timeline->edit_cursor = 0;
    timeline->elapsed = 0;
    timeline->action_elapsed = 0;
    timeline->action_started = 0;
    return(1);
}

int timeline_seek(Timeline *timeline, Scene *scene, double time)
{
    int low = 0, high;
    const TimelineCheckpoint *checkpoint;
    const Mobject *snapshot;

    if(timeline == NULL || scene == NULL || !isfinite(time) || time < 0.0f || !timeline->initialized ||
        timeline->initial_count != scene->mobject_count) {return(0);}
    high = timeline->checkpoint_count;
    while(low < high) {
        int mid = low + (high - low) / 2;

        if(timeline->checkpoints[mid].time <= time) {low = mid + 1;}
        else {high = mid;}
    }
    checkpoint = low ? &timeline->checkpoints[low - 1] : NULL;
    snapshot = checkpoint ? checkpoint->mobjects : timeline->initial_mobjects;
    timeline_restore_snapshot(scene, snapshot);
    timeline->current_action = checkpoint ? checkpoint->action : 0;
    timeline->edit_cursor = checkpoint ? checkpoint->edit_cursor : 0;
    timeline->elapsed = checkpoint ? checkpoint->time : 0;
    timeline->action_elapsed = 0;
    timeline->action_started = 0;
    return(timeline_update(timeline, scene, time - timeline->elapsed));
}

int scene_play_many(Scene *scene, const Animation *animations, int count)
{
    return(timeline_play_many(&scene->timeline, scene, animations, count));
}

int scene_play_staggered(Scene *scene, const Animation *animations, int count, float lag_ratio)
{
    Animation *items;
    double cursor = 0;
    int result;

    if(scene == NULL || animations == NULL || count <= 0 || !isfinite(lag_ratio) || lag_ratio < 0 ||
        (size_t)count > SIZE_MAX / sizeof(*items)) {return(0);}
    items = malloc((size_t)count * sizeof(*items));
    if(items == NULL) {return(0);}
    for(int i = 0; i < count; i++) {
        double offset = cursor + animations[i].offset;

        if(!isfinite(offset) || offset < 0 || offset > FLT_MAX ||
            !isfinite(animation_duration(&animations[i])) || animation_duration(&animations[i]) < 0) {free(items); return(0);}
        items[i] = animations[i];
        items[i].offset = (float)offset;
        cursor += animation_duration(&animations[i]) * lag_ratio;
    }
    result = scene_play_many(scene, items, count);
    free(items);
    return(result);
}

int scene_play_sequence(Scene *scene, const Animation *animations, int count)
{
    return(scene_play_staggered(scene, animations, count, 1));
}

int scene_play(Scene *scene, Animation animation)
{
    return(scene_play_many(scene, &animation, 1));
}

int scene_wait(Scene *scene, float duration)
{
    return(timeline_wait(&scene->timeline, duration));
}

int scene_wait_precise(Scene *scene, double duration)
{
    if(scene == NULL) {return(0);}
    return(timeline_wait_precise(&scene->timeline, duration));
}

int scene_update(Scene *scene, double dt)
{
    return(timeline_update(&scene->timeline, scene, dt));
}
