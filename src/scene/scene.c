#include "internal.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <float.h>

struct SceneSnapshot {
    Mobject object;
    VectorPath *paths[3];
};

Mobject *scene_get(Scene *scene, int handle)
{
    int slot = scene_slot(scene, handle);

    if(slot < 0 || scene->mobjects[slot].removed) {return(NULL);}
    return(&scene->mobjects[slot]);
}

int scene_read(const Scene *scene, int handle, Mobject *output)
{
    int slot;

    if(scene == NULL || output == NULL) {return(0);}
    slot = scene_slot(scene, handle);
    if(slot < 0 || scene->mobjects[slot].removed) {return(0);}
    *output = scene->mobjects[slot];
    return(1);
}

SceneSnapshot *scene_snapshot_create(const Scene *scene, int handle)
{
    SceneSnapshot *snapshot;
    const VectorPath *sources[3];
    const VectorPath *copies[3] = {0};

    if(scene == NULL) {return(NULL);}
    snapshot = calloc(1, sizeof(*snapshot));
    if(snapshot == NULL) {return(NULL);}
    if(!scene_read(scene, handle, &snapshot->object)) {free(snapshot); return(NULL);}
    sources[0] = snapshot->object.path;
    sources[1] = snapshot->object.morph_source;
    sources[2] = snapshot->object.morph_target;
    for(int i = 0; i < 3; i++) {
        if(sources[i] == NULL) {continue;}
        for(int j = 0; j < i; j++) {
            if(sources[j] == sources[i]) {copies[i] = copies[j]; break;}
        }
        if(copies[i] == NULL) {
            snapshot->paths[i] = calloc(1, sizeof(*snapshot->paths[i]));
            if(snapshot->paths[i] == NULL || !path_clone(snapshot->paths[i], sources[i])) {
                scene_snapshot_destroy(snapshot);
                return(NULL);
            }
            copies[i] = snapshot->paths[i];
        }
    }
    snapshot->object.path = (VectorPath *)copies[0];
    snapshot->object.morph_source = copies[1];
    snapshot->object.morph_target = copies[2];
    snapshot->object.morph_cache = NULL;
    return(snapshot);
}

const Mobject *scene_snapshot_get(const SceneSnapshot *snapshot)
{
    return(snapshot ? &snapshot->object : NULL);
}

int scene_snapshot_refresh(SceneSnapshot *snapshot, const Scene *scene, int handle)
{
    SceneSnapshot *replacement;

    if(snapshot == NULL) {return(0);}
    replacement = scene_snapshot_create(scene, handle);
    if(replacement == NULL) {return(0);}
    for(int i = 0; i < 3; i++) {
        if(snapshot->paths[i]) {path_destroy(snapshot->paths[i]); free(snapshot->paths[i]);}
    }
    *snapshot = *replacement;
    free(replacement);
    return(1);
}

void scene_snapshot_destroy(SceneSnapshot *snapshot)
{
    if(snapshot == NULL) {return;}
    for(int i = 0; i < 3; i++) {
        if(snapshot->paths[i]) {path_destroy(snapshot->paths[i]); free(snapshot->paths[i]);}
    }
    free(snapshot);
}

