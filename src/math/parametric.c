#include "internal.h"
#include <float.h>

#define GRAPH_BUDGET 4000000

typedef struct {
    double t;
    double x;
    double y;
    int valid;
} ParamSample;

typedef struct {
    const MathAxes *axes;
    MathParametric function;
    void *context;
    GraphSampling sampling;
    GraphClip clip;
    VectorPath path;
    int drawing;
    int budget;
} ParamBuilder;

int axes_coordinates(const MathAxes *axes, ManicVec2 point, double *x, double *y)
{
    ManicVec2 check;
    double cx, cy;

    if(x == NULL || y == NULL || !vector_valid(point) ||
        !axes_point(axes, axes ? axes->x_min : 0, axes ? axes->y_min : 0, &check)) {return(0);}
    cx = (point.x / (double)axes->width + 0.5) * ((double)axes->x_max - axes->x_min) + axes->x_min;
    cy = (point.y / (double)axes->height + 0.5) * ((double)axes->y_max - axes->y_min) + axes->y_min;
    if(!isfinite(cx) || !isfinite(cy)) {return(0);}
    *x = cx;
    *y = cy;
    return(1);
}

GraphClip graph_clip_axes(const MathAxes *axes)
{
    if(axes == NULL) {return((GraphClip){0});}
    return((GraphClip){1, axes->x_min, axes->x_max, axes->y_min, axes->y_max});
}

static ParamSample param_sample(ParamBuilder *builder, double t)
{
    ParamSample sample = {.t = t};

    sample.valid = builder->function(t, builder->context, &sample.x, &sample.y) &&
        isfinite(sample.x) && isfinite(sample.y);
    return(sample);
}

static int clip_edge(double p, double q, double *low, double *high)
{
    double t;

    if(p == 0) {return(q >= 0);}
    t = q / p;
    if(p < 0) {
        if(t > *high) {return(0);}
        if(t > *low) {*low = t;}
    }
    else {
        if(t < *low) {return(0);}
        if(t < *high) {*high = t;}
    }
    return(1);
}

static int param_line(ParamBuilder *builder, ParamSample a, ParamSample b)
{
    double dx = b.x - a.x, dy = b.y - a.y;
    double low = 0, high = 1;
    ManicVec2 p, q;

    if(!a.valid || !b.valid || !isfinite(dx) || !isfinite(dy)) {builder->drawing = 0; return(1);}
    if(builder->clip.enabled &&
        (!clip_edge(-dx, a.x - builder->clip.x_min, &low, &high) ||
         !clip_edge(dx, builder->clip.x_max - a.x, &low, &high) ||
         !clip_edge(-dy, a.y - builder->clip.y_min, &low, &high) ||
         !clip_edge(dy, builder->clip.y_max - a.y, &low, &high))) {builder->drawing = 0; return(1);}
    if(!axes_point(builder->axes, a.x + dx * low, a.y + dy * low, &p) ||
        !axes_point(builder->axes, a.x + dx * high, a.y + dy * high, &q)) {builder->drawing = 0; return(1);}
    if((!builder->drawing || low > 0) && !path_move_to(&builder->path, p)) {return(0);}
    if(!path_line_to(&builder->path, q)) {return(0);}
    builder->drawing = high >= 1;
    return(1);
}

static int param_refine(ParamBuilder *builder, ParamSample a, ParamSample b, int depth)
{
    double middle = a.t + (b.t - a.t) / 2;
    double xr = (double)builder->axes->x_max - builder->axes->x_min;
    double yr = (double)builder->axes->y_max - builder->axes->y_min;
    ParamSample m;
    double deviation = INFINITY, jump = INFINITY;
    int split;

    if(builder->budget-- <= 0) {return(0);}
    if(middle <= a.t || middle >= b.t) {builder->drawing = 0; return(1);}
    m = param_sample(builder, middle);
    if(a.valid && b.valid && m.valid) {
        deviation = hypot((m.x - (a.x * 0.5 + b.x * 0.5)) / xr,
            (m.y - (a.y * 0.5 + b.y * 0.5)) / yr);
        jump = hypot((b.x - a.x) / xr, (b.y - a.y) / yr);
    }
    split = !isfinite(deviation) || deviation > builder->sampling.tolerance || jump > 0.5;
    if(split && depth < builder->sampling.max_depth) {
        return(param_refine(builder, a, m, depth + 1) && param_refine(builder, m, b, depth + 1));
    }
    if(split && (!isfinite(deviation) || deviation > builder->sampling.tolerance * 4 || jump > 0.5)) {
        builder->drawing = 0;
        return(1);
    }
    return(param_line(builder, a, b));
}

