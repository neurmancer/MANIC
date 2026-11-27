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

static Bezier morph_segment(const VectorPath *path, const MorphSubpath *subpath,
    size_t index, size_t offset, int reverse)
{
    size_t count = subpath->end - subpath->start;
    size_t segment = reverse ? (offset + count - 1 - index) % count : (offset + index) % count;
    Bezier curve = path->segments[subpath->start + segment];

    if(reverse) {
        for(int i = 0; i <= curve.degree / 2; i++) {
            ManicVec2 point = curve.points[i];

            curve.points[i] = curve.points[curve.degree - i];
            curve.points[curve.degree - i] = point;
        }
    }
    return(curve);
}

static double morph_alignment(const VectorPath *a, const MorphSubpath *left,
    const VectorPath *b, const MorphSubpath *right, size_t offset, int reverse)
{
    size_t na = left->end - left->start;
    size_t nb = right->end - right->start;
    double cost = 0;

    for(int i = 0; i < 24; i++) {
        double t = (i + 0.5) / 24.0;
        size_t ia = (size_t)(t * na);
        size_t ib = (size_t)(t * nb);
        Bezier ca = morph_segment(a, left, ia, 0, 0);
        Bezier cb = morph_segment(b, right, ib, offset, reverse);
        ManicVec2 pa, pb;
        double dx, dy;

        if(!bezier_evaluate(&ca, t * na - ia, &pa) ||
            !bezier_evaluate(&cb, t * nb - ib, &pb)) {return(1e100);}
        dx = (double)pa.x - pb.x;
        dy = (double)pa.y - pb.y;
        cost += dx * dx + dy * dy;
    }
    return(sqrt(cost / 24));
}

static double morph_best_alignment(const VectorPath *a, const MorphSubpath *left,
    const VectorPath *b, const MorphSubpath *right, int allow_reverse, size_t *offset, int *reverse)
{
    size_t count = right->end - right->start;
    size_t choices = right->closed ? count : 1;
    double best = 1e100;

    *offset = 0;
    *reverse = 0;
    for(int direction = allow_reverse == 2 ? 1 : 0; direction <= (allow_reverse != 0); direction++) {
        for(size_t i = 0; i < choices && i < 256; i++) {
            size_t candidate = choices > 256 ? i * (choices / 256) + i * (choices % 256) / 256 : i;
            size_t start = right->closed ? candidate : (direction ? count : 0);
            double cost = morph_alignment(a, left, b, right, start, direction);

            if(cost < best) {best = cost; *offset = start; *reverse = direction;}
        }
    }
    if(right->closed && choices > 256) {
        size_t span = choices / 256 + (choices % 256 != 0);
        size_t center = *offset;
        int direction = *reverse;

        for(size_t i = 1; i <= span; i++) {
            size_t candidates[2] = {(center + i) % choices, (center + choices - i) % choices};

            for(int side = 0; side < 2; side++) {
                double cost = morph_alignment(a, left, b, right, candidates[side], direction);

                if(cost < best) {best = cost; *offset = candidates[side];}
            }
        }
    }
    return(best);
}

static double morph_subpath_cost(const VectorPath *a, const MorphSubpath *left,
    const VectorPath *b, const MorphSubpath *right, int allow_reverse)
{
    size_t offset;
    int reverse;

    if(left->closed != right->closed) {return(1e100);}
    return(morph_best_alignment(a, left, b, right, allow_reverse, &offset, &reverse) +
        0.5 * hypot(left->width - right->width, left->height - right->height));
}

