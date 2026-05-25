#include "ThinTask.h"
#include "ThinTaskPort.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

struct tt_task {
    tt_task_entry_t entry;
    void *user;
    char name[TT_NAME_MAX];
    tt_task_port_t *port;
    atomic_bool stop_requested;
    atomic_bool finished;
    atomic_bool joined;
};

const char *tt_task_version(void) {
    return "0.1.0";
}

const char *tt_task_result_string(tt_result_t result) {
    switch (result) {
        case TT_OK: return "ok";
        case TT_ERR_INVALID: return "invalid argument";
        case TT_ERR_NOMEM: return "out of memory";
        case TT_ERR_STATE: return "invalid state";
        case TT_ERR_TIMEOUT: return "timeout";
        case TT_ERR_PORT: return "port error";
        default: return "unknown";
    }
}

void tt_task_attr_default(tt_task_attr_t *attr) {
    if (!attr) return;
    memset(attr, 0, sizeof(*attr));
    attr->name = "tt_task";
}

static void tt_task_copy_name(char *dst, const char *src) {
    const char *name = (src && src[0]) ? src : "tt_task";
    strncpy(dst, name, TT_NAME_MAX - 1u);
    dst[TT_NAME_MAX - 1u] = '\0';
}

tt_result_t tt_task_create(tt_task_t **out_task,
                           tt_task_entry_t entry,
                           void *user,
                           const tt_task_attr_t *attr) {
    tt_task_attr_t local_attr;
    tt_task_t *task;
    tt_result_t result;

    if (!out_task || !entry) return TT_ERR_INVALID;
    *out_task = NULL;

    if (attr) {
        local_attr = *attr;
    } else {
        tt_task_attr_default(&local_attr);
    }

    task = (tt_task_t *)calloc(1u, sizeof(*task));
    if (!task) return TT_ERR_NOMEM;

    task->entry = entry;
    task->user = user;
    tt_task_copy_name(task->name, local_attr.name);
    atomic_init(&task->stop_requested, 0);
    atomic_init(&task->finished, 0);
    atomic_init(&task->joined, 0);

    result = tt_task_port_create(task, &local_attr);
    if (result != TT_OK) {
        free(task);
        return result;
    }

    *out_task = task;
    return TT_OK;
}

void tt_task_request_stop(tt_task_t *task) {
    if (!task) return;
    atomic_store_explicit(&task->stop_requested, 1, memory_order_release);
}

int tt_task_stop_requested(const tt_task_t *task) {
    if (!task) return 1;
    return atomic_load_explicit(&task->stop_requested, memory_order_acquire) ? 1 : 0;
}

int tt_task_is_finished(const tt_task_t *task) {
    if (!task) return 1;
    return atomic_load_explicit(&task->finished, memory_order_acquire) ? 1 : 0;
}

tt_result_t tt_task_join(tt_task_t *task, uint32_t timeout_ms) {
    tt_result_t result;

    if (!task) return TT_ERR_INVALID;
    if (atomic_load_explicit(&task->joined, memory_order_acquire)) return TT_OK;

    result = tt_task_port_join(task, timeout_ms);
    if (result == TT_OK) {
        atomic_store_explicit(&task->joined, 1, memory_order_release);
    }
    return result;
}

void tt_task_destroy(tt_task_t *task) {
    if (!task) return;

    if (!atomic_load_explicit(&task->joined, memory_order_acquire)) {
        tt_task_request_stop(task);
        (void)tt_task_join(task, TT_WAIT_FOREVER);
    }

    tt_task_port_destroy(task);
    free(task);
}

const char *tt_task_name(const tt_task_t *task) {
    if (!task) return "";
    return task->name;
}

void *tt_task_user(const tt_task_t *task) {
    if (!task) return NULL;
    return task->user;
}

void tt_task_sleep_ms(uint32_t ms) {
    tt_task_port_sleep_ms(ms);
}

void tt_task_yield(void) {
    tt_task_port_yield();
}

uint32_t tt_task_now_ms(void) {
    return tt_task_port_now_ms();
}

void tt_task_port_run(tt_task_t *task) {
    if (!task) return;
    task->entry(task, task->user);
    atomic_store_explicit(&task->finished, 1, memory_order_release);
}

void tt_task_port_set_data(tt_task_t *task, tt_task_port_t *port) {
    if (!task) return;
    task->port = port;
}

tt_task_port_t *tt_task_port_get_data(const tt_task_t *task) {
    if (!task) return NULL;
    return task->port;
}

const char *tt_task_port_get_name(const tt_task_t *task) {
    return tt_task_name(task);
}
