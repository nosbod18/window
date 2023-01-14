#pragma once
#include <stdbool.h>

typedef enum WtkEventType {
    WtkEventType_KeyUp,
    WtkEventType_KeyDown,
    WtkEventType_MouseUp,
    WtkEventType_MouseDown,
    WtkEventType_MouseEnter,
    WtkEventType_MouseLeave,
    WtkEventType_MouseScroll,
    WtkEventType_MouseMotion,
    WtkEventType_WindowClose,
    WtkEventType_WindowResize,
    WtkEventType_WindowFocusIn,
    WtkEventType_WindowFocusOut,
} WtkEventType;

// Ascii keys can use their character representation, e.g. 'w', 'A', '+', ...
typedef enum WtkKey {
    WtkKey_Backspace = 0x08, WtkKey_Tab = 0x09, WtkKey_Enter = 0x0a, WtkKey_Escape = 0x1b,
    WtkKey_Up = 0x80, WtkKey_Down, WtkKey_Left, WtkKey_Right,
    WtkKey_F1, WtkKey_F2, WtkKey_F3, WtkKey_F4, WtkKey_F5, WtkKey_F6, WtkKey_F7, WtkKey_F8, WtkKey_F9, WtkKey_F10, WtkKey_F11, WtkKey_F12,
    WtkKey_LeftShift, WtkKey_LeftCtrl, WtkKey_LeftSuper, WtkKey_LeftAlt,
    WtkKey_RightShift, WtkKey_RightCtrl, WtkKey_RightSuper, WtkKey_RightAlt,
    WtkKey_Capslock,
} WtkKey;

typedef enum WtkButton {
    WtkButton_1,
    WtkButton_2,
    WtkButton_3,
    WtkButton_4,
    WtkButton_5,
    WtkButton_6,
    WtkButton_7,
    WtkButton_8,
} WtkButton;

typedef enum WtkMod {
    WtkMod_Shift   = 1U << 0,
    WtkMod_Ctrl    = 1U << 1,
    WtkMod_Alt     = 1U << 2,
    WtkMod_Super   = 1U << 3,
} WtkMod;

typedef union WtkEvent {
    struct { int dx, dy;                } scroll, motion;
    struct { int code, sym, mods, x, y; } key, button;
} WtkEvent;

typedef struct WtkWindow WtkWindow;

typedef struct WtkWindowDesc {
    void (*event_handler)(WtkWindow *window, WtkEventType type, WtkEvent const *event);
    char const *title;
    int x, y, w, h;
} WtkWindowDesc;


WtkWindow  *WtkCreateWindow         (WtkWindowDesc const *desc);
void        WtkMakeCurrent          (WtkWindow *window);
void        WtkSwapBuffers          (WtkWindow *window);
void        WtkPollEvents           (void);
void        WtkDeleteWindow         (WtkWindow *window);

void        WtkGetWindowRect        (WtkWindow const *window, int *x, int *y, int *w, int *h);
bool        WtkGetWindowShouldClose (WtkWindow const *window);

void        WtkSetWindowOrigin      (WtkWindow *window, int x, int y);
void        WtkSetWindowSize        (WtkWindow *window, int w, int h);
void        WtkSetWindowTitle       (WtkWindow *window, char const *title);
void        WtkSetWindowShouldClose (WtkWindow *window, bool should_close);