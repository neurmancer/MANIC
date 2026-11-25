#include "internal.h"
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>

Mobject *scene_get(Scene *scene, int handle)
{
    int slot = scene_slot(scene, handle);

    if(slot < 0 || scene->mobjects[slot].removed) {return(NULL);}
    return(&scene->mobjects[slot]);
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

int scene_set_parent(Scene *scene, int child, int parent)
{
    int child_slot = scene_slot(scene, child);
    int parent_slot = parent < 0 ? -1 : scene_slot(scene, parent);
    int ancestor = parent;

    if(child_slot < 0 || scene->mobjects[child_slot].removed || parent < -1 ||
        (parent >= 0 && (parent_slot < 0 || scene->mobjects[parent_slot].removed ||
            scene->mobjects[parent_slot].type != MOBJECT_GROUP))) {return(0);}
    while(ancestor >= 0) {
        int slot = scene_slot(scene, ancestor);

        if(slot < 0 || ancestor == child) {return(0);}
        ancestor = scene->mobjects[slot].parent;
    }
    scene->mobjects[child_slot].parent = parent;
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
    scene->mobjects[slot].layer = layer;
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
    *scene = (Scene){0};
}
