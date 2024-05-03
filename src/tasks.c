#include "tasks.h"
#include "interpolators.h"

#include "raymath.h"

Task_VTable task_vtable = {0};
size_t TASK_MOVE_V2_TAG = 0;
size_t TASK_MOVE_V4_TAG = 0;
size_t TASK_SEQ_TAG = 0;
size_t TASK_GROUP_TAG = 0;

void task_reset(Task task, Env env)
{
    task_vtable.items[task.tag].reset(env, task.data);
}

bool task_update(Task task, Env env)
{
    return task_vtable.items[task.tag].update(env, task.data);
}

size_t task_vtable_register(Arena *a, Task_Funcs funcs)
{
    size_t index = task_vtable.count;
    arena_da_append(a, &task_vtable, funcs);
    return index;
}

void task_vtable_rebuild(Arena *a)
{
    memset(&task_vtable, 0, sizeof(task_vtable));

    TASK_MOVE_V2_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = task_move_v2_update,
        .reset = task_move_v2_reset,
    });
    TASK_MOVE_V4_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = task_move_v4_update,
        .reset = task_move_v4_reset,
    });
    TASK_SEQ_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = task_seq_update,
        .reset = task_seq_reset,
    });
    TASK_GROUP_TAG = task_vtable_register(a, (Task_Funcs) {
        .update = task_group_update,
        .reset = task_group_reset,
    });
}

void task_move_v2_reset(Env env, void *raw_data)
{
    (void) env;
    Move_V2_Data *data = raw_data;
    data->t = 0.0f;
    data->init = false;
}

bool task_move_v2_update(Env env, void *raw_data)
{
    Move_V2_Data *data = raw_data;
    if (data->t >= 1.0f) return true; // task is done

    if (!data->init) {
        // First update of the task
        if (data->value) data->start = *data->value;
        data->init = true;
    }

    data->t = (data->t*data->duration + env.delta_time)/data->duration;
    if (data->value) *data->value = Vector2Lerp(data->start, data->target, smoothstep(data->t));
    return data->t >= 1.0f;
}

Task task_move_v2(Arena *a, Vector2 *value, Vector2 target, float duration)
{
    Move_V2_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->value = value;
    data->target = target;
    data->duration = duration;
    return (Task) {
        .tag = TASK_MOVE_V2_TAG,
        .data = data,
    };
}

void task_move_v4_reset(Env env, void *raw_data)
{
    (void) env;
    Move_V4_Data *data = raw_data;
    data->t = 0.0f;
    data->init = false;
}

bool task_move_v4_update(Env env, void *raw_data)
{
    Move_V4_Data *data = raw_data;
    if (data->t >= 1.0f) return true;

    if (!data->init) {
        // First update of the task
        if (data->value) data->start = *data->value;
        data->init = true;
    }

    data->t = (data->t*data->duration + env.delta_time)/data->duration;
    if (data->value) *data->value = QuaternionLerp(data->start, data->target, smoothstep(data->t));
    return data->t >= 1.0f;
}

Task task_move_v4(Arena *a, Vector4 *value, Color target, float duration)
{
    Move_V4_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));
    data->value = value;
    data->target = ColorNormalize(target);
    data->duration = duration;
    return (Task) {
        .tag = TASK_MOVE_V4_TAG,
        .data = data,
    };
}

void task_group_reset(Env env, void *raw_data)
{
    Group_Data *data = raw_data;
    for (size_t i = 0; i < data->tasks.count; ++i) {
        task_reset(data->tasks.items[i], env);
    }
}

bool task_group_update(Env env, void *raw_data)
{
    Group_Data *data = raw_data;
    bool finished = true;
    for (size_t i = 0; i < data->tasks.count; ++i) {
        Task it = data->tasks.items[i];
        if (!task_update(it, env)) {
            finished = false;
        }
    }
    return finished;
}

Task task_group_(Arena *a, ...)
{
    Group_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));

    va_list args;
    va_start(args, a);
    for (;;) {
        Task task = va_arg(args, Task);
        if (task.data == NULL) break;
        arena_da_append(a, &data->tasks, task);
    }
    va_end(args);

    return (Task) {
        .tag = TASK_GROUP_TAG,
        .data = data,
    };
}

void task_seq_reset(Env env, void *raw_data)
{
    (void) env;
    Seq_Data *data = raw_data;
    for (size_t i = 0; i < data->tasks.count; ++i) {
        Task it = data->tasks.items[i];
        task_reset(it, env);
    }
    data->it = 0;
}

bool task_seq_update(Env env, void *raw_data)
{
    Seq_Data *data = raw_data;
    if (data->it >= data->tasks.count) return true;

    Task task = data->tasks.items[data->it];
    if (task_update(task, env)) {
        data->it += 1;
    }

    return data->it >= data->tasks.count;
}

Task task_seq_(Arena *a, ...)
{
    Seq_Data *data = arena_alloc(a, sizeof(*data));
    memset(data, 0, sizeof(*data));

    va_list args;
    va_start(args, a);
    for (;;) {
        Task task = va_arg(args, Task);
        if (task.data == NULL) break;
        arena_da_append(a, &data->tasks, task);
    }
    va_end(args);

    return (Task) {
        .tag = TASK_SEQ_TAG,
        .data = data,
    };
}