int path_parametric_graph(VectorPath *output, const MathAxes *axes, MathParametric function, void *context,
    double minimum, double maximum, int samples, GraphSampling sampling, GraphClip clip,
    const double *breaks, size_t break_count)
{
    ParamBuilder builder = {0};
    ParamSample previous;
    ManicVec2 validation;
    size_t next_break = 0;
    int result = 0;

    if(output == NULL || output->segments != NULL || output->count != 0 || function == NULL ||
        !isfinite(minimum) || !isfinite(maximum) || maximum <= minimum || !isfinite(maximum - minimum) ||
        samples < 2 || samples > 1000000 || sampling.max_depth < 0 || sampling.max_depth > 20 ||
        !isfinite(sampling.tolerance) || sampling.tolerance <= 0 || (break_count && !breaks) ||
        !axes_point(axes, axes ? axes->x_min : 0, axes ? axes->y_min : 0, &validation) ||
        (clip.enabled && (!isfinite(clip.x_min) || !isfinite(clip.x_max) ||
            !isfinite(clip.y_min) || !isfinite(clip.y_max) || clip.x_max <= clip.x_min || clip.y_max <= clip.y_min))) {return(0);}
    for(size_t i = 0; i < break_count; i++) {
        if(!isfinite(breaks[i]) || breaks[i] <= minimum || breaks[i] >= maximum ||
            (i && breaks[i] <= breaks[i - 1])) {return(0);}
    }
    builder.axes = axes;
    builder.function = function;
    builder.context = context;
    builder.sampling = sampling;
    builder.clip = clip;
    builder.budget = GRAPH_BUDGET;
    previous = param_sample(&builder, minimum);
    for(int i = 1; i < samples; i++) {
        double t = minimum + (maximum - minimum) * i / (samples - 1);
        ParamSample current;

        while(next_break < break_count && breaks[next_break] <= t) {
            double value = breaks[next_break++];
            double before = nextafter(value, -INFINITY);
            double after = nextafter(value, INFINITY);

            if(before > previous.t && !param_refine(&builder, previous, param_sample(&builder, before), 0)) {goto cleanup;}
            builder.drawing = 0;
            previous = param_sample(&builder, after);
        }
        if(t > previous.t) {
            current = param_sample(&builder, t);
            if(!param_refine(&builder, previous, current, 0)) {goto cleanup;}
            previous = current;
        }
    }
    if(builder.path.count == 0) {goto cleanup;}
    *output = builder.path;
    builder.path = (VectorPath){0};
    result = 1;

cleanup:
    path_destroy(&builder.path);
    return(result);
}

int scene_parametric_graph(Scene *scene, const MathAxes *axes, MathParametric function, void *context,
    double minimum, double maximum, int samples, GraphSampling sampling, GraphClip clip,
    const double *breaks, size_t break_count, ManicColor color)
{
    VectorPath path = {0};
    int group, id = -1;

    if(scene == NULL || axes == NULL || (group = scene_slot(scene, axes->group)) < 0 ||
        scene->mobjects[group].removed || scene->mobjects[group].type != MOBJECT_GROUP ||
        !path_parametric_graph(&path, axes, function, context, minimum, maximum, samples,
            sampling, clip, breaks, break_count)) {return(-1);}
    id = scene_add(scene, path_mobject(&path, (ManicVec2){0}, color, (ManicColor){0}, 3));
    if(id >= 0 && !scene_set_parent(scene, id, axes->group)) {scene_remove(scene, id); id = -1;}
    path_destroy(&path);
    return(id);
}

typedef struct {
    MathFunction function;
    void *context;
} FunctionAdapter;

static int function_parametric(double t, void *context, double *x, double *y)
{
    FunctionAdapter *adapter = context;

    *x = t;
    *y = adapter->function(t, adapter->context);
    return(1);
}

int scene_function_graph_config(Scene *scene, const MathAxes *axes, MathFunction function, void *context,
    int samples, GraphSampling sampling, GraphClip clip, const double *breaks, size_t break_count, ManicColor color)
{
    FunctionAdapter adapter = {function, context};

    if(axes == NULL || function == NULL) {return(-1);}
    return(scene_parametric_graph(scene, axes, function_parametric, &adapter, axes->x_min, axes->x_max,
        samples, sampling, clip, breaks, break_count, color));
}
