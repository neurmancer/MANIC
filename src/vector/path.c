#include "internal.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int vector_valid(ManicVec2 point)
{
    return(isfinite(point.x) && isfinite(point.y));
}

ManicVec2 vector_lerp(ManicVec2 a, ManicVec2 b, double t)
{
    ManicVec2 point = {
        (float)((double)a.x + ((double)b.x - a.x) * t),
        (float)((double)a.y + ((double)b.y - a.y) * t)
    };

    return(point);
}

int bezier_evaluate(const Bezier *curve, double t, ManicVec2 *point)
{
    ManicVec2 work[4];

    if(curve == NULL || point == NULL || curve->degree < 1 || curve->degree > 3 ||
        !isfinite(t) || t < 0.0 || t > 1.0) {return(0);}
    for(int i = 0; i <= curve->degree; i++) {
        if(!vector_valid(curve->points[i])) {return(0);}
        work[i] = curve->points[i];
    }
    for(int level = curve->degree; level > 0; level--) {
        for(int i = 0; i < level; i++) {work[i] = vector_lerp(work[i], work[i + 1], t);}
    }
    *point = work[0];
    return(1);
}

int bezier_subdivide(const Bezier *curve, double t, Bezier *left, Bezier *right)
{
    ManicVec2 work[4];
    Bezier a = {0};
    Bezier b = {0};
    int degree;

    if(curve == NULL || left == NULL || right == NULL || left == right ||
        curve->degree < 1 || curve->degree > 3 || !isfinite(t) || t < 0.0 || t > 1.0) {
        return(0);
    }
    degree = curve->degree;
    a.degree = degree;
    b.degree = degree;
    for(int i = 0; i <= degree; i++) {
        if(!vector_valid(curve->points[i])) {return(0);}
        work[i] = curve->points[i];
    }
    a.points[0] = work[0];
    b.points[degree] = work[degree];
    for(int level = 1; level <= degree; level++) {
        for(int i = 0; i <= degree - level; i++) {work[i] = vector_lerp(work[i], work[i + 1], t);}
        a.points[level] = work[0];
        b.points[degree - level] = work[degree - level];
    }
    *left = a;
    *right = b;
    return(1);
}

static void bounds_include(PathBounds *bounds, ManicVec2 point)
{
    if(point.x < bounds->min.x) {bounds->min.x = point.x;}
    if(point.y < bounds->min.y) {bounds->min.y = point.y;}
    if(point.x > bounds->max.x) {bounds->max.x = point.x;}
    if(point.y > bounds->max.y) {bounds->max.y = point.y;}
}

static void bounds_at_root(const Bezier *curve, PathBounds *bounds, double t)
{
    ManicVec2 point;

    if(t > 0.0 && t < 1.0 && bezier_evaluate(curve, t, &point)) {bounds_include(bounds, point);}
}

static void bounds_axis(const Bezier *curve, PathBounds *bounds, int axis)
{
    double p[4] = {0};
    double a;
    double b;
    double c;
    double discriminant;

    for(int i = 0; i <= curve->degree; i++) {p[i] = axis ? curve->points[i].y : curve->points[i].x;}
    if(curve->degree == 2) {
        b = p[0] - 2.0 * p[1] + p[2];
        if(b != 0.0) {bounds_at_root(curve, bounds, (p[0] - p[1]) / b);}
    }
    else if(curve->degree == 3) {
        a = -p[0] + 3.0 * p[1] - 3.0 * p[2] + p[3];
        b = 2.0 * (p[0] - 2.0 * p[1] + p[2]);
        c = p[1] - p[0];
        if(a == 0.0) {
            if(b != 0.0) {bounds_at_root(curve, bounds, -c / b);}
        }
        else {
            discriminant = b * b - 4.0 * a * c;
            if(discriminant >= 0.0) {
                double root = sqrt(discriminant);
                double q = -0.5 * (b + copysign(root, b));

                if(q != 0.0) {
                    bounds_at_root(curve, bounds, q / a);
                    bounds_at_root(curve, bounds, c / q);
                }
                else {bounds_at_root(curve, bounds, -b / (2.0 * a));}
            }
        }
    }
}

