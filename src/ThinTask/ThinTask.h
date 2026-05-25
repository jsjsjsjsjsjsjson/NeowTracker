#ifndef THINTASK_H
#define THINTASK_H

/*
    ThinTask - small cross-platform task wrapper.

    Compile ThinTask.c with exactly one ThinTaskPort_*.c file. The public API owns only
    task lifecycle, cooperative stop requests, and basic time helpers; scheduler
    and RTOS specifics stay in the selected port file.
*/

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TT_VERSION_MAJOR 0
#define TT_VERSION_MINOR 1
#define TT_VERSION_PATCH 0

#define TT_WAIT_FOREVER ((uint32_t)0xffffffffu)
#define TT_NAME_MAX 32u

typedef enum tt_result {
    TT_OK = 0,
    TT_ERR_INVALID = -1,
    TT_ERR_NOMEM = -2,
    TT_ERR_STATE = -3,
    TT_ERR_TIMEOUT = -4,
    TT_ERR_PORT = -5
} tt_result_t;

typedef struct tt_task tt_task_t;

typedef void (*tt_task_entry_t)(tt_task_t *task, void *user);

typedef struct tt_task_attr {
    const char *name;      /* NULL: "tt_task" */
    size_t stack_size;     /* bytes; 0 lets the port choose its default */
    int priority;          /* port-defined; pthread port ignores the default 0 */
    uint32_t flags;        /* reserved */
    void *port_user;       /* optional port-specific config */
} tt_task_attr_t;

const char *tt_task_version(void);
const char *tt_task_result_string(tt_result_t result);

void tt_task_attr_default(tt_task_attr_t *attr);

tt_result_t tt_task_create(tt_task_t **out_task,
                           tt_task_entry_t entry,
                           void *user,
                           const tt_task_attr_t *attr);

void tt_task_request_stop(tt_task_t *task);
int tt_task_stop_requested(const tt_task_t *task);
int tt_task_is_finished(const tt_task_t *task);

tt_result_t tt_task_join(tt_task_t *task, uint32_t timeout_ms);
void tt_task_destroy(tt_task_t *task);

const char *tt_task_name(const tt_task_t *task);
void *tt_task_user(const tt_task_t *task);

void tt_task_sleep_ms(uint32_t ms);
void tt_task_yield(void);
uint32_t tt_task_now_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* THINTASK_H */
