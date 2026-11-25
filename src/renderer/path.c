#include <math.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    ManicVec2 *points;
    size_t count;
    size_t capacity;
} PathVertices;

static int vertices_append(PathVertices *vertices, ManicVec2 point)
{
    if(vertices->count == vertices->capacity) {
        size_t capacity = vertices->capacity ? vertices->capacity * 2 : 32;
        ManicVec2 *points;

        if(capacity < vertices->capacity || capacity > SIZE_MAX / sizeof(*points)) {return(0);}
        points = realloc(vertices->points, capacity * sizeof(*points));
        if(points == NULL) {return(0);}
        vertices->points = points;
        vertices->capacity = capacity;
    }
    vertices->points[vertices->count++] = point;
    return(1);
}

static int curve_flat_enough(const Bezier *curve)
{
    double dx = (double)curve->points[curve->degree].x - curve->points[0].x;
    double dy = (double)curve->points[curve->degree].y - curve->points[0].y;
    double length_squared = dx * dx + dy * dy;

    for(int i = 1; i < curve->degree; i++) {
        double px = (double)curve->points[i].x - curve->points[0].x;
        double py = (double)curve->points[i].y - curve->points[0].y;
        double cross = dx * py - dy * px;
        double projection = dx * px + dy * py;

        if(length_squared < 1e-12) {
            if(px * px + py * py > 0.25) {return(0);}
        }
        else if(cross * cross > 0.25 * length_squared || projection < 0.0 || projection > length_squared) {
            return(0);
        }
    }
    return(1);
}

static int flatten_curve(PathVertices *vertices, const Bezier *curve, int depth)
{
    Bezier left;
    Bezier right;

    if(depth >= 12 || curve_flat_enough(curve)) {
        return(vertices_append(vertices, curve->points[curve->degree]));
    }
    if(!bezier_subdivide(curve, 0.5, &left, &right)) {return(0);}
    return(flatten_curve(vertices, &left, depth + 1) && flatten_curve(vertices, &right, depth + 1));
}

static double point_cross(ManicVec2 a, ManicVec2 b, ManicVec2 c)
{
    return(((double)b.x - a.x) * ((double)c.y - a.y) -
        ((double)b.y - a.y) * ((double)c.x - a.x));
}

static void render_bevel(ManicVec2 before, ManicVec2 point, ManicVec2 after, float radius, Color color)
{
    double dx1 = point.x - before.x;
    double dy1 = point.y - before.y;
    double dx2 = after.x - point.x;
    double dy2 = after.y - point.y;
    double length1 = hypot(dx1, dy1);
    double length2 = hypot(dx2, dy2);
    double cross = dx1 * dy2 - dy1 * dx2;
    double side;
    ManicVec2 a;
    ManicVec2 b;

    if(length1 == 0.0 || length2 == 0.0 || fabs(cross) < 1e-9) {return;}
    side = cross > 0.0 ? -1.0 : 1.0;
    a = (ManicVec2){point.x + (float)(side * -dy1 * radius / length1),
        point.y + (float)(side * dx1 * radius / length1)};
    b = (ManicVec2){point.x + (float)(side * -dy2 * radius / length2),
        point.y + (float)(side * dx2 * radius / length2)};
    if(point_cross(point, a, b) < 0.0) {DrawTriangle(raylib_vector(point), raylib_vector(a), raylib_vector(b), color);}
    else {DrawTriangle(raylib_vector(point), raylib_vector(b), raylib_vector(a), color);}
}