int bezier_bounds(const Bezier *curve, PathBounds *bounds)
{
    if(curve == NULL || bounds == NULL || curve->degree < 1 || curve->degree > 3) {return(0);}
    for(int i = 0; i <= curve->degree; i++) {
        if(!vector_valid(curve->points[i])) {return(0);}
    }
    bounds->min = curve->points[0];
    bounds->max = curve->points[0];
    bounds_include(bounds, curve->points[curve->degree]);
    bounds_axis(curve, bounds, 0);
    bounds_axis(curve, bounds, 1);
    return(1);
}

int path_move_to(VectorPath *path, ManicVec2 point)
{
    if(path == NULL || !vector_valid(point)) {return(0);}
    path->current = point;
    path->subpath_start = point;
    path->has_current = 1;
    path->new_subpath = 1;
    path->closed = 0;
    return(1);
}

static int path_append(VectorPath *path, int degree, const ManicVec2 *points)
{
    Bezier segment = {0};

    if(path == NULL || !path->has_current || path->closed) {return(0);}
    for(int i = 0; i < degree; i++) {
        if(!vector_valid(points[i])) {return(0);}
    }
    if(path->count == path->capacity) {
        size_t capacity = path->capacity ? path->capacity * 2 : 8;
        Bezier *segments;

        if(capacity < path->capacity || capacity > SIZE_MAX / sizeof(*segments)) {return(0);}
        segments = realloc(path->segments, capacity * sizeof(*segments));
        if(segments == NULL) {return(0);}
        path->segments = segments;
        path->capacity = capacity;
    }
    segment.degree = degree;
    segment.starts_subpath = path->new_subpath;
    path->new_subpath = 0;
    segment.points[0] = path->current;
    for(int i = 0; i < degree; i++) {segment.points[i + 1] = points[i];}
    path->segments[path->count++] = segment;
    path->current = points[degree - 1];
    return(1);
}

int path_line_to(VectorPath *path, ManicVec2 end)
{
    return(path_append(path, 1, &end));
}

int path_quadratic_to(VectorPath *path, ManicVec2 control, ManicVec2 end)
{
    ManicVec2 points[] = {control, end};

    return(path_append(path, 2, points));
}

int path_cubic_to(VectorPath *path, ManicVec2 control1, ManicVec2 control2, ManicVec2 end)
{
    ManicVec2 points[] = {control1, control2, end};

    return(path_append(path, 3, points));
}

int path_close(VectorPath *path)
{
    if(path == NULL || !path->has_current || path->new_subpath || path->closed) {return(0);}
    if(!path_line_to(path, path->subpath_start)) {return(0);}
    path->segments[path->count - 1].closes_subpath = 1;
    path->closed = 1;
    return(1);
}

int path_clone(VectorPath *destination, const VectorPath *source)
{
    VectorPath copy = *source;

    if(source->count > SIZE_MAX / sizeof(*copy.segments)) {return(0);}
    copy.segments = NULL;
    copy.capacity = source->count;
    if(source->count) {
        copy.segments = malloc(source->count * sizeof(*copy.segments));
        if(copy.segments == NULL) {return(0);}
        memcpy(copy.segments, source->segments, source->count * sizeof(*copy.segments));
    }
    *destination = copy;
    return(1);
}

int path_compatible(const VectorPath *a, const VectorPath *b)
{
    if(a == NULL || b == NULL || a->count != b->count) {return(0);}
    for(size_t i = 0; i < a->count; i++) {
        if(a->segments[i].degree != b->segments[i].degree ||
            a->segments[i].starts_subpath != b->segments[i].starts_subpath ||
            a->segments[i].closes_subpath != b->segments[i].closes_subpath) {return(0);}
    }
    return(1);
}

int path_bounds(const VectorPath *path, PathBounds *bounds)
{
    PathBounds segment_bounds;

    if(path == NULL || bounds == NULL || path->count == 0 ||
        !bezier_bounds(&path->segments[0], bounds)) {return(0);}
    for(size_t i = 1; i < path->count; i++) {
        if(!bezier_bounds(&path->segments[i], &segment_bounds)) {return(0);}
        bounds_include(bounds, segment_bounds.min);
        bounds_include(bounds, segment_bounds.max);
    }
    return(1);
}

void path_destroy(VectorPath *path)
{
    if(path == NULL) {return;}
    free(path->segments);
    *path = (VectorPath){0};
}