int scene_add(Scene *scene, Mobject mobject)
{
    int slot = -1;
    unsigned int generation = 0;

    if(scene->next_order == ULLONG_MAX) {return(-1);}
    for(int i = 0; i < scene->mobject_count; i++) {
        if(scene->mobjects[i].reusable && scene->mobjects[i].generation < MANIC_MAX_GENERATION) {
            slot = i;
            generation = scene->mobjects[i].generation + 1;
            break;
        }
    }
    if(slot < 0) {
        if((unsigned int)scene->mobject_count > MANIC_SLOT_MASK) {return(-1);}
        if(scene->mobject_count == scene->mobject_capacity) {
            int capacity = scene->mobject_capacity ? scene->mobject_capacity * 2 : 16;
            Mobject *mobjects;

            if(capacity < scene->mobject_capacity || (unsigned int)capacity > MANIC_SLOT_MASK + 1u) {
                capacity = (int)(MANIC_SLOT_MASK + 1u);
            }
            if((size_t)capacity > SIZE_MAX / sizeof(*mobjects)) {return(-1);}
            mobjects = realloc(scene->mobjects, (size_t)capacity * sizeof(*mobjects));
            if(mobjects == NULL) {return(-1);}
            scene->mobjects = mobjects;
            scene->mobject_capacity = capacity;
        }
        slot = scene->mobject_count;
    }
    if(mobject.type == MOBJECT_PATH) {
        VectorPath *copy;

        if(mobject.path == NULL) {return(-1);}
        copy = malloc(sizeof(*copy));
        if(copy == NULL) {return(-1);}
        if(!path_clone(copy, mobject.path)) {free(copy); return(-1);}
        mobject.path = copy;
    }
    if(mobject.type == MOBJECT_LINE && mobject.line_scale == 0) {mobject.line_scale = 1;}
    mobject.parent = -1;
    mobject.active = 1;
    mobject.removed = 0;
    mobject.reusable = 0;
    mobject.generation = generation;
    mobject.order = scene->next_order;
    if(scene->timeline.initialized) {
        if(slot == scene->mobject_count) {
            Mobject *initial;
            size_t count = (size_t)scene->mobject_count + 1;

            if(count > SIZE_MAX / sizeof(*initial) ||
                (initial = realloc(scene->timeline.initial_mobjects, count * sizeof(*initial))) == NULL) {
                if(mobject.type == MOBJECT_PATH) {path_destroy(mobject.path); free(mobject.path);}
                return(-1);
            }
            scene->timeline.initial_mobjects = initial;
            scene->timeline.initial_count = (int)count;
        }
        scene->timeline.initial_mobjects[slot] = mobject;
    }
    timeline_invalidate_checkpoints(&scene->timeline);
    scene->mobjects[slot] = mobject;
    if(slot == scene->mobject_count) {scene->mobject_count++;}
    scene->next_order++;
    return((int)((generation << MANIC_SLOT_BITS) | (unsigned int)slot));
}

int scene_parent_valid(const Scene *scene, int child, int parent)
{
    int child_slot, parent_slot, ancestor = parent;
    int steps = 0;

    if(scene == NULL) {return(0);}
    child_slot = scene_slot(scene, child);
    parent_slot = parent < 0 ? -1 : scene_slot(scene, parent);
    if(child_slot < 0 || scene->mobjects[child_slot].removed || parent < -1 ||
        (parent >= 0 && (parent_slot < 0 || scene->mobjects[parent_slot].removed ||
            scene->mobjects[parent_slot].type != MOBJECT_GROUP))) {return(0);}
    while(ancestor >= 0) {
        int slot = scene_slot(scene, ancestor);

        if(slot < 0 || ancestor == child || steps++ >= scene->mobject_count) {return(0);}
        ancestor = scene->mobjects[slot].parent;
    }
    return(1);
}

static int scene_before_playback(const Scene *scene)
{
    return(scene->timeline.current_action == 0 && !scene->timeline.action_started && scene->timeline.elapsed == 0);
}

int scene_set_parent(Scene *scene, int child, int parent)
{
    int slot;

    if(!scene_parent_valid(scene, child, parent)) {return(0);}
    slot = scene_slot(scene, child);
    if(!scene_before_playback(scene)) {
        if(!timeline_record_edit(&scene->timeline, EDIT_PARENT, child, parent, 0)) {return(0);}
    }
    else if(scene->timeline.initialized) {
        Scene baseline = *scene;

        baseline.mobjects = scene->timeline.initial_mobjects;
        if(!scene_parent_valid(&baseline, child, parent)) {return(0);}
        baseline.mobjects[slot].parent = parent;
        timeline_invalidate_checkpoints(&scene->timeline);
    }
    scene->mobjects[slot].parent = parent;
    return(1);
}

