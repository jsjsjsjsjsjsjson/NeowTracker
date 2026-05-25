#include "TaskPort.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <stdlib.h>

struct nt_task_port {
    TaskHandle_t handle;
    SemaphoreHandle_t done;
    int joined;
};

static TickType_t nt_task_ms_to_ticks(uint32_t timeout_ms) {
    if (timeout_ms == NT_TASK_WAIT_FOREVER) return portMAX_DELAY;
    return pdMS_TO_TICKS(timeout_ms);
}

static void nt_task_freertos_entry(void *arg) {
    nt_task_t *task = (nt_task_t *)arg;
    struct nt_task_port *port = nt_task_port_get_data(task);

    nt_task_port_run(task);

    if (port && port->done) {
        xSemaphoreGive(port->done);
    }

    vTaskDelete(NULL);
}

nt_result_t nt_task_port_create(nt_task_t *task, const nt_task_attr_t *attr) {
    struct nt_task_port *port;
    UBaseType_t priority;
    configSTACK_DEPTH_TYPE stack_words;
    BaseType_t rc;

    if (!task || !attr) return NT_TASK_ERR_INVALID;

    port = (struct nt_task_port *)calloc(1u, sizeof(*port));
    if (!port) return NT_TASK_ERR_NOMEM;

    port->done = xSemaphoreCreateBinary();
    if (!port->done) {
        free(port);
        return NT_TASK_ERR_NOMEM;
    }

    priority = (attr->priority > 0) ? (UBaseType_t)attr->priority : (UBaseType_t)tskIDLE_PRIORITY;
    stack_words = (attr->stack_size > 0u)
        ? (configSTACK_DEPTH_TYPE)((attr->stack_size + sizeof(StackType_t) - 1u) / sizeof(StackType_t))
        : (configSTACK_DEPTH_TYPE)configMINIMAL_STACK_SIZE;

    nt_task_port_set_data(task, port);

    rc = xTaskCreate(nt_task_freertos_entry,
                     nt_task_port_get_name(task),
                     stack_words,
                     task,
                     priority,
                     &port->handle);

    if (rc != pdPASS) {
        vSemaphoreDelete(port->done);
        nt_task_port_set_data(task, NULL);
        free(port);
        return NT_TASK_ERR_PORT;
    }

    return NT_TASK_OK;
}

nt_result_t nt_task_port_join(nt_task_t *task, uint32_t timeout_ms) {
    struct nt_task_port *port = nt_task_port_get_data(task);

    if (!task || !port || !port->done) return NT_TASK_ERR_INVALID;
    if (port->joined) return NT_TASK_OK;

    if (xSemaphoreTake(port->done, nt_task_ms_to_ticks(timeout_ms)) != pdTRUE) {
        return NT_TASK_ERR_TIMEOUT;
    }

    port->joined = 1;
    port->handle = NULL;
    return NT_TASK_OK;
}

void nt_task_port_destroy(nt_task_t *task) {
    struct nt_task_port *port = nt_task_port_get_data(task);
    if (!port) return;

    if (!port->joined && port->handle) {
        vTaskDelete(port->handle);
        port->handle = NULL;
    }

    if (port->done) {
        vSemaphoreDelete(port->done);
    }

    free(port);
    nt_task_port_set_data(task, NULL);
}

void nt_task_port_sleep_ms(uint32_t ms) {
    if (ms == 0u) {
        taskYIELD();
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(ms));
}

void nt_task_port_yield(void) {
    taskYIELD();
}

uint32_t nt_task_port_now_ms(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}
