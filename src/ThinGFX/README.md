# ThinGFX

ThinGFX is a C++ grayscale graphics and layer compositor library. The public API is platform-neutral; the selected port is chosen at build time.

## Build-time port selection

```cmake
set(THINGFX_PORT sdl CACHE STRING "" FORCE)          # desktop SDL test
# set(THINGFX_PORT esp32_ssd1306 CACHE STRING "" FORCE) # ESP32 SSD1306
# set(THINGFX_PORT none CACHE STRING "" FORCE)          # no default port

add_subdirectory(third_party/ThinGFX)
target_link_libraries(my_app PRIVATE ThinGFX)
```

Application code includes only:

```cpp
#include "thingfx/ThinGFX.hpp"
```

No port-specific header is needed.

## SDL example

```cpp
thingfx::PortConfig port;
port.title = "ThinGFX";
port.scale = 4;
port.format = thingfx::PixelFormat::Mono1;

thingfx::Gui gui(128, 64, port);

thingfx::WindowConfig cfg;
cfg.kind = thingfx::NodeKind::Layer;
cfg.frame = {0, 0, 128, 64};
cfg.ownsCanvas = true;
cfg.canvasFormat = thingfx::PixelFormat::Mono1;
cfg.showTitle = false;
cfg.border = thingfx::WindowBorder::None;

auto layer = gui.createLayer(cfg);
auto gfx = layer.draw();
gfx.fillScreen(0);
gfx.setCursor(0, 0);
gfx.setTextColor(255, 0);
gfx.print("Hello");
layer.commit();

while (gui.pump()) {
    // Port event processing. Rendering is driven by layer commits when the daemon is enabled.
}
```

## ESP32 SSD1306

For the ESP32 SSD1306 default port, pass the `esp_lcd_panel_handle_t` as `PortConfig::native`:

```cpp
thingfx::PortConfig port;
port.format = thingfx::PixelFormat::Mono1;
port.native = panel_handle;
port.daemonStackBytes = 4096;
port.daemonPriority = 5;

thingfx::Gui gui(128, 64, port);
```