int scene_add_child(Scene *scene, int parent, Mobject object)
{
    int parent_slot, child;

    if(scene == NULL || (parent_slot = scene_slot(scene, parent)) < 0 ||
        scene->mobjects[parent_slot].removed || scene->mobjects[parent_slot].type != MOBJECT_GROUP ||
        (!scene_before_playback(scene) && !timeline_reserve_edits(&scene->timeline, 1))) {return(-1);}
    child = scene_add(scene, object);
    if(child < 0) {return(-1);}
    if(!scene_set_parent(scene, child, parent)) {scene_remove(scene, child); return(-1);}
    return(child);
}

int scene_group(Scene *scene, ManicVec2 position, const int *children, int count)
{
    int group;

    if(scene == NULL || !vector_valid(position) || count < 0 || (count && children == NULL) ||
        (!scene_before_playback(scene) && !timeline_reserve_edits(&scene->timeline, count))) {return(-1);}
    for(int i = 0; i < count; i++) {
        int slot = scene_slot(scene, children[i]);

        if(slot < 0 || scene->mobjects[slot].removed) {return(-1);}
        for(int j = 0; j < i; j++) {if(children[j] == children[i]) {return(-1);}}
    }
    group = scene_add(scene, group_create(position));
    if(group < 0) {return(-1);}
    for(int i = 0; i < count; i++) {
        if(!scene_set_parent(scene, children[i], group)) {return(-1);}
    }
    return(group);
}

int scene_tracker(Scene *scene, double value)
{
    Mobject tracker = {0};

    if(scene == NULL || !isfinite(value)) {return(-1);}
    tracker.type = MOBJECT_TRACKER;
    tracker.tracker_value = value;
    return(scene_add(scene, tracker));
}

int scene_tracker_value(const Scene *scene, int tracker, double *value)
{
    int slot;

    if(scene == NULL || value == NULL || (slot = scene_slot(scene, tracker)) < 0 ||
        scene->mobjects[slot].removed || scene->mobjects[slot].type != MOBJECT_TRACKER) {return(0);}
    *value = scene->mobjects[slot].tracker_value;
    return(1);
}

static Mobject *scene_edit_snapshot(const Scene *scene)
{
    Mobject *snapshot;

    if(scene->mobject_count == 0 || (size_t)scene->mobject_count > SIZE_MAX / sizeof(*snapshot)) {return(NULL);}
    snapshot = malloc((size_t)scene->mobject_count * sizeof(*snapshot));
    if(snapshot) {memcpy(snapshot, scene->mobjects, (size_t)scene->mobject_count * sizeof(*snapshot));}
    return(snapshot);
}

int scene_tracker_set(Scene *scene, int tracker, double value)
{
    int slot, recorded;
    Mobject *snapshot;

    if(scene == NULL || !isfinite(value) || (slot = scene_slot(scene, tracker)) < 0 ||
        scene->mobjects[slot].removed || scene->mobjects[slot].type != MOBJECT_TRACKER) {return(0);}
    snapshot = scene_edit_snapshot(scene);
    if(snapshot == NULL) {return(0);}
    recorded = !scene_before_playback(scene);
    if(recorded && !timeline_record_edit(&scene->timeline, EDIT_TRACKER, tracker, 0, value)) {free(snapshot); return(0);}
    scene->mobjects[slot].tracker_value = value;
    if(!scene_apply_updaters(scene)) {
        memcpy(scene->mobjects, snapshot, (size_t)scene->mobject_count * sizeof(*snapshot));
        if(recorded) {timeline_retract_edit(&scene->timeline);}
        free(snapshot);
        return(0);
    }
    if(!recorded && scene->timeline.initialized) {
        scene->timeline.initial_mobjects[slot].tracker_value = value;
        timeline_invalidate_checkpoints(&scene->timeline);
    }
    free(snapshot);
    return(1);
}

int scene_updater_conflicts(SceneUpdaterType type, AnimationType animation)
{
    if(type == UPDATER_POSITION) {
        return(animation == ANIMATION_MOVE || animation == ANIMATION_FOLLOW_CURVE ||
            animation == ANIMATION_FOLLOW_PATH);
    }
    if(type == UPDATER_ROTATION) {return(animation == ANIMATION_ROTATE);}
    if(type == UPDATER_OPACITY) {return(animation == ANIMATION_FADE);}
    return(1);
}