static void render_miter(ManicVec2 before, ManicVec2 point, ManicVec2 after, float radius, Color color)
{
    double dx1 = point.x - before.x;
    double dy1 = point.y - before.y;
    double dx2 = after.x - point.x;
    double dy2 = after.y - point.y;
    double length1 = hypot(dx1, dy1);
    double length2 = hypot(dx2, dy2);
    double cross = dx1 * dy2 - dy1 * dx2;
    double side;
    double nx1;
    double ny1;
    double nx2;
    double ny2;
    double denominator;
    ManicVec2 a;
    ManicVec2 b;
    ManicVec2 tip;

    if(length1 == 0.0 || length2 == 0.0 || fabs(cross) < 1e-9) {return;}
    side = cross > 0.0 ? -1.0 : 1.0;
    nx1 = side * -dy1 / length1;
    ny1 = side * dx1 / length1;
    nx2 = side * -dy2 / length2;
    ny2 = side * dx2 / length2;
    denominator = 1.0 + nx1 * nx2 + ny1 * ny2;
    if(denominator <= 0.0 || 2.0 / denominator > 16.0) {
        render_bevel(before, point, after, radius, color);
        return;
    }
    a = (ManicVec2){point.x + (float)(nx1 * radius), point.y + (float)(ny1 * radius)};
    b = (ManicVec2){point.x + (float)(nx2 * radius), point.y + (float)(ny2 * radius)};
    tip = (ManicVec2){point.x + (float)((nx1 + nx2) * radius / denominator),
        point.y + (float)((ny1 + ny2) * radius / denominator)};
    if(point_cross(point, a, tip) < 0.0) {
        DrawTriangle(raylib_vector(point), raylib_vector(a), raylib_vector(tip), color);
        DrawTriangle(raylib_vector(point), raylib_vector(tip), raylib_vector(b), color);
    }
    else {
        DrawTriangle(raylib_vector(point), raylib_vector(tip), raylib_vector(a), color);
        DrawTriangle(raylib_vector(point), raylib_vector(b), raylib_vector(tip), color);
    }
}

static void render_contour(const PathVertices *vertices, int closed, const Mobject *mobject, float pixel_scale)
{
    size_t count = vertices->count;
    float width = mobject->stroke_width * pixel_scale;

    while(closed && count > 1 && vertices->points[0].x == vertices->points[count - 1].x &&
        vertices->points[0].y == vertices->points[count - 1].y) {count--;}
    if(width <= 0 || !mobject->color.a) {return;}
    for(size_t i = 1; i < vertices->count; i++) {
        ManicVec2 a = vertices->points[i - 1];
        ManicVec2 b = vertices->points[i];

        if(!closed && mobject->path_cap == PATH_CAP_SQUARE && (i == 1 || i + 1 == vertices->count)) {
            double dx = b.x - a.x;
            double dy = b.y - a.y;
            double length = hypot(dx, dy);

            if(length > 0.0) {
                float x = (float)(dx * width / (2.0 * length));
                float y = (float)(dy * width / (2.0 * length));

                if(i == 1) {a.x -= x; a.y -= y;}
                if(i + 1 == vertices->count) {b.x += x; b.y += y;}
            }
        }
        DrawLineEx(raylib_vector(a), raylib_vector(b), width, raylib_color(mobject->color));
    }
    for(size_t i = 0; i < count; i++) {
        int endpoint = !closed && (i == 0 || i + 1 == count);

        if((endpoint && mobject->path_cap == PATH_CAP_ROUND) ||
            (!endpoint && mobject->path_join == PATH_JOIN_ROUND)) {
            DrawCircleV(raylib_vector(vertices->points[i]), width / 2.0f, raylib_color(mobject->color));
        }
        else if(!endpoint && mobject->path_join == PATH_JOIN_BEVEL) {
            render_bevel(vertices->points[(i + count - 1) % count], vertices->points[i],
                vertices->points[(i + 1) % count], width / 2.0f, raylib_color(mobject->color));
        }
        else if(!endpoint && mobject->path_join == PATH_JOIN_MITER) {
            render_miter(vertices->points[(i + count - 1) % count], vertices->points[i],
                vertices->points[(i + 1) % count], width / 2.0f, raylib_color(mobject->color));
        }
    }
}

static int contour_append(TessContour **contours, size_t *count, size_t *capacity,
    PathVertices *vertices, int closed)
{
    if(*count == *capacity) {
        size_t next = *capacity ? *capacity * 2 : 8;
        TessContour *items;

        if(next < *capacity || next > SIZE_MAX / sizeof(*items)) {return(0);}
        items = realloc(*contours, next * sizeof(*items));
        if(items == NULL) {return(0);}
        *contours = items;
        *capacity = next;
    }
    (*contours)[*count] = (TessContour){vertices->points, vertices->count, closed};
    (*count)++;
    *vertices = (PathVertices){0};
    return(1);
}

