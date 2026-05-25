#include "../../src/ThinGFXInternal.hpp"

#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdlib.h>

namespace thingfx {
using namespace detail;

class ESP32SSD1306DefaultPort : public Port {
public:
    struct State {
        esp_lcd_panel_handle_t panel;
        SemaphoreHandle_t lock;
        SemaphoreHandle_t commit_sem;
        TaskHandle_t daemon_task;
        void (*entry)(void *);
        void *entry_arg;
        uint8_t *target_pixels;
        uint16_t width;
        uint16_t height;
        uint32_t daemon_stack_bytes;
        uint8_t daemon_priority;
        bool daemon_stop;
    };

    ESP32SSD1306DefaultPort(uint16_t width, uint16_t height, const PortConfig &config)
        : state_(0)
    {
        esp_lcd_panel_handle_t panel = static_cast<esp_lcd_panel_handle_t>(config.native);
        if (!panel || width == 0 || height == 0) return;
        state_ = static_cast<State *>(calloc(1, sizeof(State)));
        if (!state_) return;
        state_->panel = panel;
        state_->width = width;
        state_->height = height;
        state_->daemon_stack_bytes = config.daemonStackBytes ? config.daemonStackBytes : 4096;
        state_->daemon_priority = config.daemonPriority ? config.daemonPriority : 5;
        state_->lock = xSemaphoreCreateMutex();
        state_->commit_sem = xSemaphoreCreateBinary();
        size_t bytes = framebufferBytes(width, height, PixelFormat::Mono1);
        state_->target_pixels = static_cast<uint8_t *>(calloc(1, bytes));
        if (!state_->lock || !state_->commit_sem || !state_->target_pixels) return;
        tgfx_canvas_wrap(&target, width, height, width, PixelFormat::Mono1,
                         state_->target_pixels);
    }

    ~ESP32SSD1306DefaultPort() override
    {
        if (!state_) return;
        stopDaemon();
        if (state_->lock) vSemaphoreDelete(state_->lock);
        if (state_->commit_sem) vSemaphoreDelete(state_->commit_sem);
        free(state_->target_pixels);
        free(state_);
        state_ = 0;
    }

    bool valid() const override
    {
        return state_ && state_->panel && state_->target_pixels && target.pixels;
    }

    Result present(const CanvasBuffer &canvas, const Rect *dirty) override
    {
        (void)dirty;
        if (!valid() || canvas.format != PixelFormat::Mono1 || !canvas.pixels) {
            return Result::InvalidArgument;
        }
        esp_err_t err = esp_lcd_panel_draw_bitmap(state_->panel,
                                                  0, 0,
                                                  canvas.width, canvas.height,
                                                  canvas.pixels);
        return err == ESP_OK ? Result::Ok : Result::Unsupported;
    }

    Result startDaemon(void (*entry)(void *), void *arg) override
    {
        if (!state_ || !entry || state_->daemon_task) return Result::InvalidArgument;
        state_->entry = entry;
        state_->entry_arg = arg;
        state_->daemon_stop = false;
        BaseType_t ok = xTaskCreate(daemonTrampoline, "thingfx",
                                    state_->daemon_stack_bytes,
                                    state_, state_->daemon_priority,
                                    &state_->daemon_task);
        return ok == pdPASS ? Result::Ok : Result::NoMemory;
    }

    void stopDaemon() override
    {
        if (!state_ || !state_->daemon_task) return;
        state_->daemon_stop = true;
        xSemaphoreGive(state_->commit_sem);
        state_->daemon_task = NULL;
    }

    void notifyCommit() override
    {
        if (state_ && state_->commit_sem) xSemaphoreGive(state_->commit_sem);
    }

    bool waitCommit() override
    {
        if (!state_ || !state_->commit_sem) return false;
        xSemaphoreTake(state_->commit_sem, portMAX_DELAY);
        return !state_->daemon_stop;
    }

    bool waitCommitTimeout(uint32_t timeoutMs) override
    {
        if (!state_ || !state_->commit_sem) return false;
        xSemaphoreTake(state_->commit_sem, pdMS_TO_TICKS(timeoutMs));
        return !state_->daemon_stop;
    }

    void lock() override
    {
        if (state_ && state_->lock) xSemaphoreTake(state_->lock, portMAX_DELAY);
    }

    void unlock() override
    {
        if (state_ && state_->lock) xSemaphoreGive(state_->lock);
    }

private:
    static void daemonTrampoline(void *arg)
    {
        State *state = static_cast<State *>(arg);
        if (state && state->entry) {
            state->entry(state->entry_arg);
        }
        vTaskDelete(NULL);
    }

    State *state_;
};

Port *createDefaultPort(uint16_t width, uint16_t height, const PortConfig &config)
{
    return new ESP32SSD1306DefaultPort(width, height, config);
}

void destroyDefaultPort(Port *port)
{
    delete port;
}

} // namespace thingfx