static int scene_bind_updater(Scene *scene, int node, int tracker, SceneUpdaterType type,
    ScenePositionUpdater position, SceneScalarUpdater scalar, const void *context, size_t context_size, int copy_context)
{
    int target, source;
    SceneUpdater *items;
    void *owned = NULL;
    Mobject *snapshot = NULL;
    int capacity, recorded;

    if(scene == NULL ||
        (type == UPDATER_POSITION ? position == NULL : scalar == NULL) ||
        (copy_context && context_size && context == NULL) ||
        (target = scene_slot(scene, node)) < 0 || (source = scene_slot(scene, tracker)) < 0 ||
        scene->mobjects[target].removed || scene->mobjects[source].removed ||
        scene->mobjects[target].type == MOBJECT_TRACKER || scene->mobjects[source].type != MOBJECT_TRACKER ||
        (type == UPDATER_OPACITY && scene->mobjects[target].type == MOBJECT_GROUP)) {return(0);}
    for(int i = 0; i < scene->updater_count; i++) {
        if(scene->updaters[i].node == node && scene->updaters[i].type == type) {return(0);}
    }
    recorded = !scene_before_playback(scene);
    for(int i = recorded ? scene->timeline.current_action : 0; i < scene->timeline.action_count; i++) {
        const SceneAction *action = &scene->timeline.actions[i];

        for(int j = 0; j < action->animation_count; j++) {
            const Animation *animation = &action->animations[j];

            if(animation->mobject_index == node && scene_updater_conflicts(type, animation->type)) {return(0);}
        }
    }
    if(scene->updater_count == scene->updater_capacity) {
        if(scene->updater_capacity > INT_MAX / 2) {return(0);}
        capacity = scene->updater_capacity ? scene->updater_capacity * 2 : 8;
        if((size_t)capacity > SIZE_MAX / sizeof(*items)) {return(0);}
        items = realloc(scene->updaters, (size_t)capacity * sizeof(*items));
        if(items == NULL) {return(0);}
        scene->updaters = items;
        scene->updater_capacity = capacity;
    }
    if(copy_context && context_size) {
        owned = malloc(context_size);
        if(owned == NULL) {return(0);}
        memcpy(owned, context, context_size);
    }
    if(recorded) {
        snapshot = scene_edit_snapshot(scene);
        if(snapshot == NULL || !timeline_record_edit(&scene->timeline, EDIT_BINDING, node, type, 1)) {
            free(snapshot);
            free(owned);
            return(0);
        }
    }
    scene->mobjects[target].bindings_active |= 1u << type;
    if(!recorded && scene->timeline.initialized) {scene->timeline.initial_mobjects[target].bindings_active |= 1u << type;}
    scene->updaters[scene->updater_count++] = (SceneUpdater){
        .node = node, .tracker = tracker, .type = type, .position = position, .scalar = scalar,
        .context = copy_context ? owned : context, .owned_context = owned,
        .line_dx = (double)scene->mobjects[target].end.x - scene->mobjects[target].position.x,
        .line_dy = (double)scene->mobjects[target].end.y - scene->mobjects[target].position.y,
        .line_scale = scene->mobjects[target].line_scale
    };
    if(recorded && !scene_apply_updaters(scene)) {
        memcpy(scene->mobjects, snapshot, (size_t)scene->mobject_count * sizeof(*snapshot));
        scene->updater_count--;
        free(owned);
        timeline_retract_edit(&scene->timeline);
        free(snapshot);
        return(0);
    }
    free(snapshot);
    timeline_invalidate_checkpoints(&scene->timeline);
    return(1);
}

