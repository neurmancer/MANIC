#include "internal.h"
#include <float.h>
#include <math.h>
#include <stdio.h>

int axes_point(const MathAxes *axes, double x, double y, ManicVec2 *point)
{
    double px;
    double py;

    if(axes == NULL || point == NULL || !isfinite(x) || !isfinite(y) ||
        !isfinite(axes->x_min) || !isfinite(axes->x_max) ||
        !isfinite(axes->y_min) || !isfinite(axes->y_max) ||
        !isfinite(axes->width) || !isfinite(axes->height) ||
        axes->x_max <= axes->x_min || axes->y_max <= axes->y_min ||
        axes->width <= 0 || axes->height <= 0) {return(0);}
    px = ((x - axes->x_min) / ((double)axes->x_max - axes->x_min) - 0.5) * axes->width;
    py = ((y - axes->y_min) / ((double)axes->y_max - axes->y_min) - 0.5) * axes->height;
    if(!isfinite(px) || !isfinite(py) || fabs(px) > FLT_MAX || fabs(py) > FLT_MAX) {return(0);}
    *point = (ManicVec2){(float)px, (float)py};
    return(1);
}

int scene_number_line(Scene *scene, ManicVec2 start, ManicVec2 end, float min, float max,
    float tick_step, float tick_size, ManicColor color)
{
    VectorPath path = {0};
    int id = -1;
    double dx = (double)end.x - start.x;
    double dy = (double)end.y - start.y;
    double length = hypot(dx, dy);
    double first;
    double ticks;

    if(!vector_valid(start) || !vector_valid(end) || !isfinite(min) || !isfinite(max) ||
        !isfinite(tick_step) || !isfinite(tick_size) || max <= min || tick_step <= 0 ||
        tick_size < 0 || length == 0.0) {return(-1);}
    first = ceil((double)min / tick_step);
    ticks = floor((double)max / tick_step) - first + 1;
    if(!isfinite(ticks) || ticks > 10000 || !isfinite(first) || fabs(first) > 1e12) {return(-1);}
    if(!path_move_to(&path, start) || !path_line_to(&path, end)) {goto cleanup;}
    for(int i = 0; i < (int)ticks; i++) {
        double value = (first + i) * tick_step;
        double fraction = (value - min) / (max - min);
        ManicVec2 middle = {(float)(start.x + dx * fraction), (float)(start.y + dy * fraction)};
        ManicVec2 offset = {(float)(-dy / length * tick_size / 2), (float)(dx / length * tick_size / 2)};

        if(!path_move_to(&path, (ManicVec2){middle.x - offset.x, middle.y - offset.y}) ||
            !path_line_to(&path, (ManicVec2){middle.x + offset.x, middle.y + offset.y})) {goto cleanup;}
    }
    id = scene_add(scene, path_mobject(&path, (ManicVec2){0, 0}, color, (ManicColor){0}, 2));

cleanup:
    path_destroy(&path);
    return(id);
}

int scene_axes(Scene *scene, MathAxes *axes, ManicVec2 position, float tick_step, ManicColor color)
{
    ManicVec2 x1;
    ManicVec2 x2;
    ManicVec2 y1;
    ManicVec2 y2;
    int group;
    int xline;
    int yline;

    if(axes == NULL || !vector_valid(position) || axes->x_min > 0 || axes->x_max < 0 ||
        axes->y_min > 0 || axes->y_max < 0 ||
        !axes_point(axes, axes->x_min, 0, &x1) || !axes_point(axes, axes->x_max, 0, &x2) ||
        !axes_point(axes, 0, axes->y_min, &y1) || !axes_point(axes, 0, axes->y_max, &y2)) {return(0);}
    group = scene_add(scene, group_create(position));
    if(group < 0) {return(0);}
    xline = scene_number_line(scene, x1, x2, axes->x_min, axes->x_max, tick_step, 0.12f, color);
    yline = scene_number_line(scene, y1, y2, axes->y_min, axes->y_max, tick_step, 0.12f, color);
    if(xline < 0 || yline < 0 || !scene_set_parent(scene, xline, group) ||
        !scene_set_parent(scene, yline, group)) {
        if(xline >= 0) {scene_remove(scene, xline);}
        if(yline >= 0) {scene_remove(scene, yline);}
        scene_remove(scene, group);
        return(0);
    }
    axes->group = group;
    return(1);
}

