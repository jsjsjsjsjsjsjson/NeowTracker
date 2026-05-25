#ifndef NEOW_TASK_H
#define NEOW_TASK_H

/*
    Neow Task - small cross-platform task wrapper.

    Compile Task.c with exactly one TaskPort_*.c file. The public API owns only
    task lifecycle, cooperative stop requests, and basic time helpers; scheduler
    and RTOS specifics stay in the selected port file.
*/

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NT_TASK_VERSION_MAJOR 0
#define NT_TASK_VERSION_MINOR 1
#define NT_TASK_VERSION_PATCH 0

#define NT_TASK_WAIT_FOREVER ((uint32_t)0xffffffffu)
#define NT_TASK_NAME_MAX 32u

typedef enum nt_result {
    NT_TASK_OK = 0,
    NT_TASK_ERR_INVALID = -1,
    NT_TASK_ERR_NOMEM = -2,
    NT_TASK_ERR_STATE = -3,
    NT_TASK_ERR_TIMEOUT = -4,
    NT_TASK_ERR_PORT = -5
} nt_result_t;

typedef struct nt_task nt_task_t;

typedef void (*nt_task_entry_t)(nt_task_t *task, void *user);

typedef struct nt_task_attr {
    const char *name;      /* NULL: "nt_task" */
    size_t stack_size;     /* bytes; 0 lets the port choose its default */
    int priority;          /* port-defined; pthread port ignores the default 0 */
    uint32_t flags;        /* reserved */
    void *port_user;       /* optional port-specific config */
} nt_task_attr_t;

const char *nt_task_version(void);
const char *nt_task_result_string(nt_result_t result);

void nt_task_attr_default(nt_task_attr_t *attr);

nt_result_t nt_task_create(nt_task_t **out_task,
                           nt_task_entry_t entry,
                           void *user,
                           const nt_task_attr_t *attr);

void nt_task_request_stop(nt_task_t *task);
int nt_task_stop_requested(const nt_task_t *task);
int nt_task_is_finished(const nt_task_t *task);

nt_result_t nt_task_join(nt_task_t *task, uint32_t timeout_ms);
void nt_task_destroy(nt_task_t *task);

const char *nt_task_name(const nt_task_t *task);
void *nt_task_user(const nt_task_t *task);

void nt_task_sleep_ms(uint32_t ms);
void nt_task_yield(void);
uint32_t nt_task_now_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* NEOW_TASK_H */