static int scene_unbind_updater(Scene *scene, int node, SceneUpdaterType type)
{
    if(scene == NULL) {return(0);}
    if(!scene_before_playback(scene)) {return(scene_set_binding(scene, node, (SceneBindingType)type, 0));}
    for(int i = 0; i < scene->updater_count; i++) {
        if(scene->updaters[i].node == node && scene->updaters[i].type == type) {
            int slot = scene_slot(scene, node);

            if(timeline_binding_has_edits(&scene->timeline, node, type)) {return(0);}
            for(int j = 0; j < scene->timeline.action_count; j++) {
                SceneAction *action = &scene->timeline.actions[j];

                if(action->type == ACTION_BINDING && action->node == node && action->binding == (int)type) {return(0);}
            }
            if(slot >= 0) {
                scene->mobjects[slot].bindings_active &= ~(1u << type);
                if(scene->timeline.initialized) {scene->timeline.initial_mobjects[slot].bindings_active &= ~(1u << type);}
            }
            free(scene->updaters[i].owned_context);
            memmove(&scene->updaters[i], &scene->updaters[i + 1],
                (size_t)(scene->updater_count - i - 1) * sizeof(*scene->updaters));
            scene->updater_count--;
            timeline_invalidate_checkpoints(&scene->timeline);
            return(1);
        }
    }
    return(0);
}

int scene_bind_position(Scene *scene, int node, int tracker, ScenePositionUpdater function, const void *context)
{
    return(scene_bind_updater(scene, node, tracker, UPDATER_POSITION, function, NULL, context, 0, 0));
}

int scene_bind_position_copy(Scene *scene, int node, int tracker, ScenePositionUpdater function,
    const void *context, size_t size)
{
    return(scene_bind_updater(scene, node, tracker, UPDATER_POSITION, function, NULL, context, size, 1));
}

int scene_unbind_position(Scene *scene, int node)
{
    return(scene_unbind_updater(scene, node, UPDATER_POSITION));
}

int scene_bind_rotation(Scene *scene, int node, int tracker, SceneScalarUpdater function, const void *context)
{
    return(scene_bind_updater(scene, node, tracker, UPDATER_ROTATION, NULL, function, context, 0, 0));
}

int scene_bind_rotation_copy(Scene *scene, int node, int tracker, SceneScalarUpdater function,
    const void *context, size_t size)
{
    return(scene_bind_updater(scene, node, tracker, UPDATER_ROTATION, NULL, function, context, size, 1));
}

int scene_unbind_rotation(Scene *scene, int node)
{
    return(scene_unbind_updater(scene, node, UPDATER_ROTATION));
}

int scene_bind_opacity(Scene *scene, int node, int tracker, SceneScalarUpdater function, const void *context)
{
    return(scene_bind_updater(scene, node, tracker, UPDATER_OPACITY, NULL, function, context, 0, 0));
}

int scene_bind_opacity_copy(Scene *scene, int node, int tracker, SceneScalarUpdater function,
    const void *context, size_t size)
{
    return(scene_bind_updater(scene, node, tracker, UPDATER_OPACITY, NULL, function, context, size, 1));
}

int scene_unbind_opacity(Scene *scene, int node)
{
    return(scene_unbind_updater(scene, node, UPDATER_OPACITY));
}

int scene_set_binding(Scene *scene, int node, SceneBindingType binding, int enabled)
{
    int slot, found = 0, recorded;
    unsigned int mask;
    Mobject *snapshot;

    if(scene == NULL || binding < SCENE_BIND_POSITION || binding > SCENE_BIND_OPACITY ||
        (enabled != 0 && enabled != 1) || (slot = scene_slot(scene, node)) < 0 ||
        scene->mobjects[slot].removed) {return(0);}
    for(int i = 0; i < scene->updater_count; i++) {
        if(scene->updaters[i].node == node && (int)scene->updaters[i].type == (int)binding) {found = 1; break;}
    }
    if(!found || (snapshot = scene_edit_snapshot(scene)) == NULL) {return(0);}
    recorded = !scene_before_playback(scene);
    if(recorded && !timeline_record_edit(&scene->timeline, EDIT_BINDING, node, binding, enabled)) {
        free(snapshot);
        return(0);
    }
    mask = 1u << binding;
    if(enabled) {scene->mobjects[slot].bindings_active |= mask;}
    else {scene->mobjects[slot].bindings_active &= ~mask;}
    if(!scene_apply_updaters(scene)) {
        memcpy(scene->mobjects, snapshot, (size_t)scene->mobject_count * sizeof(*snapshot));
        if(recorded) {timeline_retract_edit(&scene->timeline);}
        free(snapshot);
        return(0);
    }
    if(!recorded && scene->timeline.initialized) {
        scene->timeline.initial_mobjects[slot].bindings_active = scene->mobjects[slot].bindings_active;
        timeline_invalidate_checkpoints(&scene->timeline);
    }
    free(snapshot);
    return(1);
}

