#ifndef THINGFX_THINGFX_HPP
#define THINGFX_THINGFX_HPP

#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace thingfx {

using Color = uint8_t;
using Handle = uint32_t;
constexpr Handle InvalidHandle = 0;
constexpr Color Black = 0;
constexpr Color White = 255;

inline Color gray(uint8_t value) { return value; }

enum class Result : int {
    Ok = 0,
    InvalidArgument = -1,
    NoMemory = -2,
    NotFound = -3,
    Unsupported = -4
};

enum class PixelFormat : uint8_t {
    Mono1 = 1,       // SSD1306-style vertical LSB 1bpp
    Gray4 = 2,       // 2 horizontal pixels per byte, high nibble first
    Gray8 = 3
};

enum class NodeKind : uint8_t {
    Layer = 0,
    Window = 1,
    Floating = 2,
    Widget = 3
};

enum class WindowBorder : uint8_t {
    None = 0,
    Rect = 1
};

enum class EventType : uint8_t {
    None = 0,
    Cancel = 4,
    KeyDown = 5,
    KeyUp = 6
};

enum class Key : uint32_t {
    Unknown = 0,
    Up = 0x100,
    Down = 0x101,
    Left = 0x102,
    Right = 0x103,
    Enter = 0x104,
    Back = 0x105,
    Escape = 0x106,
    Menu = 0x107,
    Home = 0x108,
    End = 0x109,
    PageUp = 0x10A,
    PageDown = 0x10B
};

enum KeyModifier : uint32_t {
    ModNone = 0,
    ModShift = 1u << 0,
    ModCtrl = 1u << 1,
    ModAlt = 1u << 2,
    ModMeta = 1u << 3
};

enum class AnimationProperty : uint8_t {
    X = 0,
    Y = 1,
    Opacity = 2
};

enum class AnimationCurve : uint8_t {
    Linear = 0,
    EaseOut = 1
};

enum class Easing : uint8_t {
    None = 0,
    Linear = 1,
    EaseIn = 2,
    EaseOut = 3,
    EaseInOut = 4,
    SmoothStep = 5
};

struct Rect {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
};

struct Transition {
    Easing easing;
    uint32_t durationMs;
    float speed;

    Transition(Easing e = Easing::None, uint32_t duration = 0, float speedScale = 1.0f)
        : easing(e), durationMs(duration), speed(speedScale) {}
};

struct CanvasBuffer {
    uint16_t width;
    uint16_t height;
    uint16_t stride;
    PixelFormat format;
    uint8_t *pixels;
    int16_t clip_x;
    int16_t clip_y;
    int16_t clip_w;
    int16_t clip_h;
};

struct Glyph {
    uint16_t bitmapOffset;
    uint8_t width;
    uint8_t height;
    uint8_t xAdvance;
    int8_t xOffset;
    int8_t yOffset;
};

struct Font {
    const uint8_t *bitmap;
    const Glyph *glyph;
    uint16_t first;
    uint16_t last;
    uint8_t yAdvance;
};

extern const Font Rismol3x5;
extern const Font Rismol5x7;
const Font *defaultFont();
const Font *titleFont();

struct Event {
    EventType type;
    uint32_t key;
    uint32_t modifiers;
    bool repeat;
};

class Gui;

struct PortConfig {
    const char *title = 0;
    uint8_t scale = 1;
    PixelFormat format = PixelFormat::Gray8;
    void *native = 0;
    void *user = 0;
    uint32_t daemonStackBytes = 4096;
    uint8_t daemonPriority = 5;
    bool autoStartDaemon = true;
};

class Port {
public:
    CanvasBuffer target;

    Port();
    virtual ~Port() {}

    virtual bool valid() const { return target.pixels != 0; }
    virtual bool pump(Gui &gui)
    {
        (void)gui;
        return true;
    }
    virtual bool shouldClose() const { return false; }
    virtual bool hasSystemBorder() const { return true; }
    virtual Result present(const CanvasBuffer &canvas, const Rect *dirty) = 0;
    virtual Result startDaemon(void (*entry)(void *), void *arg)
    {
        (void)entry;
        (void)arg;
        return Result::Unsupported;
    }
    virtual void stopDaemon() {}
    virtual void notifyCommit() {}
    virtual bool waitCommit() { return false; }
    virtual bool waitCommitTimeout(uint32_t timeoutMs)
    {
        (void)timeoutMs;
        return waitCommit();
    }
    virtual void lock() {}
    virtual void unlock() {}
};

Port *createDefaultPort(uint16_t width, uint16_t height,
                        const PortConfig &config = PortConfig());
void destroyDefaultPort(Port *port);

namespace detail { struct ContextState; }

inline PixelFormat nativePixelFormat(PixelFormat format) { return format; }
inline size_t framebufferBytes(uint16_t width, uint16_t height,
                               PixelFormat format)
{
    switch (format) {
    case PixelFormat::Mono1:
        return static_cast<size_t>(width) * ((height + 7u) / 8u);
    case PixelFormat::Gray4:
        return static_cast<size_t>((width + 1u) / 2u) * height;
    case PixelFormat::Gray8:
        return static_cast<size_t>(width) * height;
    default:
        return 0;
    }
}

