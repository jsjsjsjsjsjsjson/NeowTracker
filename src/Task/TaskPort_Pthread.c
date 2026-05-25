#include "TaskPort.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <time.h>

struct nt_task_port {
    pthread_t thread;
    int joined;
};

static void *nt_task_pthread_entry(void *arg) {
    nt_task_port_run((nt_task_t *)arg);
    return NULL;
}

static uint32_t nt_task_elapsed_ms(uint32_t start_ms, uint32_t now_ms) {
    return now_ms - start_ms;
}

nt_result_t nt_task_port_create(nt_task_t *task, const nt_task_attr_t *attr) {
    pthread_attr_t pthread_attr;
    struct nt_task_port *port;
    int attr_ready = 0;
    int rc;

    if (!task || !attr) return NT_TASK_ERR_INVALID;

    port = (struct nt_task_port *)calloc(1u, sizeof(*port));
    if (!port) return NT_TASK_ERR_NOMEM;

    rc = pthread_attr_init(&pthread_attr);
    if (rc == 0) {
        attr_ready = 1;
        if (attr->stack_size > 0u) {
            rc = pthread_attr_setstacksize(&pthread_attr, attr->stack_size);
            if (rc != 0) {
                pthread_attr_destroy(&pthread_attr);
                free(port);
                return NT_TASK_ERR_INVALID;
            }
        }
    }

    nt_task_port_set_data(task, port);

    rc = pthread_create(&port->thread,
                        attr_ready ? &pthread_attr : NULL,
                        nt_task_pthread_entry,
                        task);

    if (attr_ready) {
        pthread_attr_destroy(&pthread_attr);
    }

    if (rc != 0) {
        nt_task_port_set_data(task, NULL);
        free(port);
        return (rc == ENOMEM) ? NT_TASK_ERR_NOMEM : NT_TASK_ERR_PORT;
    }

    return NT_TASK_OK;
}

nt_result_t nt_task_port_join(nt_task_t *task, uint32_t timeout_ms) {
    struct nt_task_port *port = nt_task_port_get_data(task);
    uint32_t start_ms;

    if (!task || !port) return NT_TASK_ERR_INVALID;
    if (port->joined) return NT_TASK_OK;

    if (timeout_ms == NT_TASK_WAIT_FOREVER) {
        if (pthread_join(port->thread, NULL) != 0) return NT_TASK_ERR_PORT;
        port->joined = 1;
        return NT_TASK_OK;
    }

    start_ms = nt_task_port_now_ms();
    while (!nt_task_is_finished(task)) {
        if (timeout_ms == 0u ||
            nt_task_elapsed_ms(start_ms, nt_task_port_now_ms()) >= timeout_ms) {
            return NT_TASK_ERR_TIMEOUT;
        }
        nt_task_port_sleep_ms(1u);
    }

    if (pthread_join(port->thread, NULL) != 0) return NT_TASK_ERR_PORT;
    port->joined = 1;
    return NT_TASK_OK;
}

void nt_task_port_destroy(nt_task_t *task) {
    struct nt_task_port *port = nt_task_port_get_data(task);
    if (!port) return;
    free(port);
    nt_task_port_set_data(task, NULL);
}

void nt_task_port_sleep_ms(uint32_t ms) {
    struct timespec ts;

    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;

    while (nanosleep(&ts, &ts) != 0 && errno == EINTR) {}
}

void nt_task_port_yield(void) {
    sched_yield();
}

uint32_t nt_task_port_now_ms(void) {
    struct timespec ts;

#if defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif

    return (uint32_t)(((uint64_t)ts.tv_sec * 1000u) + ((uint64_t)ts.tv_nsec / 1000000u));
}