static int path_match_subpaths(const VectorPath *a, const VectorPath *b,
    MorphSubpath **source, MorphSubpath **target, size_t **matching, size_t *count, int *global_reverse)
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
    double *costs = NULL;
    unsigned char *used = NULL;
    int success = 0;
    double best_total = 1e100;

    if(!path_describe_subpaths(a, &left, &n) || !path_describe_subpaths(b, &right, &m) || n != m ||
        n == SIZE_MAX || n + 1 > SIZE_MAX / sizeof(double) || n + 1 > SIZE_MAX / sizeof(size_t) ||
        n > SIZE_MAX / n || n * n > SIZE_MAX / sizeof(*costs)) {goto cleanup;}
    assignment = malloc(n * sizeof(*assignment));
    p = calloc(n + 1, sizeof(*p));
    way = calloc(n + 1, sizeof(*way));
    u = calloc(n + 1, sizeof(*u));
    v = calloc(n + 1, sizeof(*v));
    minimum = malloc((n + 1) * sizeof(*minimum));
    used = malloc(n + 1);
    costs = malloc(n * n * sizeof(*costs));
    if(assignment == NULL || p == NULL || way == NULL || u == NULL || v == NULL ||
        minimum == NULL || used == NULL || costs == NULL) {goto cleanup;}
    for(int direction = 0; direction <= (n > 1); direction++) {
        memset(p, 0, (n + 1) * sizeof(*p));
        memset(u, 0, (n + 1) * sizeof(*u));
        memset(v, 0, (n + 1) * sizeof(*v));
        for(size_t i = 0; i < n; i++) {
            for(size_t j = 0; j < n; j++) {
                costs[i * n + j] = morph_subpath_cost(a, &left[i], b, &right[j],
                    n == 1 ? 1 : (direction ? 2 : 0));
            }
        }
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
                        double cost = costs[(current - 1) * n + j - 1] - u[current] - v[j];

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
        {
            double total = 0;

            for(size_t j = 1; j <= n; j++) {total += costs[(p[j] - 1) * n + j - 1];}
            if(total < best_total) {
                best_total = total;
                *global_reverse = direction;
                for(size_t j = 1; j <= n; j++) {assignment[p[j] - 1] = j - 1;}
            }
        }
    }
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
    free(costs);
    free(used);
    return(success);
}