class Print {
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t byte) = 0;

    size_t write(const char *text);
    size_t write(const uint8_t *buffer, size_t size);

    size_t print(const char *text);
    size_t print(char ch);
    size_t print(int value, int base = 10);
    size_t print(unsigned int value, int base = 10);
    size_t print(long value, int base = 10);
    size_t print(unsigned long value, int base = 10);
    size_t print(float value, int digits = 2);
    size_t print(double value, int digits = 2);

    size_t println();
    size_t println(const char *text);
    size_t println(int value, int base = 10);
    size_t println(unsigned int value, int base = 10);
    size_t println(long value, int base = 10);
    size_t println(unsigned long value, int base = 10);
    size_t println(float value, int digits = 2);
    size_t println(double value, int digits = 2);

    size_t printf(const char *format, ...);

private:
    size_t printUnsigned(unsigned long value, int base);
};

class ThinGFX : public Print {
public:
    explicit ThinGFX(CanvasBuffer *canvas = 0);
    virtual ~ThinGFX() {}

    CanvasBuffer *native() { return canvas_; }
    const CanvasBuffer *native() const { return canvas_; }
    void setCanvas(CanvasBuffer *canvas);
    bool resetClip();
    bool setClip(const Rect &rect);
    bool intersectClip(const Rect &rect);
    Rect clip() const;

    int16_t width() const { return width_; }
    int16_t height() const { return height_; }
    int16_t rawWidth() const { return raw_width_; }
    int16_t rawHeight() const { return raw_height_; }

    virtual void drawPixel(int16_t x, int16_t y, Color color);
    virtual Color getPixel(int16_t x, int16_t y) const;

    virtual void startWrite() {}
    virtual void writePixel(int16_t x, int16_t y, Color color);
    virtual void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                               Color color);
    virtual void writeFastVLine(int16_t x, int16_t y, int16_t h, Color color);
    virtual void writeFastHLine(int16_t x, int16_t y, int16_t w, Color color);
    virtual void writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                           Color color);
    virtual void endWrite() {}

    virtual void setRotation(uint8_t rotation);
    uint8_t getRotation() const { return rotation_; }
    virtual void invertDisplay(bool invert) { inverted_ = invert; }

    virtual void drawFastVLine(int16_t x, int16_t y, int16_t h, Color color);
    virtual void drawFastHLine(int16_t x, int16_t y, int16_t w, Color color);
    virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                          Color color);
    virtual void fillScreen(Color color);
    virtual void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                          Color color);
    virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h,
                          Color color);

    void drawCircle(int16_t x0, int16_t y0, int16_t r, Color color);
    void fillCircle(int16_t x0, int16_t y0, int16_t r, Color color);
    void drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, Color color);
    void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                      int16_t x2, int16_t y2, Color color);
    void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t radius, Color color);
    void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                       int16_t radius, Color color);
    void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap,
                    int16_t w, int16_t h, Color color);
    void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap,
                    int16_t w, int16_t h, Color color, Color bg);

    void drawChar(int16_t x, int16_t y, unsigned char ch, Color color,
                  Color bg, uint8_t size);
    void drawChar(int16_t x, int16_t y, unsigned char ch, Color color,
                  Color bg, uint8_t size_x, uint8_t size_y);
    void getTextBounds(const char *text, int16_t x, int16_t y, int16_t *x1,
                       int16_t *y1, uint16_t *w, uint16_t *h) const;

    size_t write(uint8_t byte) override;

    void setCursor(int16_t x, int16_t y) { cursor_x_ = x; cursor_y_ = y; }
    int16_t getCursorX() const { return cursor_x_; }
    int16_t getCursorY() const { return cursor_y_; }

    void setTextColor(Color color) { text_color_ = color; text_bg_ = color; }
    void setTextColor(Color color, Color bg) { text_color_ = color; text_bg_ = bg; }
    void setTextSize(uint8_t size) { setTextSize(size, size); }
    void setTextSize(uint8_t size_x, uint8_t size_y);
    void setTextWrap(bool wrap) { wrap_ = wrap; }
    void cp437(bool enable = true) { cp437_ = enable; }
    void setFont(const Font *font = 0);
    const Font *font() const { return font_; }

protected:
    CanvasBuffer *canvas_;
    int16_t raw_width_;
    int16_t raw_height_;
    int16_t width_;
    int16_t height_;
    int16_t cursor_x_;
    int16_t cursor_y_;
    Color text_color_;
    Color text_bg_;
    uint8_t text_size_x_;
    uint8_t text_size_y_;
    uint8_t rotation_;
    bool wrap_;
    bool cp437_;
    bool inverted_;
    const Font *font_;
};

class Canvas : public ThinGFX {
public:
    explicit Canvas(CanvasBuffer *canvas = 0) : ThinGFX(canvas) {}
};

class OwnedCanvas : public ThinGFX {
public:
    OwnedCanvas(uint16_t width, uint16_t height, PixelFormat format);
    ~OwnedCanvas();

    OwnedCanvas(const OwnedCanvas &) = delete;
    OwnedCanvas &operator=(const OwnedCanvas &) = delete;

