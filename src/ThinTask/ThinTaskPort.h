#ifndef THINTASK_PORT_H
#define THINTASK_PORT_H

#include "ThinTask.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tt_task_port tt_task_port_t;

tt_result_t tt_task_port_create(tt_task_t *task, const tt_task_attr_t *attr);
tt_result_t tt_task_port_join(tt_task_t *task, uint32_t timeout_ms);
void tt_task_port_destroy(tt_task_t *task);

void tt_task_port_sleep_ms(uint32_t ms);
void tt_task_port_yield(void);
uint32_t tt_task_port_now_ms(void);

void tt_task_port_run(tt_task_t *task);

void tt_task_port_set_data(tt_task_t *task, tt_task_port_t *port);
tt_task_port_t *tt_task_port_get_data(const tt_task_t *task);
const char *tt_task_port_get_name(const tt_task_t *task);

#ifdef __cplusplus
}
#endif

#endif /* THINTASK_PORT_H */