typedef struct {
    const MathAxes *axes;
    MathFunction function;
    void *context;
    VectorPath *path;
    GraphSampling options;
    int drawing;
    int budget;
} GraphBuilder;

typedef struct {
    double x;
    double y;
} GraphSample;

static int graph_segment(GraphBuilder *graph, GraphSample a, GraphSample b)
{
    const MathAxes *axes = graph->axes;
    ManicVec2 p, q;
    double t0 = 0, t1 = 1, dy = b.y - a.y;

    if(!isfinite(a.y) || !isfinite(b.y) || !isfinite(dy)) {
        graph->drawing = 0;
        return(1);
    }
    if(dy == 0) {
        if(a.y < axes->y_min || a.y > axes->y_max) {
            graph->drawing = 0;
            return(1);
        }
    }
    else {
        double lo = (axes->y_min - a.y) / dy;
        double hi = (axes->y_max - a.y) / dy;

        if(lo > hi) {
            double tmp = lo;

            lo = hi;
            hi = tmp;
        }
        if(lo > t0) {t0 = lo;}
        if(hi < t1) {t1 = hi;}
        if(t0 > t1 || t1 < 0 || t0 > 1) {
            graph->drawing = 0;
            return(1);
        }
    }
    if(!axes_point(axes, a.x + (b.x - a.x) * t0, a.y + dy * t0, &p) ||
        !axes_point(axes, a.x + (b.x - a.x) * t1, a.y + dy * t1, &q)) {
        graph->drawing = 0;
        return(1);
    }
    if((!graph->drawing || t0 > 0) && !path_move_to(graph->path, p)) {return(0);}
    if(!path_line_to(graph->path, q)) {return(0);}
    graph->drawing = t1 >= 1;
    return(1);
}

static int graph_refine(GraphBuilder *graph, GraphSample a, GraphSample b, int depth)
{
    GraphSample mid = {(a.x + b.x) * 0.5, 0};
    double range = (double)graph->axes->y_max - graph->axes->y_min;
    int split;

    if(graph->budget <= 0) {return(0);}
    if(mid.x == a.x || mid.x == b.x) {
        graph->drawing = 0;
        return(1);
    }
    graph->budget--;
    mid.y = graph->function(mid.x, graph->context);
    if(isfinite(a.y) && isfinite(mid.y) && isfinite(b.y) &&
        ((a.y < graph->axes->y_min && mid.y < graph->axes->y_min && b.y < graph->axes->y_min) ||
         (a.y > graph->axes->y_max && mid.y > graph->axes->y_max && b.y > graph->axes->y_max)) &&
        fabs(mid.y - (a.y * 0.5 + b.y * 0.5)) <= graph->options.tolerance * range) {
        graph->drawing = 0;
        return(1);
    }
    split = !isfinite(a.y) || !isfinite(mid.y) || !isfinite(b.y) ||
        fabs(mid.y - (a.y * 0.5 + b.y * 0.5)) > graph->options.tolerance * range ||
        fabs(a.y - b.y) > range * 0.5;
    if(split && depth < graph->options.max_depth) {
        return(graph_refine(graph, a, mid, depth + 1) && graph_refine(graph, mid, b, depth + 1));
    }
    if(split && (!isfinite(a.y) || !isfinite(mid.y) || !isfinite(b.y) ||
        fabs(a.y - b.y) > range * 0.5 ||
        fabs(mid.y - (a.y * 0.5 + b.y * 0.5)) > graph->options.tolerance * range * 4)) {
        graph->drawing = 0;
        return(1);
    }
    return(graph_segment(graph, a, b));
}