    bool valid() const { return native() != 0; }
};

class Window;

using WindowDrawCallback = void (*)(Window &window, ThinGFX &gfx,
                                    const Rect &dirty, void *user);
using WindowEventCallback = bool (*)(Window &window, const Event &event,
                                     void *user);

struct WindowConfig {
    NodeKind kind = NodeKind::Window;
    Rect frame = {0, 0, 0, 0};
    uint8_t opacity = 255;
    bool visible = true;
    bool ownsCanvas = true;
    PixelFormat canvasFormat = PixelFormat::Gray8;
    bool showTitle = true;
    WindowBorder border = WindowBorder::Rect;
    const char *title = 0;
    Transition transition;
    Transition createTransition;
    Transition destroyTransition;
    WindowDrawCallback onDraw = 0;
    WindowEventCallback onEvent = 0;
    void *user = 0;
};

class Window {
public:
    Window() : gui_(0), handle_(InvalidHandle) {}
    Window(Gui *gui, Handle handle) : gui_(gui), handle_(handle) {}

    bool valid() const { return gui_ != 0 && handle_ != InvalidHandle; }
    Handle handle() const { return handle_; }

    Rect rect() const;
    bool setRect(const Rect &rect);
    bool setRectImmediate(const Rect &rect);
    bool setRectAnimated(const Rect &rect, const Transition &transition);
    bool setTransition(const Transition &transition);
    Transition transition() const;
    bool setDestroyTransition(const Transition &transition);
    Transition destroyTransition() const;
    bool setVisible(bool visible);
    bool setOpacity(uint8_t opacity);
    bool bringToFront();
    bool sendToBack();
    bool setParent(const Window &parent);
    Window parent() const;
    Window firstChild() const;
    Window nextSibling() const;
    Window createChild(const WindowConfig &config) const;
    bool focus();
    bool invalidate(const Rect &rect);
    bool invalidate();
    bool dispatch(const Event &event) const;
    bool destroy();
    bool destroyImmediate();
    bool destroyAnimated(const Transition &transition);
    Canvas canvas() const;
    Canvas back() const;
    Canvas draw() const;
    bool commit();
    bool commit(const Rect &rect);
    bool discard();

private:
    Gui *gui_;
    Handle handle_;

    friend class Gui;
};

class Gui {
public:
    Gui(uint16_t width, uint16_t height);
    Gui(uint16_t width, uint16_t height, PixelFormat outputFormat);
    Gui(uint16_t width, uint16_t height, const PortConfig &portConfig);
    Gui(uint16_t width, uint16_t height, Port *externalPort);
    ~Gui();

    Gui(const Gui &) = delete;
    Gui &operator=(const Gui &) = delete;

    bool valid() const { return ctx_ != 0; }
    uint16_t width() const;
    uint16_t height() const;
    bool resize(uint16_t width, uint16_t height);

    Window root() const;
    Window createWindow(const WindowConfig &config);
    Window createWindow(const Window &parent, const WindowConfig &config);
    Window createLayer(const WindowConfig &config);
    Window createLayer(const Window &parent, const WindowConfig &config);
    void lock();
    void unlock();
    bool setFocus(const Window &window);
    bool clearFocus();
    bool focusNext();
    bool focusPrevious();
    Window focus() const;
    bool dispatchTo(const Window &window, const Event &event);
    bool dispatchEvent(const Event &event, Window *target = 0);
    bool dispatchKey(uint32_t key, bool pressed, bool repeat = false,
                     uint32_t modifiers = ModNone, Window *target = 0);
    bool dispatchKey(Key key, bool pressed, bool repeat = false,
                     uint32_t modifiers = ModNone, Window *target = 0)
    {
        return dispatchKey(static_cast<uint32_t>(key), pressed, repeat,
                           modifiers, target);
    }
    bool needsRender() const;
    bool render(ThinGFX &target);
    bool render(CanvasBuffer *target);
    bool attachPort(Port *port);
    Port *port();
    const Port *port() const;
    bool portValid() const;
    bool pump();
    bool present(const Rect *dirty = 0);
    bool startDaemon();
    void stopDaemon();
    bool daemonStep();
    void notifyDaemon();
    void tick(uint32_t delta_ms);
    bool animating() const;

private:
    struct NodeBinding {
        Gui *gui;
        Handle handle;
        WindowDrawCallback draw;
        WindowEventCallback event;
        void *user;
    };

    static void drawThunk(detail::ContextState *ctx, Handle handle,
                          CanvasBuffer *canvas, const Rect *dirty,
                          void *user);
    static uint8_t eventThunk(detail::ContextState *ctx, Handle handle,
                              const Event *event, void *user);
    NodeBinding *bindingFor(Handle handle);
    void removeBinding(Handle handle);
    void removeBindingsRecursive(Handle handle);
    void removeStaleBindings();

    detail::ContextState *ctx_;
    Port *owned_port_;
    bool owns_port_;
    bool daemon_started_;
    std::vector<NodeBinding *> bindings_;

    friend class Window;
};

using GFX = ThinGFX;

} // namespace thingfx

#endif