int path_rectangle(VectorPath *path, float width, float height)
{
    if(!isfinite(width) || !isfinite(height) || width <= 0.0f || height <= 0.0f) {return(0);}
    return(path_move_to(path, (ManicVec2){-width / 2, -height / 2}) &&
        path_line_to(path, (ManicVec2){width / 2, -height / 2}) &&
        path_line_to(path, (ManicVec2){width / 2, height / 2}) &&
        path_line_to(path, (ManicVec2){-width / 2, height / 2}) && path_close(path));
}

int path_circle(VectorPath *path, float radius)
{
    float handle;

    if(!isfinite(radius) || radius <= 0.0f) {return(0);}
    handle = radius * 0.5522847498f;
    return(path_move_to(path, (ManicVec2){radius, 0}) &&
        path_cubic_to(path, (ManicVec2){radius, handle}, (ManicVec2){handle, radius}, (ManicVec2){0, radius}) &&
        path_cubic_to(path, (ManicVec2){-handle, radius}, (ManicVec2){-radius, handle}, (ManicVec2){-radius, 0}) &&
        path_cubic_to(path, (ManicVec2){-radius, -handle}, (ManicVec2){-handle, -radius}, (ManicVec2){0, -radius}) &&
        path_cubic_to(path, (ManicVec2){handle, -radius}, (ManicVec2){radius, -handle}, (ManicVec2){radius, 0}) &&
        path_close(path));
}

static Bezier bezier_cubic(const Bezier *curve)
{
    Bezier result = *curve;

    if(curve->degree == 1) {
        result.points[1] = vector_lerp(curve->points[0], curve->points[1], 1.0 / 3.0);
        result.points[2] = vector_lerp(curve->points[0], curve->points[1], 2.0 / 3.0);
        result.points[3] = curve->points[1];
    }
    else if(curve->degree == 2) {
        result.points[1] = vector_lerp(curve->points[0], curve->points[1], 2.0 / 3.0);
        result.points[2] = vector_lerp(curve->points[2], curve->points[1], 2.0 / 3.0);
        result.points[3] = curve->points[2];
    }
    result.degree = 3;
    return(result);
}

static int path_subpath_range(const VectorPath *path, size_t start, size_t *end, int *closed)
{
    size_t next;

    if(start >= path->count || !path->segments[start].starts_subpath) {return(0);}
    next = start + 1;
    while(next < path->count && !path->segments[next].starts_subpath) {next++;}
    *end = next;
    *closed = path->segments[next - 1].closes_subpath;
    return(1);
}

typedef struct {
    size_t start;
    size_t end;
    int closed;
    double center_x;
    double center_y;
    double width;
    double height;
} MorphSubpath;

static int path_describe_subpaths(const VectorPath *path, MorphSubpath **result, size_t *count)
{
    size_t total = 0;
    size_t start = 0;
    MorphSubpath *items;

    if(path == NULL || path->count == 0) {return(0);}
    while(start < path->count) {
        size_t end;
        int closed;

        if(!path_subpath_range(path, start, &end, &closed)) {return(0);}
        total++;
        start = end;
    }
    if(total > SIZE_MAX / sizeof(*items)) {return(0);}
    items = malloc(total * sizeof(*items));
    if(items == NULL) {return(0);}
    start = 0;
    for(size_t i = 0; i < total; i++) {
        size_t end;
        int closed;
        PathBounds bounds;
        PathBounds segment;

        if(!path_subpath_range(path, start, &end, &closed) ||
            !bezier_bounds(&path->segments[start], &bounds)) {free(items); return(0);}
        for(size_t j = start + 1; j < end; j++) {
            if(!bezier_bounds(&path->segments[j], &segment)) {free(items); return(0);}
            bounds_include(&bounds, segment.min);
            bounds_include(&bounds, segment.max);
        }
        items[i] = (MorphSubpath){start, end, closed,
            ((double)bounds.min.x + bounds.max.x) / 2.0,
            ((double)bounds.min.y + bounds.max.y) / 2.0,
            (double)bounds.max.x - bounds.min.x, (double)bounds.max.y - bounds.min.y};
        start = end;
    }
    *result = items;
    *count = total;
    return(1);
}

static double morph_subpath_cost(const MorphSubpath *a, const MorphSubpath *b)
{
    if(a->closed != b->closed) {return(1e100);}
    return(hypot(a->center_x - b->center_x, a->center_y - b->center_y) +
        0.5 * hypot(a->width - b->width, a->height - b->height));
}