static int camera_render_path(const ManicCamera *camera, const Mobject *mobjects, int node,
    const Mobject *mobject, int width, int height, float pixel_scale)
{
    const VectorPath *path = mobject->morph_source ? mobject->morph_source : mobject->path;
    VectorPath interpolated = {0};
    PathVertices vertices = {0};
    TessContour *contours = NULL;
    TessMesh mesh = {0};
    size_t count = 0;
    size_t capacity = 0;
    int success = 1;

    if(path == NULL) {return(0);}
    if(mobject->morph_target != NULL) {
        if(!path_interpolate(path, mobject->morph_target, mobject->morph_alpha, &interpolated)) {
            path_destroy(&interpolated);
            return(0);
        }
        path = &interpolated;
    }
    for(size_t i = 0; i < path->count; i++) {
        Bezier curve = path->segments[i];
        float cosine = cosf(mobject->rotation);
        float sine = sinf(mobject->rotation);


        if(curve.starts_subpath && vertices.count &&
            !contour_append(&contours, &count, &capacity, &vertices, 0)) {success = 0; break;}
        for(int j = 0; j <= curve.degree; j++) {
            float x = curve.points[j].x * mobject->path_scale;
            float y = curve.points[j].y * mobject->path_scale;
            ManicVec2 point = {mobject->position.x + x * cosine - y * sine,
                mobject->position.y + x * sine + y * cosine};

            curve.points[j] = camera_to_screen(camera, camera_world_point(mobjects, node, point), width, height);
        }
        if((!vertices.count && !vertices_append(&vertices, curve.points[0])) ||
            !flatten_curve(&vertices, &curve, 0)) {success = 0; break;}
        if(curve.closes_subpath && !contour_append(&contours, &count, &capacity, &vertices, 1)) {
            success = 0;
            break;
        }
    }
    if(success && vertices.count && !contour_append(&contours, &count, &capacity, &vertices, 0)) {success = 0;}
    if(success && mobject->fill_color.a && mobject->path_progress > 0.0f) {
        if(tessellate_contours(contours, count, mobject->path_fill_rule, &mesh)) {
            ManicColor fill = mobject->fill_color;

            fill.a = (unsigned char)(fill.a * mobject->path_progress + 0.5f);
            for(size_t i = 0; i + 2 < mesh.count; i += 3) {
                DrawTriangle(raylib_vector(mesh.vertices[i]), raylib_vector(mesh.vertices[i + 1]),
                    raylib_vector(mesh.vertices[i + 2]), raylib_color(fill));
            }
        }
        else {success = 0;}
    }
    if(success && mobject->path_progress >= 1.0f) {
        for(size_t i = 0; i < count; i++) {
            PathVertices contour = {contours[i].points, contours[i].count, contours[i].count};

            render_contour(&contour, contours[i].closed, mobject, pixel_scale);
        }
    }
    else if(success && mobject->path_progress > 0.0f) {
        double length = 0.0;
        double remaining;

        for(size_t i = 0; i < count; i++) {
            for(size_t j = 1; j < contours[i].count; j++) {
                length += hypot((double)contours[i].points[j].x - contours[i].points[j - 1].x,
                    (double)contours[i].points[j].y - contours[i].points[j - 1].y);
            }
        }
        remaining = length * mobject->path_progress;
        for(size_t i = 0; i < count && remaining > 0.0 && success; i++) {
            PathVertices partial = {0};
            int complete = 1;

            if(contours[i].count && !vertices_append(&partial, contours[i].points[0])) {success = 0;}
            for(size_t j = 1; j < contours[i].count && success; j++) {
                ManicVec2 a = contours[i].points[j - 1];
                ManicVec2 b = contours[i].points[j];
                double segment_length = hypot((double)b.x - a.x, (double)b.y - a.y);

                if(segment_length > remaining) {
                    if(!vertices_append(&partial, vector_lerp(a, b, remaining / segment_length))) {success = 0;}
                    remaining = 0.0;
                    complete = 0;
                    break;
                }
                if(!vertices_append(&partial, b)) {success = 0;}
                remaining -= segment_length;
            }
            if(success) {render_contour(&partial, complete && contours[i].closed, mobject, pixel_scale);}
            free(partial.points);
        }
    }
    if(!success) {TraceLog(LOG_WARNING, "MANIC: Could not render vector path");}
    path_destroy(&interpolated);
    free(mesh.vertices);
    free(vertices.points);
    for(size_t i = 0; i < count; i++) {free(contours[i].points);}
    free(contours);
    return(success);
}