int scene_apply_updaters(Scene *scene)
{
    for(int i = 0; i < scene->updater_count; i++) {
        const SceneUpdater *updater = &scene->updaters[i];
        int node = scene_slot(scene, updater->node);
        int tracker = scene_slot(scene, updater->tracker);
        Mobject *object;
        double value = NAN;

        if(node < 0 || tracker < 0 || scene->mobjects[node].removed || scene->mobjects[tracker].removed) {continue;}
        object = &scene->mobjects[node];
        if(!(object->bindings_active & (1u << updater->type))) {continue;}
        if(updater->type == UPDATER_POSITION) {
            ManicVec2 position = {NAN, NAN};

            if(!updater->position(scene->mobjects[tracker].tracker_value, scene->timeline.elapsed,
                updater->context, &position) || !vector_valid(position)) {return(0);}
            if(object->type == MOBJECT_LINE) {
                double scale = updater->line_scale ? object->line_scale / updater->line_scale : 0;
                double x = (double)position.x + updater->line_dx * scale;
                double y = (double)position.y + updater->line_dy * scale;

                if(!isfinite(x) || !isfinite(y) || fabs(x) > FLT_MAX || fabs(y) > FLT_MAX) {return(0);}
                object->end = (ManicVec2){(float)x, (float)y};
            }
            object->position = position;
        }
        else {
            if(!updater->scalar(scene->mobjects[tracker].tracker_value, scene->timeline.elapsed,
                updater->context, &value) || !isfinite(value)) {return(0);}
            if(updater->type == UPDATER_ROTATION) {
                if(fabs(value) > FLT_MAX) {return(0);}
                if(object->type == MOBJECT_GROUP) {object->group_rotation = (float)value;}
                else {object->rotation = (float)value;}
            }
            else if(updater->type == UPDATER_OPACITY) {
                unsigned char alpha;

                if(value < 0 || value > 1) {return(0);}
                alpha = (unsigned char)(value * 255 + 0.5);
                object->color.a = alpha;
                if(object->type == MOBJECT_PATH) {
                    object->fill_color.a = (unsigned char)(((unsigned int)alpha * object->path_fill_alpha + 127) / 255);
                }
            }
            else {return(0);}
        }
    }
    return(1);
}

int scene_remove(Scene *scene, int node)
{
    int slot = scene_slot(scene, node);

    if(slot < 0 || scene->mobjects[slot].removed) {return(0);}
    timeline_invalidate_checkpoints(&scene->timeline);
    for(int i = 0; i < scene->mobject_count; i++) {
        int ancestor = (int)((scene->mobjects[i].generation << MANIC_SLOT_BITS) | (unsigned int)i);

        while(ancestor >= 0 && ancestor != node) {
            int parent_slot = scene_slot(scene, ancestor);

            if(parent_slot < 0) {break;}
            ancestor = scene->mobjects[parent_slot].parent;
        }
        if(ancestor == node) {scene->mobjects[i].active = 0; scene->mobjects[i].removed = 1;}
    }
    return(1);
}