int path_morphable(const VectorPath *a, const VectorPath *b)
{
    MorphSubpath *source = NULL;
    MorphSubpath *target = NULL;
    size_t *matching = NULL;
    size_t count;
    int global_reverse;
    int result = path_match_subpaths(a, b, &source, &target, &matching, &count, &global_reverse);

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

static int morph_cut_contour(const VectorPath *path, const MorphSubpath *subpath,
    size_t offset, int reverse, double fraction, VectorPath *result)
{
    size_t count = subpath->end - subpath->start;
    Bezier first = morph_segment(path, subpath, 0, offset, reverse);
    Bezier before, after;

    if(!bezier_subdivide(&first, fraction, &before, &after)) {return(0);}
    result->segments[0] = after;
    for(size_t i = 1; i < count; i++) {result->segments[i] = morph_segment(path, subpath, i, offset, reverse);}
    result->segments[count] = before;
    result->count = count + 1;
    return(1);
}

static double morph_cut_cost(const VectorPath *a, const MorphSubpath *left,
    const VectorPath *candidate, const MorphSubpath *right)
{
    ManicVec2 p = a->segments[left->start].points[0];
    ManicVec2 q = candidate->segments[0].points[0];

    return(morph_alignment(a, left, candidate, right, 0, 0) +
        0.5 * hypot((double)p.x - q.x, (double)p.y - q.y));
}

static int path_interpolate_matched(const VectorPath *a, const VectorPath *b, float alpha,
    VectorPath *output, MorphSubpath *source, MorphSubpath *target, size_t *matching, size_t count, int global_reverse)
{
    for(size_t subpath = 0; subpath < count; subpath++) {
        MorphSubpath *left = &source[subpath];
        MorphSubpath *right = &target[matching[subpath]];
        size_t sa = 0;
        size_t sb = 0;
        double t = 0.0;
        size_t na = left->end - left->start;
        size_t nb = right->end - right->start;
        size_t offset;
        int reverse;
        Bezier first;
        VectorPath cut = {0};
        MorphSubpath cut_description = {0};
        const VectorPath *right_path = b;
        double best;

        best = morph_best_alignment(a, left, b, right,
            count == 1 ? 1 : (global_reverse ? 2 : 0), &offset, &reverse);
        first = morph_segment(b, right, 0, offset, reverse);
        if(right->closed && nb < 1024 && nb < SIZE_MAX / sizeof(*cut.segments) - 1) {
            ManicVec2 start = a->segments[left->start].points[0];
            double best_cut = 0;
            size_t best_offset = offset;

            best += 0.5 * hypot((double)start.x - first.points[0].x, (double)start.y - first.points[0].y);
            cut.segments = malloc((nb + 1) * sizeof(*cut.segments));
            if(cut.segments == NULL) {return(0);}
            cut_description = (MorphSubpath){0, nb + 1, 1, 0, 0, 0, 0};
            for(int side = 0; side < 2; side++) {
                size_t candidate = side ? (offset + nb - 1) % nb : offset;

                for(int sample = 1; sample < 32; sample++) {
                    double fraction = sample / 32.0;
                    double cost;

                    if(!morph_cut_contour(b, right, candidate, reverse, fraction, &cut)) {free(cut.segments); return(0);}
                    cost = morph_cut_cost(a, left, &cut, &cut_description);
                    if(cost < best) {best = cost; best_cut = fraction; best_offset = candidate;}
                }
            }
            if(best_cut > 0) {
                double lo = fmax(0.000001, best_cut - 1.0 / 32);
                double hi = fmin(0.999999, best_cut + 1.0 / 32);

                for(int i = 0; i < 16; i++) {
                    double x = (2 * lo + hi) / 3;
                    double y = (lo + 2 * hi) / 3;
                    double cx, cy;

                    if(!morph_cut_contour(b, right, best_offset, reverse, x, &cut)) {free(cut.segments); return(0);}
                    cx = morph_cut_cost(a, left, &cut, &cut_description);
                    if(!morph_cut_contour(b, right, best_offset, reverse, y, &cut)) {free(cut.segments); return(0);}
                    cy = morph_cut_cost(a, left, &cut, &cut_description);
                    if(cx < cy) {hi = y;}
                    else {lo = x;}
                }
                if(!morph_cut_contour(b, right, best_offset, reverse, (lo + hi) / 2, &cut)) {free(cut.segments); return(0);}
                right_path = &cut;
                right = &cut_description;
                nb++;
                offset = 0;
                reverse = 0;
                first = cut.segments[0];
            }
        }
        if(!path_move_to(output, vector_lerp(a->segments[left->start].points[0],
            first.points[0], alpha))) {free(cut.segments); return(0);}
        while(sa < na && sb < nb) {
            double nexta = (double)(sa + 1) / na;
            double nextb = (double)(sb + 1) / nb;
            double next = nexta < nextb ? nexta : nextb;
            Bezier ca;
            Bezier cb;
            ManicVec2 points[4];

            Bezier segment = morph_segment(right_path, right, sb, offset, reverse);

            if(!bezier_interval(&a->segments[left->start + sa], t * na - sa, next * na - sa, &ca) ||
                !bezier_interval(&segment, t * nb - sb, next * nb - sb, &cb)) {free(cut.segments); return(0);}
            for(int j = 0; j < 4; j++) {points[j] = vector_lerp(ca.points[j], cb.points[j], alpha);}
            if(!path_cubic_to(output, points[1], points[2], points[3])) {free(cut.segments); return(0);}
            t = next;
            if(nexta <= nextb) {sa++;}
            if(nextb <= nexta) {sb++;}
        }
        if(left->closed) {output->segments[output->count - 1].closes_subpath = 1; output->closed = 1;}
        free(cut.segments);
    }
    return(1);
}

int path_interpolate(const VectorPath *a, const VectorPath *b, float alpha, VectorPath *output)
{
    MorphSubpath *source = NULL, *target = NULL;
    size_t *matching = NULL, count = 0;
    int success;
    int global_reverse;

    if(output == NULL || output == a || output == b || !isfinite(alpha) || alpha < 0 || alpha > 1 ||
        !path_match_subpaths(a, b, &source, &target, &matching, &count, &global_reverse)) {return(0);}
    success = path_interpolate_matched(a, b, alpha, output, source, target, matching, count, global_reverse);
    free(source);
    free(target);
    free(matching);
    return(success);
}

MorphCache *morph_cache_create(const VectorPath *a, const VectorPath *b)
{
    MorphSubpath *source = NULL, *target = NULL;
    size_t *matching = NULL, count = 0;
    int global_reverse;
    MorphCache *cache = calloc(1, sizeof(*cache));
    int ok = cache && path_match_subpaths(a, b, &source, &target, &matching, &count, &global_reverse);

    if(ok) {
        ok = path_interpolate_matched(a, b, 0, &cache->left, source, target, matching, count, global_reverse) &&
            path_interpolate_matched(a, b, 1, &cache->right, source, target, matching, count, global_reverse);
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

static double bezier_speed(const Bezier *curve, double t)
{
    double x[3], y[3];
    int degree = curve->degree;

    for(int i = 0; i < degree; i++) {
        x[i] = degree * ((double)curve->points[i + 1].x - curve->points[i].x);
        y[i] = degree * ((double)curve->points[i + 1].y - curve->points[i].y);
    }
    for(int level = degree - 1; level > 0; level--) {
        for(int i = 0; i < level; i++) {
            x[i] += (x[i + 1] - x[i]) * t;
            y[i] += (y[i + 1] - y[i]) * t;
        }
    }
    return(hypot(x[0], y[0]));
}

static double bezier_arc_integral(const Bezier *curve, double begin, double end)
{
    static const double nodes[] = {0.1834346424956498, 0.5255324099163290,
        0.7966664774136267, 0.9602898564975363};
    static const double weights[] = {0.3626837833783620, 0.3137066458778873,
        0.2223810344533745, 0.1012285362903763};
    double middle = (begin + end) / 2;
    double half = (end - begin) / 2;
    double sum = 0;

    for(int i = 0; i < 4; i++) {
        sum += weights[i] * (bezier_speed(curve, middle - half * nodes[i]) +
            bezier_speed(curve, middle + half * nodes[i]));
    }
    return(sum * half);
}

int bezier_arc_table(const Bezier *curve, ArcCache *cache)
{
    if(!curve || !cache || curve->degree < 1 || curve->degree > 3) {return(0);}
    for(int i = 0; i <= curve->degree; i++) {
        if(!vector_valid(curve->points[i])) {return(0);}
    }
    cache->curve = *curve;
    cache->lengths[0] = 0;
    for(int i = 1; i <= 128; i++) {
        cache->lengths[i] = cache->lengths[i - 1] +
            bezier_arc_integral(curve, (double)(i - 1) / 128, (double)i / 128);
    }
    return(1);
}

double bezier_arc_lookup(const ArcCache *cache, double fraction)
{
    double wanted, begin, end, base;
    int low = 1, high = 128;

    if(fraction <= 0) {return(0);}
    if(fraction >= 1) {return(1);}
    if(cache->lengths[128] == 0) {return(fraction);}
    wanted = cache->lengths[128] * fraction;
    while(low < high) {
        int mid = low + (high - low) / 2;

        if(cache->lengths[mid] < wanted) {low = mid + 1;}
        else {high = mid;}
    }
    begin = (double)(low - 1) / 128;
    end = (double)low / 128;
    base = begin;
    for(int i = 0; i < 24; i++) {
        double middle = (begin + end) / 2;
        double length = cache->lengths[low - 1] + bezier_arc_integral(&cache->curve, base, middle);

        if(length < wanted) {begin = middle;}
        else {end = middle;}
    }
    return((begin + end) / 2);
}

double bezier_arc_parameter(const Bezier *curve, double fraction)
{
    ArcCache cache;

    return(bezier_arc_table(curve, &cache) ? bezier_arc_lookup(&cache, fraction) : fraction);
}

struct PathArcCache {
    VectorPath path;
    double *lengths;
    double tolerance;
};

static int path_arc_integral(const Bezier *curve, double begin, double end, double whole,
    double tolerance, int depth, int *budget, double *result)
{
    double middle = (begin + end) / 2;
    double left = bezier_arc_integral(curve, begin, middle);
    double right = bezier_arc_integral(curve, middle, end);
    double combined = left + right;

    if((*budget)-- <= 0 || !isfinite(combined)) {return(0);}
    if(fabs(combined - whole) <= tolerance) {*result = combined; return(1);}
    if(depth == 0) {return(0);}
    if(!path_arc_integral(curve, begin, middle, left, tolerance / 2, depth - 1, budget, &left) ||
        !path_arc_integral(curve, middle, end, right, tolerance / 2, depth - 1, budget, &right)) {return(0);}
    *result = left + right;
    return(1);
}

PathArcCache *path_arc_create(const VectorPath *path, double tolerance)
{
    PathArcCache *cache;

    if(path == NULL || path->count == 0 || path->segments == NULL || !isfinite(tolerance) || tolerance <= 0 ||
        path->count > (SIZE_MAX / sizeof(double)) - 1) {return(NULL);}
    cache = calloc(1, sizeof(*cache));
    if(cache == NULL) {return(NULL);}
    cache->lengths = calloc(path->count + 1, sizeof(*cache->lengths));
    cache->tolerance = tolerance;
    if(cache->lengths == NULL || !path_clone(&cache->path, path)) {path_arc_destroy(cache); return(NULL);}
    for(size_t i = 0; i < path->count; i++) {
        const Bezier *curve = &path->segments[i];
        double length;
        double whole;
        int budget = 8192;

        if(curve->degree < 1 || curve->degree > 3 || (i && curve->starts_subpath)) {path_arc_destroy(cache); return(NULL);}
        for(int j = 0; j <= curve->degree; j++) {
            if(!vector_valid(curve->points[j])) {path_arc_destroy(cache); return(NULL);}
        }
        if(i) {
            const Bezier *previous = &path->segments[i - 1];
            ManicVec2 end = previous->points[previous->degree];

            if(end.x != curve->points[0].x || end.y != curve->points[0].y) {path_arc_destroy(cache); return(NULL);}
        }
        whole = bezier_arc_integral(curve, 0, 1);
        if(!path_arc_integral(curve, 0, 1, whole, tolerance / path->count, 20, &budget, &length)) {
            path_arc_destroy(cache);
            return(NULL);
        }
        cache->lengths[i + 1] = cache->lengths[i] + length;
    }
    return(cache);
}

void path_arc_destroy(PathArcCache *cache)
{
    if(cache == NULL) {return;}
    path_destroy(&cache->path);
    free(cache->lengths);
    free(cache);
}

int path_arc_point(const PathArcCache *cache, double fraction, ManicVec2 *point)
{
    size_t low = 0, high;
    double target, begin = 0, end = 1;
    const Bezier *curve;

    if(cache == NULL || point == NULL || !isfinite(fraction) || fraction < 0 || fraction > 1) {return(0);}
    high = cache->path.count;
    if(fraction == 0) {return(bezier_evaluate(&cache->path.segments[0], 0, point));}
    if(fraction == 1) {return(bezier_evaluate(&cache->path.segments[high - 1], 1, point));}
    target = cache->lengths[high] * fraction;
    if(target == 0) {return(bezier_evaluate(&cache->path.segments[0], 0, point));}
    while(low + 1 < high) {
        size_t middle = low + (high - low) / 2;

        if(cache->lengths[middle] < target) {low = middle;}
        else {high = middle;}
    }
    curve = &cache->path.segments[low];
    target -= cache->lengths[low];
    for(int i = 0; i < 40; i++) {
        double middle = (begin + end) / 2;
        double length;
        double whole = bezier_arc_integral(curve, 0, middle);
        int budget = 8192;

        if(!path_arc_integral(curve, 0, middle, whole, cache->tolerance / cache->path.count, 20, &budget, &length)) {return(0);}
        if(length < target) {begin = middle;}
        else {end = middle;}
    }
    return(bezier_evaluate(curve, (begin + end) / 2, point));
}