int scene_function_graph_with_breaks(Scene *scene, const MathAxes *axes, MathFunction function, void *context,
    int samples, GraphSampling options, const double *breaks, size_t break_count, ManicColor color)
{
    VectorPath path = {0};
    GraphBuilder graph = {axes, function, context, &path, options, 0, 4000000};
    GraphSample previous;
    ManicVec2 validation;
    int id = -1, slot;
    size_t next_break = 0;

    if(!scene || !axes || !function || samples < 2 || samples > 1000000 ||
        (break_count && !breaks) || options.max_depth < 0 || options.max_depth > 20 ||
        !isfinite(options.tolerance) || options.tolerance <= 0 ||
        !axes_point(axes, axes->x_min, axes->y_min, &validation) ||
        (slot = scene_slot(scene, axes->group)) < 0 || !scene->mobjects[slot].active) {return(-1);}
    for(size_t i = 0; i < break_count; i++) {
        if(!isfinite(breaks[i]) || breaks[i] <= axes->x_min || breaks[i] >= axes->x_max ||
            (i && breaks[i] <= breaks[i - 1])) {return(-1);}
    }
    previous = (GraphSample){axes->x_min, function(axes->x_min, context)};
    for(int i = 1; i < samples; i++) {
        GraphSample current;

        current.x = axes->x_min + ((double)axes->x_max - axes->x_min) * i / (samples - 1);
        while(next_break < break_count && breaks[next_break] <= current.x) {
            double point = breaks[next_break++];
            double before = nextafter(point, -INFINITY);
            double after = nextafter(point, INFINITY);

            if(before > previous.x) {
                GraphSample edge = {before, function(before, context)};

                if(!graph_refine(&graph, previous, edge, 0)) {goto cleanup;}
            }
            graph.drawing = 0;
            previous = (GraphSample){after, function(after, context)};
        }
        if(current.x > previous.x) {
            current.y = function(current.x, context);
            if(!graph_refine(&graph, previous, current, 0)) {goto cleanup;}
            previous = current;
        }
    }
    if(path.count == 0) {goto cleanup;}
    id = scene_add(scene, path_mobject(&path, (ManicVec2){0, 0}, color, (ManicColor){0}, 3));
    if(id >= 0 && !scene_set_parent(scene, id, axes->group)) {
        scene_remove(scene, id);
        id = -1;
    }

cleanup:
    path_destroy(&path);
    return(id);
}

int scene_function_graph_adaptive(Scene *scene, const MathAxes *axes, MathFunction function, void *context,
    int samples, GraphSampling options, ManicColor color)
{
    return(scene_function_graph_with_breaks(scene, axes, function, context, samples, options, NULL, 0, color));
}

int scene_function_graph(Scene *scene, const MathAxes *axes, MathFunction function, void *context,
    int samples, ManicColor color)
{
    return(scene_function_graph_adaptive(scene, axes, function, context, samples,
        (GraphSampling){8, 0.001}, color));
}

static int number_stroke(VectorPath *path, ManicVec2 a, ManicVec2 b)
{
    return(path_move_to(path, a) && path_line_to(path, b));
}

static int number_glyph(VectorPath *path, char character, float x, float height)
{
    static const unsigned char digits[] = {63, 6, 91, 79, 102, 109, 125, 7, 127, 111};
    static const float segments[7][4] = {
        {0,1,0.6f,1}, {0.6f,1,0.6f,0.5f}, {0.6f,0.5f,0.6f,0},
        {0,0,0.6f,0}, {0,0,0,0.5f}, {0,0.5f,0,1}, {0,0.5f,0.6f,0.5f}
    };
    unsigned int mask = 0;

    if(character >= '0' && character <= '9') {mask = digits[character - '0'];}
    else if(character == '-') {mask = 64;}
    else if(character == 'e' || character == 'E') {mask = 121;}
    else if(character == '+') {
        return(number_stroke(path, (ManicVec2){x, 0.5f * height}, (ManicVec2){x + 0.6f * height, 0.5f * height}) &&
            number_stroke(path, (ManicVec2){x + 0.3f * height, 0.2f * height}, (ManicVec2){x + 0.3f * height, 0.8f * height}));
    }
    else if(character == '.') {
        return(number_stroke(path, (ManicVec2){x + 0.25f * height, 0}, (ManicVec2){x + 0.35f * height, 0}));
    }
    else {return(0);}
    for(int i = 0; i < 7; i++) {
        if((mask & (1u << i)) &&
            !number_stroke(path, (ManicVec2){x + segments[i][0] * height, segments[i][1] * height},
                (ManicVec2){x + segments[i][2] * height, segments[i][3] * height})) {return(0);}
    }
    return(1);
}

