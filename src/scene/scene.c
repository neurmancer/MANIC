#include "internal.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <float.h>

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

static int scene_structure_editable(const Scene *scene)
{
    for(int i = 0; i < scene->timeline.current_action; i++) {
        ActionType type = scene->timeline.actions[i].type;

        if(type == ACTION_PARENT || type == ACTION_LAYER) {return(0);}
    }
    return(1);
}

int scene_set_parent(Scene *scene, int child, int parent)
{
    int slot;

    if(!scene_parent_valid(scene, child, parent) || !scene_structure_editable(scene)) {return(0);}
    slot = scene_slot(scene, child);
    if(scene->timeline.initialized) {
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

    if(scene == NULL || !scene_structure_editable(scene) || (parent_slot = scene_slot(scene, parent)) < 0 ||
        scene->mobjects[parent_slot].removed || scene->mobjects[parent_slot].type != MOBJECT_GROUP) {return(-1);}
    child = scene_add(scene, object);
    if(child < 0) {return(-1);}
    if(!scene_set_parent(scene, child, parent)) {scene_remove(scene, child); return(-1);}
    return(child);
}

int scene_group(Scene *scene, ManicVec2 position, const int *children, int count)
{
    int group;

    if(scene == NULL || !vector_valid(position) || count < 0 || (count && children == NULL) ||
        (count && !scene_structure_editable(scene))) {return(-1);}
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

static int scene_before_playback(const Scene *scene)
{
    return(scene->timeline.current_action == 0 && !scene->timeline.action_started && scene->timeline.elapsed == 0);
}

int scene_tracker_set(Scene *scene, int tracker, double value)
{
    int slot;

    if(scene == NULL || !scene_before_playback(scene) || !isfinite(value) || (slot = scene_slot(scene, tracker)) < 0 ||
        scene->mobjects[slot].removed || scene->mobjects[slot].type != MOBJECT_TRACKER) {return(0);}
    scene->mobjects[slot].tracker_value = value;
    if(scene->timeline.initialized) {
        scene->timeline.initial_mobjects[slot].tracker_value = value;
        timeline_invalidate_checkpoints(&scene->timeline);
    }
    return(1);
}

int scene_bind_position(Scene *scene, int node, int tracker, ScenePositionUpdater function, const void *context)
{
    int target, source;
    SceneUpdater *items;
    int capacity;

    if(scene == NULL || !scene_before_playback(scene) || function == NULL || (target = scene_slot(scene, node)) < 0 ||
        (source = scene_slot(scene, tracker)) < 0 || scene->mobjects[target].removed ||
        scene->mobjects[source].removed || scene->mobjects[target].type == MOBJECT_TRACKER ||
        scene->mobjects[source].type != MOBJECT_TRACKER) {return(0);}
    for(int i = 0; i < scene->updater_count; i++) {
        if(scene->updaters[i].node == node) {return(0);}
    }
    for(int i = 0; i < scene->timeline.action_count; i++) {
        const SceneAction *action = &scene->timeline.actions[i];

        for(int j = 0; j < action->animation_count; j++) {
            const Animation *animation = &action->animations[j];

            if(animation->mobject_index == node &&
                (animation->type == ANIMATION_MOVE || animation->type == ANIMATION_FOLLOW_CURVE ||
                 animation->type == ANIMATION_FOLLOW_PATH ||
                 (scene->mobjects[target].type == MOBJECT_LINE && animation->type == ANIMATION_SCALE))) {return(0);}
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
    scene->updaters[scene->updater_count++] = (SceneUpdater){node, tracker, function, context,
        (double)scene->mobjects[target].end.x - scene->mobjects[target].position.x,
        (double)scene->mobjects[target].end.y - scene->mobjects[target].position.y};
    timeline_invalidate_checkpoints(&scene->timeline);
    return(1);
}

int scene_unbind_position(Scene *scene, int node)
{
    if(scene == NULL || !scene_before_playback(scene)) {return(0);}
    for(int i = 0; i < scene->updater_count; i++) {
        if(scene->updaters[i].node == node) {
            memmove(&scene->updaters[i], &scene->updaters[i + 1],
                (size_t)(scene->updater_count - i - 1) * sizeof(*scene->updaters));
            scene->updater_count--;
            timeline_invalidate_checkpoints(&scene->timeline);
            return(1);
        }
    }
    return(0);
}

int scene_apply_updaters(Scene *scene)
{
    for(int i = 0; i < scene->updater_count; i++) {
        const SceneUpdater *updater = &scene->updaters[i];
        int node = scene_slot(scene, updater->node);
        int tracker = scene_slot(scene, updater->tracker);
        ManicVec2 position;
        Mobject *object;

        if(node < 0 || tracker < 0 || scene->mobjects[node].removed || scene->mobjects[tracker].removed) {continue;}
        object = &scene->mobjects[node];
        if(!updater->function(scene->mobjects[tracker].tracker_value, scene->timeline.elapsed,
            updater->context, &position) || !vector_valid(position)) {return(0);}
        if(object->type == MOBJECT_LINE) {
            double x = (double)position.x + updater->line_dx;
            double y = (double)position.y + updater->line_dy;

            if(!isfinite(x) || !isfinite(y) || fabs(x) > FLT_MAX || fabs(y) > FLT_MAX) {return(0);}
            object->end = (ManicVec2){(float)x, (float)y};
        }
        object->position = position;
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

    if(slot < 0 || scene->mobjects[slot].removed || !scene_structure_editable(scene)) {return(0);}
    scene->mobjects[slot].layer = layer;
    if(scene->timeline.initialized) {
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
    free(scene->timeline.initial_mobjects);
    for(int i = 0; i < scene->mobject_count; i++) {
        if(scene->mobjects[i].type == MOBJECT_PATH && scene->mobjects[i].path != NULL) {
            path_destroy(scene->mobjects[i].path);
            free(scene->mobjects[i].path);
        }
    }
    free(scene->mobjects);
    free(scene->updaters);
    *scene = (Scene){0};
}