int scene_set_layer(Scene *scene, int node, int layer)
{
    int slot = scene_slot(scene, node);

    if(slot < 0 || scene->mobjects[slot].removed) {return(0);}
    if(!scene_before_playback(scene)) {
        if(!timeline_record_edit(&scene->timeline, EDIT_LAYER, node, layer, 0)) {return(0);}
    }
    scene->mobjects[slot].layer = layer;
    if(scene_before_playback(scene) && scene->timeline.initialized) {
        scene->timeline.initial_mobjects[slot].layer = layer;
        timeline_invalidate_checkpoints(&scene->timeline);
    }
    return(1);
}

int scene_collect_removed(Scene *scene)
{
    int collected = 0;

    for(int node = 0; node < scene->mobject_count; node++) {
        VectorPath *path = scene->mobjects[node].path;
        int referenced = 0;

        if(!scene->mobjects[node].removed || scene->mobjects[node].reusable ||
            scene->mobjects[node].generation >= MANIC_MAX_GENERATION) {continue;}
        for(int i = 0; i < scene->mobject_count && !referenced; i++) {
            if(path != NULL && !scene->mobjects[i].removed && (scene->mobjects[i].morph_source == path ||
                scene->mobjects[i].morph_target == path)) {referenced = 1;}
            if(path != NULL && scene->timeline.initialized && !scene->mobjects[i].removed &&
                (scene->timeline.initial_mobjects[i].morph_source == path ||
                 scene->timeline.initial_mobjects[i].morph_target == path)) {referenced = 1;}
        }
        for(int i = 0; i < scene->timeline.action_count && !referenced; i++) {
            SceneAction *action = &scene->timeline.actions[i];

            for(int j = 0; j < action->animation_count; j++) {
                Animation *animation = &action->animations[j];

                int source = scene_slot(scene, animation->mobject_index);
                int target = scene_slot(scene, animation->target_mobject_index);

                if(path != NULL && animation->type == ANIMATION_MORPH && target == node &&
                    source >= 0 && !scene->mobjects[source].removed) {referenced = 1; break;}
                if(path != NULL && action->animation_started[j] && source >= 0 && !scene->mobjects[source].removed &&
                    (action->starting_mobjects[j].morph_source == path ||
                     action->starting_mobjects[j].morph_target == path)) {referenced = 1; break;}
            }
        }
        if(referenced) {continue;}
        timeline_invalidate_checkpoints(&scene->timeline);
        if(path != NULL) {path_destroy(path); free(path);}
        scene->mobjects[node].path = NULL;
        scene->mobjects[node].reusable = 1;
        if(scene->timeline.initialized) {
            scene->timeline.initial_mobjects[node].path = NULL;
            scene->timeline.initial_mobjects[node].reusable = 1;
        }
        collected++;
    }
    return(collected);
}

void scene_destroy(Scene *scene)
{
    for(int i = 0; i < scene->timeline.action_count; i++) {
        for(int j = 0; j < scene->timeline.actions[i].animation_count; j++) {
            AnimationCache *cache = &scene->timeline.actions[i].caches[j];
            MorphCache *item = cache->morphs;
            free(cache->arc_lengths);
            path_arc_destroy(cache->path_arc);
            while(item) {
                MorphCache *next = item->next;

                path_destroy(&item->left);
                path_destroy(&item->right);
                free(item);
                item = next;
            }
        }
        free(scene->timeline.actions[i].caches);
        free(scene->timeline.actions[i].animations);
        free(scene->timeline.actions[i].starting_mobjects);
        free(scene->timeline.actions[i].animation_started);
    }
    timeline_invalidate_checkpoints(&scene->timeline);
    free(scene->timeline.actions);
    free(scene->timeline.edits);
    free(scene->timeline.initial_mobjects);
    for(int i = 0; i < scene->mobject_count; i++) {
        if(scene->mobjects[i].type == MOBJECT_PATH && scene->mobjects[i].path != NULL) {
            path_destroy(scene->mobjects[i].path);
            free(scene->mobjects[i].path);
        }
    }
    free(scene->mobjects);
    for(int i = 0; i < scene->updater_count; i++) {free(scene->updaters[i].owned_context);}
    free(scene->updaters);
    *scene = (Scene){0};
}