int scene_number_label_formatted(Scene *scene, double value, ManicVec2 position, float height,
    ManicColor color, NumberFormat format)
{
    VectorPath path = {0};
    char text[384];
    size_t length;
    float alignment;
    int id = -1;
    int written;

    if(scene == NULL || !isfinite(value) || !vector_valid(position) || !isfinite(height) || height <= 0 ||
        format.notation < NUMBER_GENERAL || format.notation > NUMBER_SCIENTIFIC ||
        format.alignment < NUMBER_ALIGN_LEFT || format.alignment > NUMBER_ALIGN_RIGHT ||
        format.precision < 0 || format.precision > 12) {return(-1);}
    if(value == 0.0) {value = 0.0;}
    if(format.notation == NUMBER_FIXED) {
        written = snprintf(text, sizeof(text), format.show_plus ? "%+.*f" : "%.*f", format.precision, value);
    }
    else if(format.notation == NUMBER_SCIENTIFIC) {
        written = snprintf(text, sizeof(text), format.show_plus ? "%+.*e" : "%.*e", format.precision, value);
    }
    else {written = snprintf(text, sizeof(text), format.show_plus ? "%+.*g" : "%.*g", format.precision, value);}
    if(written < 0 || (size_t)written >= sizeof(text)) {return(-1);}
    length = (size_t)written;
    alignment = format.alignment == NUMBER_ALIGN_LEFT ? 0 :
        (format.alignment == NUMBER_ALIGN_RIGHT ? (float)length * 0.8f - 0.2f : ((float)length * 0.8f - 0.2f) / 2);
    for(size_t i = 0; i < length; i++) {
        float x = ((float)i * 0.8f - alignment) * height;

        if(!number_glyph(&path, text[i], x, height)) {goto cleanup;}
    }
    id = scene_add(scene, path_mobject(&path, position, color, (ManicColor){0}, 1.5f));

cleanup:
    path_destroy(&path);
    return(id);
}

int scene_number_label(Scene *scene, double value, ManicVec2 position, float height, ManicColor color)
{
    return(scene_number_label_formatted(scene, value, position, height, color,
        (NumberFormat){NUMBER_GENERAL, NUMBER_ALIGN_CENTER, 6, 0}));
}

int scene_axes_labels(Scene *scene, const MathAxes *axes, float tick_step, float height, ManicColor color)
{
    double first_x;
    double first_y;
    double count_x;
    double count_y;
    int group_slot;

    if(scene == NULL || axes == NULL || !isfinite(tick_step) || tick_step <= 0 ||
        !isfinite(height) || height <= 0) {return(0);}
    group_slot = scene_slot(scene, axes->group);
    if(group_slot < 0 || scene->mobjects[group_slot].removed) {return(0);}
    first_x = ceil((double)axes->x_min / tick_step);
    first_y = ceil((double)axes->y_min / tick_step);
    count_x = floor((double)axes->x_max / tick_step) - first_x + 1;
    count_y = floor((double)axes->y_max / tick_step) - first_y + 1;
    if(!isfinite(count_x) || !isfinite(count_y) || count_x < 0 || count_y < 0 ||
        count_x > 10000 || count_y > 10000) {return(0);}
    for(int axis = 0; axis < 2; axis++) {
        double first = axis ? first_y : first_x;
        int count = (int)(axis ? count_y : count_x);

        for(int i = 0; i < count; i++) {
            double value = (first + i) * tick_step;
            ManicVec2 point;
            int label;

            if(axis && fabs(value) < tick_step * 1e-6) {continue;}
            if(!axes_point(axes, axis ? 0 : value, axis ? value : 0, &point)) {return(0);}
            if(axis) {point.x -= height * 1.6f; point.y -= height * 0.5f;}
            else {point.y -= height * 1.6f;}
            label = scene_number_label(scene, value, point, height, color);
            if(label < 0) {return(0);}
            if(!scene_set_parent(scene, label, axes->group)) {scene_remove(scene, label); return(0);}
        }
    }
    return(1);
}