static int path_match_subpaths(const VectorPath *a, const VectorPath *b,
    MorphSubpath **source, MorphSubpath **target, size_t **matching, size_t *count)
{
    MorphSubpath *left = NULL;
    MorphSubpath *right = NULL;
    size_t n = 0;
    size_t m = 0;
    size_t *assignment = NULL;
    size_t *p = NULL;
    size_t *way = NULL;
    double *u = NULL;
    double *v = NULL;
    double *minimum = NULL;
    unsigned char *used = NULL;
    int success = 0;

    if(!path_describe_subpaths(a, &left, &n) || !path_describe_subpaths(b, &right, &m) || n != m ||
        n == SIZE_MAX || n + 1 > SIZE_MAX / sizeof(double) || n + 1 > SIZE_MAX / sizeof(size_t)) {goto cleanup;}
    assignment = malloc(n * sizeof(*assignment));
    p = calloc(n + 1, sizeof(*p));
    way = calloc(n + 1, sizeof(*way));
    u = calloc(n + 1, sizeof(*u));
    v = calloc(n + 1, sizeof(*v));
    minimum = malloc((n + 1) * sizeof(*minimum));
    used = malloc(n + 1);
    if(assignment == NULL || p == NULL || way == NULL || u == NULL || v == NULL || minimum == NULL || used == NULL) {goto cleanup;}
    for(size_t row = 1; row <= n; row++) {
        size_t column = 0;

        p[0] = row;
        for(size_t j = 0; j <= n; j++) {minimum[j] = 1e100; used[j] = 0;}
        do {
            size_t current = p[column];
            size_t next = 0;
            double delta = 1e100;

            used[column] = 1;
            for(size_t j = 1; j <= n; j++) {
                if(!used[j]) {
                    double cost = morph_subpath_cost(&left[current - 1], &right[j - 1]) - u[current] - v[j];

                    if(cost < minimum[j]) {minimum[j] = cost; way[j] = column;}
                    if(minimum[j] < delta) {delta = minimum[j]; next = j;}
                }
            }
            if(next == 0 || delta >= 1e99) {goto cleanup;}
            for(size_t j = 0; j <= n; j++) {
                if(used[j]) {u[p[j]] += delta; v[j] -= delta;}
                else {minimum[j] -= delta;}
            }
            column = next;
        } while(p[column] != 0);
        do {
            size_t previous = way[column];

            p[column] = p[previous];
            column = previous;
        } while(column != 0);
    }
    for(size_t j = 1; j <= n; j++) {assignment[p[j] - 1] = j - 1;}
    *source = left;
    *target = right;
    *matching = assignment;
    *count = n;
    left = NULL;
    right = NULL;
    assignment = NULL;
    success = 1;

cleanup:
    free(left);
    free(right);
    free(assignment);
    free(p);
    free(way);
    free(u);
    free(v);
    free(minimum);
    free(used);
    return(success);
}

int path_morphable(const VectorPath *a, const VectorPath *b)
{
    MorphSubpath *source = NULL;
    MorphSubpath *target = NULL;
    size_t *matching = NULL;
    size_t count;
    int result = path_match_subpaths(a, b, &source, &target, &matching, &count);

    free(source);
    free(target);
    free(matching);
    return(result);
}

static int bezier_interval(const Bezier *curve, double begin, double end, Bezier *result)
{
    Bezier left;
    Bezier right;
    Bezier cubic = bezier_cubic(curve);

    if(begin < 0.0 && begin > -1e-12) {begin = 0.0;}
    if(end > 1.0 && end < 1.0 + 1e-12) {end = 1.0;}
    if(begin < 0.0 || end > 1.0 || begin >= end) {return(0);}
    if(!bezier_subdivide(&cubic, end, &left, &right)) {return(0);}
    if(begin > 0.0) {
        if(!bezier_subdivide(&left, begin / end, &right, result)) {return(0);}
    }
    else {*result = left;}
    return(1);
}

