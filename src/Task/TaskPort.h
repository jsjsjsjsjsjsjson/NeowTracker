#ifndef NEOW_TASK_PORT_H
#define NEOW_TASK_PORT_H

#include "Task.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct nt_task_port nt_task_port_t;

nt_result_t nt_task_port_create(nt_task_t *task, const nt_task_attr_t *attr);
nt_result_t nt_task_port_join(nt_task_t *task, uint32_t timeout_ms);
void nt_task_port_destroy(nt_task_t *task);

void nt_task_port_sleep_ms(uint32_t ms);
void nt_task_port_yield(void);
uint32_t nt_task_port_now_ms(void);

void nt_task_port_run(nt_task_t *task);

void nt_task_port_set_data(nt_task_t *task, nt_task_port_t *port);
nt_task_port_t *nt_task_port_get_data(const nt_task_t *task);
const char *nt_task_port_get_name(const nt_task_t *task);

#ifdef __cplusplus
}
#endif

#endif /* NEOW_TASK_PORT_H */