int scene_number_line_labels(Scene *scene, int group, ManicVec2 start, ManicVec2 end,
    float min, float max, float tick_step, float offset, float height, ManicColor color)
{
    double dx = (double)end.x - start.x;
    double dy = (double)end.y - start.y;
    double length = hypot(dx, dy);
    double first;
    double count;
    int group_slot = scene_slot(scene, group);

    if(group_slot < 0 || scene->mobjects[group_slot].removed ||
        scene->mobjects[group_slot].type != MOBJECT_GROUP || !vector_valid(start) || !vector_valid(end) ||
        !isfinite(min) || !isfinite(max) || max <= min || !isfinite(tick_step) || tick_step <= 0 ||
        !isfinite(offset) || !isfinite(height) || height <= 0 || length == 0) {return(0);}
    first = ceil((double)min / tick_step);
    count = floor((double)max / tick_step) - first + 1;
    if(!isfinite(first) || fabs(first) > 1e12 || !isfinite(count) || count < 0 || count > 10000) {return(0);}
    for(int i = 0; i < (int)count; i++) {
        double value = (first + i) * tick_step;
        double fraction = (value - min) / ((double)max - min);
        ManicVec2 position = {(float)(start.x + dx * fraction - dy / length * offset),
            (float)(start.y + dy * fraction + dx / length * offset)};
        int label = scene_number_label(scene, value, position, height, color);

        if(label < 0) {return(0);}
        if(!scene_set_parent(scene, label, group)) {scene_remove(scene, label); return(0);}
    }
    return(1);
}

int scene_number_plane(Scene *scene, MathAxes *axes, ManicVec2 position,
    float tick_step, float grid_step, ManicColor axis_color, ManicColor grid_color)
{
    VectorPath grid = {0};
    double first_x, first_y, count_x, count_y;
    int id = -1;
    int result = 0;

    if(scene == NULL || axes == NULL || !isfinite(grid_step) || grid_step <= 0) {return(0);}
    first_x = ceil((double)axes->x_min / grid_step);
    first_y = ceil((double)axes->y_min / grid_step);
    count_x = floor((double)axes->x_max / grid_step) - first_x + 1;
    count_y = floor((double)axes->y_max / grid_step) - first_y + 1;
    if(!isfinite(first_x) || !isfinite(first_y) || fabs(first_x) > 1e12 || fabs(first_y) > 1e12 ||
        !isfinite(count_x) || !isfinite(count_y) || count_x < 0 || count_y < 0 ||
        count_x > 10000 || count_y > 10000) {return(0);}
    for(int axis = 0; axis < 2; axis++) {
        double first = axis ? first_y : first_x;
        int count = (int)(axis ? count_y : count_x);

        for(int i = 0; i < count; i++) {
            double value = (first + i) * grid_step;
            ManicVec2 a, b;

            if(!axes_point(axes, axis ? axes->x_min : value, axis ? value : axes->y_min, &a) ||
                !axes_point(axes, axis ? axes->x_max : value, axis ? value : axes->y_max, &b) ||
                !path_move_to(&grid, a) || !path_line_to(&grid, b)) {goto cleanup;}
        }
    }
    if(!scene_axes(scene, axes, position, tick_step, axis_color)) {goto cleanup;}
    if(grid.count) {
        id = scene_add(scene, path_mobject(&grid, (ManicVec2){0}, grid_color, (ManicColor){0}, 1));
        if(id < 0 || !scene_set_parent(scene, id, axes->group) || !scene_set_layer(scene, id, -1)) {
            if(id >= 0) {scene_remove(scene, id);}
            scene_remove(scene, axes->group);
            goto cleanup;
        }
    }
    result = 1;

cleanup:
    path_destroy(&grid);
    return(result);
}