static int path_interpolate_matched(const VectorPath *a, const VectorPath *b, float alpha,
    VectorPath *output, MorphSubpath *source, MorphSubpath *target, size_t *matching, size_t count)
{
    for(size_t subpath = 0; subpath < count; subpath++) {
        MorphSubpath *left = &source[subpath];
        MorphSubpath *right = &target[matching[subpath]];
        size_t sa = 0;
        size_t sb = 0;
        double t = 0.0;
        size_t na = left->end - left->start;
        size_t nb = right->end - right->start;

        if(!path_move_to(output, vector_lerp(a->segments[left->start].points[0],
            b->segments[right->start].points[0], alpha))) {return(0);}
        while(sa < na && sb < nb) {
            double nexta = (double)(sa + 1) / na;
            double nextb = (double)(sb + 1) / nb;
            double next = nexta < nextb ? nexta : nextb;
            Bezier ca;
            Bezier cb;
            ManicVec2 points[4];

            if(!bezier_interval(&a->segments[left->start + sa], t * na - sa, next * na - sa, &ca) ||
                !bezier_interval(&b->segments[right->start + sb], t * nb - sb, next * nb - sb, &cb)) {return(0);}
            for(int j = 0; j < 4; j++) {points[j] = vector_lerp(ca.points[j], cb.points[j], alpha);}
            if(!path_cubic_to(output, points[1], points[2], points[3])) {return(0);}
            t = next;
            if(nexta <= nextb) {sa++;}
            if(nextb <= nexta) {sb++;}
        }
        if(left->closed) {output->segments[output->count - 1].closes_subpath = 1; output->closed = 1;}
    }
    return(1);
}

int path_interpolate(const VectorPath *a, const VectorPath *b, float alpha, VectorPath *output)
{
    MorphSubpath *source = NULL, *target = NULL;
    size_t *matching = NULL, count = 0;
    int success;

    if(output == NULL || output == a || output == b || !isfinite(alpha) || alpha < 0 || alpha > 1 ||
        !path_match_subpaths(a, b, &source, &target, &matching, &count)) {return(0);}
    success = path_interpolate_matched(a, b, alpha, output, source, target, matching, count);
    free(source);
    free(target);
    free(matching);
    return(success);
}

MorphCache *morph_cache_create(const VectorPath *a, const VectorPath *b)
{
    MorphSubpath *source = NULL, *target = NULL;
    size_t *matching = NULL, count = 0;
    MorphCache *cache = calloc(1, sizeof(*cache));
    int ok = cache && path_match_subpaths(a, b, &source, &target, &matching, &count);

    if(ok) {
        ok = path_interpolate_matched(a, b, 0, &cache->left, source, target, matching, count) &&
            path_interpolate_matched(a, b, 1, &cache->right, source, target, matching, count);
    }
    free(source);
    free(target);
    free(matching);
    if(!ok) {
        if(cache) {
            path_destroy(&cache->left);
            path_destroy(&cache->right);
            free(cache);
        }
        return(NULL);
    }
    cache->source = a;
    cache->target = b;
    return(cache);
}

int path_interpolate_cached(const MorphCache *cache, float alpha, VectorPath *output)
{
    if(!path_clone(output, &cache->left)) {return(0);}
    for(size_t i = 0; i < output->count; i++) {
        for(int j = 0; j <= output->segments[i].degree; j++) {
            output->segments[i].points[j] = vector_lerp(cache->left.segments[i].points[j],
                cache->right.segments[i].points[j], alpha);
        }
    }
    return(1);
}

int bezier_arc_table(const Bezier *curve, double lengths[129])
{
    ManicVec2 previous, point;

    lengths[0] = 0;
    if(!bezier_evaluate(curve, 0, &previous)) {return(0);}
    for(int i = 1; i <= 128; i++) {
        if(!bezier_evaluate(curve, (double)i / 128, &point)) {return(0);}
        lengths[i] = lengths[i - 1] + hypot((double)point.x - previous.x, (double)point.y - previous.y);
        previous = point;
    }
    return(1);
}

double bezier_arc_lookup(const double lengths[129], double fraction)
{
    double wanted;
    int low = 1, high = 128;
    double span;

    if(fraction <= 0) {return(0);}
    if(fraction >= 1) {return(1);}
    if(lengths[128] == 0) {return(fraction);}
    wanted = lengths[128] * fraction;
    while(low < high) {
        int mid = low + (high - low) / 2;

        if(lengths[mid] < wanted) {low = mid + 1;}
        else {high = mid;}
    }
    span = lengths[low] - lengths[low - 1];
    return(((double)(low - 1) + (span > 0 ? (wanted - lengths[low - 1]) / span : 0)) / 128);
}

double bezier_arc_parameter(const Bezier *curve, double fraction)
{
    double lengths[129];

    return(bezier_arc_table(curve, lengths) ? bezier_arc_lookup(lengths, fraction) : fraction);
}
