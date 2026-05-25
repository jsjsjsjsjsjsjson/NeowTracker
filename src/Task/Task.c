#include "Task.h"
#include "TaskPort.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

struct nt_task {
    nt_task_entry_t entry;
    void *user;
    char name[NT_TASK_NAME_MAX];
    nt_task_port_t *port;
    atomic_bool stop_requested;
    atomic_bool finished;
    atomic_bool joined;
};

const char *nt_task_version(void) {
    return "0.1.0";
}

const char *nt_task_result_string(nt_result_t result) {
    switch (result) {
        case NT_TASK_OK: return "ok";
        case NT_TASK_ERR_INVALID: return "invalid argument";
        case NT_TASK_ERR_NOMEM: return "out of memory";
        case NT_TASK_ERR_STATE: return "invalid state";
        case NT_TASK_ERR_TIMEOUT: return "timeout";
        case NT_TASK_ERR_PORT: return "port error";
        default: return "unknown";
    }
}

void nt_task_attr_default(nt_task_attr_t *attr) {
    if (!attr) return;
    memset(attr, 0, sizeof(*attr));
    attr->name = "nt_task";
}

static void nt_task_copy_name(char *dst, const char *src) {
    const char *name = (src && src[0]) ? src : "nt_task";
    strncpy(dst, name, NT_TASK_NAME_MAX - 1u);
    dst[NT_TASK_NAME_MAX - 1u] = '\0';
}

nt_result_t nt_task_create(nt_task_t **out_task,
                           nt_task_entry_t entry,
                           void *user,
                           const nt_task_attr_t *attr) {
    nt_task_attr_t local_attr;
    nt_task_t *task;
    nt_result_t result;

    if (!out_task || !entry) return NT_TASK_ERR_INVALID;
    *out_task = NULL;

    if (attr) {
        local_attr = *attr;
    } else {
        nt_task_attr_default(&local_attr);
    }

    task = (nt_task_t *)calloc(1u, sizeof(*task));
    if (!task) return NT_TASK_ERR_NOMEM;

    task->entry = entry;
    task->user = user;
    nt_task_copy_name(task->name, local_attr.name);
    atomic_init(&task->stop_requested, 0);
    atomic_init(&task->finished, 0);
    atomic_init(&task->joined, 0);

    result = nt_task_port_create(task, &local_attr);
    if (result != NT_TASK_OK) {
        free(task);
        return result;
    }

    *out_task = task;
    return NT_TASK_OK;
}

void nt_task_request_stop(nt_task_t *task) {
    if (!task) return;
    atomic_store_explicit(&task->stop_requested, 1, memory_order_release);
}

int nt_task_stop_requested(const nt_task_t *task) {
    if (!task) return 1;
    return atomic_load_explicit(&task->stop_requested, memory_order_acquire) ? 1 : 0;
}

int nt_task_is_finished(const nt_task_t *task) {
    if (!task) return 1;
    return atomic_load_explicit(&task->finished, memory_order_acquire) ? 1 : 0;
}

nt_result_t nt_task_join(nt_task_t *task, uint32_t timeout_ms) {
    nt_result_t result;

    if (!task) return NT_TASK_ERR_INVALID;
    if (atomic_load_explicit(&task->joined, memory_order_acquire)) return NT_TASK_OK;

    result = nt_task_port_join(task, timeout_ms);
    if (result == NT_TASK_OK) {
        atomic_store_explicit(&task->joined, 1, memory_order_release);
    }
    return result;
}

void nt_task_destroy(nt_task_t *task) {
    if (!task) return;

    if (!atomic_load_explicit(&task->joined, memory_order_acquire)) {
        nt_task_request_stop(task);
        (void)nt_task_join(task, NT_TASK_WAIT_FOREVER);
    }

    nt_task_port_destroy(task);
    free(task);
}

const char *nt_task_name(const nt_task_t *task) {
    if (!task) return "";
    return task->name;
}

void *nt_task_user(const nt_task_t *task) {
    if (!task) return NULL;
    return task->user;
}

void nt_task_sleep_ms(uint32_t ms) {
    nt_task_port_sleep_ms(ms);
}

void nt_task_yield(void) {
    nt_task_port_yield();
}

uint32_t nt_task_now_ms(void) {
    return nt_task_port_now_ms();
}

void nt_task_port_run(nt_task_t *task) {
    if (!task) return;
    task->entry(task, task->user);
    atomic_store_explicit(&task->finished, 1, memory_order_release);
}

void nt_task_port_set_data(nt_task_t *task, nt_task_port_t *port) {
    if (!task) return;
    task->port = port;
}

nt_task_port_t *nt_task_port_get_data(const nt_task_t *task) {
    if (!task) return NULL;
    return task->port;
}

const char *nt_task_port_get_name(const nt_task_t *task) {
    return nt_task_name(task);
}
