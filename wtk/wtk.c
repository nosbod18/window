#include "wtk.h"
#include <stdlib.h> // calloc, free
#include <stdio.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
    #include <GL/gl.h>
    #include <GL/wgl.h>

    // https://gist.github.com/nickrolfe/1127313ed1dbf80254b614a721b3ee9c
    typedef HGLRC WINAPI WglCreateContextAttribsARBProc(HDC hdc, HGLRC hShareContext, const int *attribList);
    typedef BOOL WINAPI WglChoosePixelFormatARBProc(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
    
    #define WGL_CONTEXT_MAJOR_VERSION_ARB             0x2091
    #define WGL_CONTEXT_MINOR_VERSION_ARB             0x2092
    #define WGL_CONTEXT_PROFILE_MASK_ARB              0x9126

    #define WGL_CONTEXT_CORE_PROFILE_BIT_ARB          0x00000001

    #define WGL_DRAW_TO_WINDOW_ARB                    0x2001
    #define WGL_ACCELERATION_ARB                      0x2003
    #define WGL_SUPPORT_OPENGL_ARB                    0x2010
    #define WGL_DOUBLE_BUFFER_ARB                     0x2011
    #define WGL_PIXEL_TYPE_ARB                        0x2013
    #define WGL_COLOR_BITS_ARB                        0x2014
    #define WGL_DEPTH_BITS_ARB                        0x2022
    #define WGL_STENCIL_BITS_ARB                      0x2023

    #define WGL_FULL_ACCELERATION_ARB                 0x2027
    #define WGL_TYPE_RGBA_ARB                         0x202B
#elif defined(__linux__)
    #include <X11/Xlib.h>
    #include <X11/keysym.h>
    #include <X11/XKBlib.h>
    #include <GL/glx.h>
    typedef GLXContext GlXCreateContextAttribsARBProc(Display *, GLXFBConfig, GLXContext, Bool, int const *);
#elif defined(__APPLE__)
    #import <Cocoa/Cocoa.h>
    @interface CocoaApp : NSObject <NSApplicationDelegate>
    @end
    @interface CocoaView : NSOpenGLView <NSWindowDelegate>
    - (id)initWithFrame:(NSRect)frame andWindow:(WtkWindow *)window;
    @end
#else
    #error "Unsupported platform"
#endif

struct WtkWindow {
    WtkWindowDesc desc;
    int closed;
#if defined(_WIN32)
    HWND window;
    HDC device;
    HGLRC context;
#elif defined(__linux__)
    Window window;
    GLXContext context;
#elif defined(__APPLE__)
    NSWindow *window;
    CocoaView *view;
#endif
};

static struct {
#if defined(_WIN32)
    struct {
        WglCreateContextAttribsARBProc *wglCreateContextAttribsARB;
        WglChoosePixelFormatARBProc *wglChoosePixelFormatARB;
    } win32;
#elif defined(__linux__)
    struct {
        GlXCreateContextAttribsARBProc *glx_create_ctx_attribs;
        Display *display;
        XContext context;
        Visual *visual;
        Window root;
        Colormap colormap;
        Atom wm_delwin;
        int screen;
        int depth;
    } x11;
#elif defined(__APPLE__)
    struct {
        CocoaApp *app;
    } cocoa;
#endif
    int num_windows;
} G = {0};

///
/// Win32
///

static LRESULT CALLBACK windowProc(HWND wnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    WtkWindow *window = (WtkWindow *)GetWindowLongPtr(wnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CLOSE:
            WtkSetWindowShouldClose(window, 1);
            window->desc.callback(window, &(WtkEvent){.type = WTK_EVENTTYPE_WINDOWCLOSE});
            return 0;

        default:
            break;
    }

    return DefWindowProc(wnd, msg, wparam, lparam);
}

static int init(void) {
    WNDCLASS wnd_class = {
        .style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC,
        .lpfnWndProc = DefWindowProc,
        .hInstance = GetModuleHandle(NULL),
        .lpszClassName = "DummyWindowClass"
    };

    if (!RegisterClass(&wnd_class))
        return 0;

    HWND dummy_wnd = CreateWindow(
        wnd_class.lpszClassName, "Dummy OpenGL Window", 0,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        0, 0, wnd_class.hInstance, 0
    );

    if (!dummy_wnd)
        return 0;

    HDC dummy_dc = GetDC(dummy_wnd);

    PIXELFORMATDESCRIPTOR pfd = {
        .nSize = sizeof pfd,
        .nVersion = 1,
        .iPixelType = PFD_TYPE_RGBA,
        .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        .cColorBits = 32,
        .cAlphaBits = 8,
        .iLayerType = PFD_MAIN_PLANE,
        .cDepthBits = 24,
        .cStencilBits = 8,
    };

    int pixel_format = ChoosePixelFormat(dummy_dc, &pfd);
    if (!pixel_format)
        return 0;

    if (!SetPixelFormat(dummy_dc, pixel_format, &pfd))
        return 0;

    HGLRC dummy_ctx = wglCreateContext(dummy_dc);
    if (!dummy_ctx)
        return 0;

    if (!wglMakeCurrent(dummy_dc, dummy_ctx))
        return 0;

    G.win32.wglCreateContextAttribsARB = (WglCreateContextAttribsARBProc *)wglGetProcAddress("wglCreateContextAttribsARB");
    G.win32.wglChoosePixelFormatARB = (WglChoosePixelFormatARBProc *)wglGetProcAddress("wglChoosePixelFormatARB");

    wglMakeCurrent(dummy_dc, 0);
    wglDeleteContext(dummy_ctx);
    ReleaseDC(dummy_wnd, dummy_dc);
    DestroyWindow(dummy_wnd);

    return 1;
}

static void quit(void) {
    // Move along...
}

static int createWindow(WtkWindow *window) {
    WNDCLASS wnd_class = {
        .style = CS_OWNDC,
        .lpfnWndProc = windowProc,
        .hInstance = GetModuleHandle(NULL),
        .hCursor = LoadCursor(0, IDC_ARROW),
        .lpszClassName = "WtkWindowClass",
    };

    if (!RegisterClass(&wnd_class))
        return 0;

    RECT rect = { .right = window->desc.w, .bottom = window->desc.h };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, 0);

    window->window = CreateWindow(
        wnd_class.lpszClassName, window->desc.title,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );

    if (!window->window)
        return 0;

    window->device = GetDC(window->window);

    int pf_attribs[] = {
        WGL_DRAW_TO_WINDOW_ARB,     GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB,     GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB,      GL_TRUE,
        WGL_ACCELERATION_ARB,       WGL_FULL_ACCELERATION_ARB,
        WGL_PIXEL_TYPE_ARB,         WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB,         32,
        WGL_DEPTH_BITS_ARB,         24,
        WGL_STENCIL_BITS_ARB,       8,
        0
    };

    int pixel_format;
    UINT num_formats;
    G.win32.wglChoosePixelFormatARB(window->device, pf_attribs, 0, 1, &pixel_format, &num_formats);
    if (!num_formats)
        return 0;

    PIXELFORMATDESCRIPTOR pfd;
    DescribePixelFormat(window->device, pixel_format, sizeof pfd, &pfd);
    if (!SetPixelFormat(window->device, pixel_format, &pfd))
        return 0;

    int ctx_attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
        WGL_CONTEXT_MINOR_VERSION_ARB, 3,
        WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0,
    };

    window->context = G.win32.wglCreateContextAttribsARB(window->device, 0, ctx_attribs);
    if (!window->context)
        return 0;

    ShowWindow(window->window, SW_SHOW);
    return 1;
}

static void makeCurrent(WtkWindow *window) {
    wglMakeCurrent(window->device, window->context);
}

static void swapBuffers(WtkWindow *window) {
    SwapBuffers(window->device);
}

static void pollEvents(void) {
    for (MSG msg; PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

static void deleteWindow(WtkWindow *window) {
    ReleaseDC(window->window, window->device);
    DestroyWindow(window->window);
    wglDeleteContext(window->context);
}

static void setWindowOrigin(WtkWindow *window, int x, int y) {
    SetWindowPos(window->window, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

static void setWindowSize(WtkWindow *window, int w, int h) {
    SetWindowPos(window->window, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
}

static void setWindowTitle(WtkWindow *window, char const *title) {
    SetWindowText(window->window, title);
}

///
/// X11
///

#if defined(__linux__)

static int translateKeyCode(int xkey, int state) {
    xkey = XkbKeycodeToKeysym(G.x11.display, xkey, 0, (state & ShiftMask) ? 1 : 0);
    switch(xkey) {
        case XK_BackSpace:  return WTK_KEY_BACKSPACE;
        case XK_Tab:        return WTK_KEY_TAB;
        case XK_Return:     return WTK_KEY_ENTER;
        case XK_Escape:     return WTK_KEY_ESCAPE;
        case XK_Up:         return WTK_KEY_UP;
        case XK_Down:       return WTK_KEY_DOWN;
        case XK_Left:       return WTK_KEY_LEFT;
        case XK_Right:      return WTK_KEY_RIGHT;
        case XK_Page_Up:    return WTK_KEY_PAGEUP;
        case XK_Page_Down:  return WTK_KEY_PAGEDOWN;
        case XK_Home:       return WTK_KEY_HOME;
        case XK_End:        return WTK_KEY_END;
        case XK_Insert:     return WTK_KEY_INSERT;
        case XK_Delete:     return WTK_KEY_DELETE;
        case XK_F1:         return WTK_KEY_F1;
        case XK_F2:         return WTK_KEY_F2;
        case XK_F3:         return WTK_KEY_F3;
        case XK_F4:         return WTK_KEY_F4;
        case XK_F5:         return WTK_KEY_F5;
        case XK_F6:         return WTK_KEY_F6;
        case XK_F7:         return WTK_KEY_F7;
        case XK_F8:         return WTK_KEY_F8;
        case XK_F9:         return WTK_KEY_F9;
        case XK_F10:        return WTK_KEY_F10;
        case XK_F11:        return WTK_KEY_F11;
        case XK_F12:        return WTK_KEY_F12;
        case XK_Shift_L:    return WTK_KEY_LSHIFT;
        case XK_Shift_R:    return WTK_KEY_RSHIFT;
        case XK_Control_L:  return WTK_KEY_LCTRL;
        case XK_Control_R:  return WTK_KEY_RCTRL;
        case XK_Super_L:    return WTK_KEY_LSUPER;
        case XK_Super_R:    return WTK_KEY_RSUPER;
        case XK_Alt_L:      return WTK_KEY_LALT;
        case XK_Alt_R:      return WTK_KEY_RALT;
        case XK_Caps_Lock:  return WTK_KEY_CAPSLOCK;
        default:            return xkey;
    }
}

static unsigned int translateState(int state) {
    unsigned int modifiers = 0;

    if (state & ControlMask) modifiers |= WTK_MOD_CTRL;
    if (state & ShiftMask)   modifiers |= WTK_MOD_SHIFT;
    if (state & Mod1Mask)    modifiers |= WTK_MOD_ALT;
    if (state & Mod4Mask)    modifiers |= WTK_MOD_SUPER;
    if (state & LockMask)    modifiers |= WTK_MOD_CAPSLOCK;

    return modifiers;
}

static void postEvent(WtkWindow *window, int type, XEvent const *xevent) {
    static WtkEvent previousEvent = {0};
    WtkEvent event = {.type = type};
    if (type == WTK_EVENTTYPE_KEYDOWN || type == WTK_EVENTTYPE_KEYUP) {
        event.keyCode    = translateKeyCode(xevent->xkey.keycode, xevent->xkey.state);
        event.modifiers  = xevent->xkey.state;
        event.location.x = xevent->xkey.x;
        event.location.y = xevent->xkey.y;
    } else if (type == WTK_EVENTTYPE_MOUSEDOWN || type == WTK_EVENTTYPE_MOUSEUP) {
        switch (xevent->xbutton.button) {
            case Button4: event.delta.y =  1.0; break;
            case Button5: event.delta.y = -1.0; break;
            case 6:       event.delta.x =  1.0; break;
            case 7:       event.delta.x = -1.0; break;
            default:      event.buttonNumber = xevent->xbutton.button - Button1 - 4; break;
        }
        event.modifiers  = xevent->xbutton.state;
        event.location.x = xevent->xbutton.x;
        event.location.y = xevent->xbutton.y;
    } else if (type == WTK_EVENTTYPE_MOUSEMOTION) {
        event.modifiers  = xevent->xmotion.state;
        event.location.x = xevent->xmotion.x;
        event.location.y = xevent->xmotion.y;
    }
    event.modifiers = translateState(event.modifiers);
    event.delta.x = event.location.x - previousEvent.location.x;
    event.delta.y = event.location.y - previousEvent.location.y;

    window->desc.callback(window, &event);
    previousEvent = event;
}

int init(void) {
    if (!(G.x11.display = XOpenDisplay(NULL)))
        return 0;

    G.x11.context = XUniqueContext();
    G.x11.screen  = DefaultScreen(G.x11.display);
    G.x11.root    = RootWindow(G.x11.display, G.x11.screen);
    G.x11.visual  = DefaultVisual(G.x11.display, G.x11.screen);
    G.x11.depth   = DefaultDepth(G.x11.display, G.x11.screen);

    if (!(G.x11.colormap = XCreateColormap(G.x11.display, G.x11.root, G.x11.visual, AllocNone)))
        return 0;

    if (!(G.x11.wm_delwin = XInternAtom(G.x11.display, "WM_DELETE_WINDOW", 0)))
        return 0;

    G.x11.glx_create_ctx_attribs = (GlXCreateContextAttribsARBProc *)glXGetProcAddressARB((GLubyte const *)"glXCreateContextAttribsARB");

    return 1;
}

void quit(void) {
    XFreeColormap(G.x11.display, G.x11.colormap);
    XCloseDisplay(G.x11.display);
}

int createWindow(WtkWindow *window) {
    XSetWindowAttributes swa = {
        .event_mask = StructureNotifyMask|PointerMotionMask|ButtonPressMask|ButtonReleaseMask|KeyPressMask|KeyReleaseMask|EnterWindowMask|LeaveWindowMask|FocusChangeMask|ExposureMask,
        .colormap = G.x11.colormap
    };

    window->window = XCreateWindow(
        G.x11.display, G.x11.root,
        window->desc.x, window->desc.y, window->desc.w, window->desc.h,
        0, G.x11.depth, InputOutput,
        G.x11.visual, CWColormap | CWEventMask, &swa
    );
    if (!window->window) return 0;

    if (!XSetWMProtocols(G.x11.display, window->window, &G.x11.wm_delwin, 1))
        return 0;

    GLint vis_attribs[] = {
        GLX_RENDER_TYPE,  GLX_RGBA_BIT,
        GLX_DOUBLEBUFFER, 1,
        None
    };

    int fbcount = 0;
    GLXFBConfig *fbc = glXChooseFBConfig(G.x11.display, G.x11.screen, vis_attribs, &fbcount);
    if (!fbc || !fbcount) return 0;

    GLint ctx_attribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 6,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        None
    };

    if (G.x11.glx_create_ctx_attribs)
        window->context = G.x11.glx_create_ctx_attribs(G.x11.display, fbc[0], NULL, 1, ctx_attribs);
    else
        window->context = glXCreateNewContext(G.x11.display, fbc[0], GLX_RGBA_TYPE, NULL, 1);

    XSaveContext(G.x11.display, window->window, G.x11.context, (XPointer)window);
    XMapWindow(G.x11.display, window->window);
    XFlush(G.x11.display);
    return 1;
}

void makeCurrent(WtkWindow *window) {
    glXMakeContextCurrent(G.x11.display, window->window, window->window, window->context);
}

void swapBuffers(WtkWindow *window) {
    glXSwapBuffers(G.x11.display, window->window);
}

void pollEvents(void) {
    WtkWindow *window;
    XEvent event;

    while (XPending(G.x11.display)) {
        XNextEvent(G.x11.display, &event);
        if (XFindContext(G.x11.display, event.xany.window, G.x11.context, (XPointer *)&window))
            continue;

        switch (event.type) {
            case KeyPress:      postEvent(window, WTK_EVENTTYPE_KEYDOWN, &event);        break;
            case KeyRelease:    postEvent(window, WTK_EVENTTYPE_KEYUP, &event);          break;
            case ButtonPress:   postEvent(window, WTK_EVENTTYPE_MOUSEDOWN, &event);      break;
            case ButtonRelease: postEvent(window, WTK_EVENTTYPE_MOUSEUP, &event);        break;
            case MotionNotify:  postEvent(window, WTK_EVENTTYPE_MOUSEMOTION, &event);    break;
            case EnterNotify:   postEvent(window, WTK_EVENTTYPE_MOUSEENTER, &event);     break;
            case LeaveNotify:   postEvent(window, WTK_EVENTTYPE_MOUSELEAVE, &event);     break;
            case MapNotify:     postEvent(window, WTK_EVENTTYPE_WINDOWFOCUSIN, &event);  break;
            case UnmapNotify:   postEvent(window, WTK_EVENTTYPE_WINDOWFOCUSOUT, &event); break;
            case ConfigureNotify: {
                window->desc.x = event.xconfigure.x;
                window->desc.y = event.xconfigure.y;
                window->desc.w = event.xconfigure.width;
                window->desc.h = event.xconfigure.height;
                postEvent(window, WTK_EVENTTYPE_WINDOWRESIZE, &event);
            } break;
            case ClientMessage: {
                if ((Atom)event.xclient.data.l[0] == G.x11.wm_delwin) {
                    WtkSetWindowShouldClose(window, 1);
                    postEvent(window, WTK_EVENTTYPE_WINDOWCLOSE, &event);
                }
            } break;
        }
    }
}

void deleteWindow(WtkWindow *window) {
    glXDestroyContext(G.x11.display, window->context);
    XDestroyWindow(G.x11.display, window->window);
}

void setWindowOrigin(WtkWindow *window, int x, int y) {
    XMoveWindow(G.x11.display, window->window, x, y);
}

void setWindowSize(WtkWindow *window, int w, int h) {
    XResizeWindow(G.x11.display, window->window, (unsigned int)w, (unsigned int)h);
}

void setWindowTitle(WtkWindow *window, char const *title) {
    XStoreName(G.x11.display, window->window, title);
}

///
/// Cocoa
///

#elif defined(__APPLE__)

static void postEvent(WtkWindow *window, WTK_EVENTTYPE type, NSEvent *event) {
    WtkEvent ev = {0};
    if (type == WTK_EVENTTYPE_KEYDOWN || type == WTK_EVENTTYPE_KEYUP) {
        ev.key.code = [event keyCode];
        ev.key.sym  = [event keyCode];
        ev.key.mods = [event modifierFlags];
        ev.key.x    = [event locationInWindow].x;
        ev.key.y    = [event locationInWindow].y;
    } else if (type == WTK_EVENTTYPE_MOUSEDOWN || type == WTK_EVENTTYPE_MOUSEUP) {
        ev.button.code = [event buttonNumber];
        ev.button.sym  = [event buttonNumber];
        ev.button.mods = [event modifierFlags];
        ev.button.x    = [event locationInWindow].x;
        ev.button.y    = [event locationInWindow].y;
    } else if (type == WTK_EVENTTYPE_MOUSESCROLL) {
        ev.motion.dx = [event deltaX];
        ev.motion.dy = [event deltaY];
    }
    window->desc.callback(window, type, &ev);
}

static float translateYCoordinate(float y) {
    return CGDisplayBounds(CGMainDisplayID()).size.height - y - 1;
}

@implementation CocoaApp
-(void)applicationWillFinishLaunching:(NSNotification *)notification {
    id menubar = [[NSMenu new] autorelease];
    id appMenuItem = [[NSMenuItem new] autorelease];
    id appMenu = [[NSMenu new] autorelease];
    id appName = [[NSProcessInfo processInfo] processName];
    id quitTitle = [@"Quit " stringByAppendingString:appName];
    id quitMenuItem = [[[NSMenuItem alloc] initWithTitle:quitTitle action:@selector(terminate:) keyEquivalent:@"q"] autorelease];

    [menubar addItem:appMenuItem];
    [appMenu addItem:quitMenuItem];
    [appMenuItem setSubmenu:appMenu];
    [NSApp setMainMenu:menubar];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
}

-(BOOL)applicationShouldTerminateAfterLastWINDOWCLOSEd:(NSApplication *)sender {
    return YES;
}
@end

@implementation CocoaView {
    WtkWindow *m_window;
}

- (id)initWithFrame:(NSRect)frame andWindow:(WtkWindow *)window  {
    NSOpenGLPixelFormatAttribute attributes[] = {
        NSOpenGLPFAOpenGLProfile,   NSOpenGLProfileVersion4_1Core,
        NSOpenGLPFAMultisample,
        NSOpenGLPFAAccelerated,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAColorSize,       32,
        NSOpenGLPFADepthSize,       24,
        NSOpenGLPFAAlphaSize,        8,
        0
    };

    if (self = [super initWithFrame:frame pixelFormat:[[[NSOpenGLPixelFormat alloc] initWithAttributes:attributes] autorelease]])
        m_window = window;
    return self;
}

-(BOOL)windowShouldClose:(NSNotification *)notification {
    WtkSetWindowShouldClose(m_window, 1);
    postEvent(m_window, WTK_EVENTTYPE_WINDOWCLOSE, NULL);
    return NO;
}

-(void)keyDown:(NSEvent *)event         { postEvent(m_window, WTK_EVENTTYPE_KEYDOWN, event); }
-(void)keyUp:(NSEvent *)event           { postEvent(m_window, WTK_EVENTTYPE_KEYUP, event); }
-(void)mouseDown:(NSEvent *)event       { postEvent(m_window, WTK_EVENTTYPE_MOUSEDOWN, event); }
-(void)mouseUp:(NSEvent *)event         { postEvent(m_window, WTK_EVENTTYPE_MOUSEUP, event);   }
-(void)rightMouseDown:(NSEvent *)event  { [self mouseDown:event]; }
-(void)rightMouseUp:(NSEvent *)event    { [self mouseUp:event];   }
-(void)otherMouseDown:(NSEvent *)event  { [self mouseDown:event]; }
-(void)otherMouseUp:(NSEvent *)event    { [self mouseUp:event];   }
-(void)mouseEntered:(NSEvent *)event    { postEvent(m_window, WTK_EVENTTYPE_MOUSEENTER, event); }
-(void)mouseExited:(NSEvent *)event     { postEvent(m_window, WTK_EVENTTYPE_MOUSELEAVE, event); }
-(void)mouseMoved:(NSEvent *)event      { postEvent(m_window, WTK_EVENTTYPE_MOUSEMOTION, event); }
-(void)scrollWheel:(NSEvent *)event     { postEvent(m_window, WTK_EVENTTYPE_MOUSESCROLL, event); }

@end

int init(void) {
    @autoreleasepool {

    [NSApplication sharedApplication];
    G.cocoa.app = [[CocoaApp alloc] init];

    [NSApp setDelegate:G.cocoa.app];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    [NSApp finishLaunching];

    return 1;

    }
}

void quit(void) {
    @autoreleasepool {

    [NSApp terminate:nil];
    [G.cocoa.app release];

    }
}

int createWindow(WtkWindow *window) {
    @autoreleasepool {

    unsigned styleMask = NSWindowStyleMaskMiniaturizable|NSWindowStyleMaskTitled|NSWindowStyleMaskClosable|NSWindowStyleMaskResizable;
    NSRect frame = NSMakeRect(0, 0, window->desc.w, window->desc.h);

    window->window = [[NSWindow alloc] initWithContentRect:frame styleMask:styleMask backing:NSBackingStoreBuffered defer:NO];
    if (!window->window) return 0;

    window->view = [[CocoaView alloc] initWithFrame:frame andWindow:window];
    if (!window->view) return 0;

    [window->window setContentView:window->view];
    [window->window setDelegate:window->view];
    [window->window makeFirstResponder:window->view];

    [window->window setAcceptsMouseMovedEvents:YES];
    [window->window setRestorable:NO];
    [window->window center];
    [window->window makeKeyAndOrderFront:nil];
    [window->window orderFront:nil];

    return 1;

    }
}

void makeCurrent(WtkWindow *window) {
    @autoreleasepool {

    [[window->view openGLContext] makeCurrentContext];

    }
}

void swapBuffers(WtkWindow *window) {
    @autoreleasepool {

    [[window->view openGLContext] flushBuffer];

    }
}

void pollEvents(void) {
    @autoreleasepool {

    while (1) {
        NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny untilDate:nil inMode:NSDefaultRunLoopMode dequeue:YES];
        if (!event) break;

        [NSApp sendEvent:event];
    }

    }
}

void deleteWindow(WtkWindow *window) {
    @autoreleasepool {

    if (window->window) {
        if (window->view)
            [window->view release];
        [window->window setDelegate:nil];
        [window->window close];
    }

    }
}

void setWindowOrigin(WtkWindow *window, int x, int y) {
    @autoreleasepool {

    NSRect rect  = NSMakeRect(x, translateYCoordinate(y + [window->view frame].size.height - 1), 0, 0);
    NSRect frame = [window->window frameRectForContentRect:rect];
    [window->window setFrameOrigin:frame.origin];

    }
}

void setWindowSize(WtkWindow *window, int w, int h) {
    @autoreleasepool {

    NSRect contentRect = [window->window contentRectForFrameRect:[window->window frame]];
    contentRect.origin.y += contentRect.size.height - h;
    contentRect.size = NSMakeSize(w, h);
    [window->window setFrame:[window->window frameRectForContentRect:contentRect] display:YES];

    }
}

void setWindowTitle(WtkWindow *window, char const *title) {
    @autoreleasepool {

    [window->window setTitle:@(title)];

    }
}

#endif // __linux__ || __APPLE__

///
/// Common
///

static void defaultEventCallback(WtkWindow *window, WtkEvent const *event) {
    (void)window; (void)event;
}

static void validateDesc(WtkWindow *window) {
    if (!window->desc.callback) window->desc.callback = defaultEventCallback;
    if (!window->desc.title)    window->desc.title = "";
    if (!window->desc.w)        window->desc.w = 640;
    if (!window->desc.h)        window->desc.h = 480;
}

WtkWindow *WtkCreateWindow(WtkWindowDesc const *desc) {
    if (!desc || (G.num_windows == 0 && !init()))
        return NULL;

    WtkWindow *window = calloc(1, sizeof *window);
    if (!window) return NULL;

    window->desc = *desc;
    validateDesc(window);

    if (!createWindow(window)) {
        WtkDeleteWindow(window);
        return NULL;
    }

    G.num_windows++;
    return window;
}

void WtkMakeCurrent(WtkWindow *window) {
    if (window) makeCurrent(window);
}

void WtkSwapBuffers(WtkWindow *window) {
    if (window) swapBuffers(window);
}

void WtkPollEvents(void) {
    pollEvents();
}

void WtkDeleteWindow(WtkWindow *window) {
    if (!window) return;

    deleteWindow(window);
    free(window);

    if (--G.num_windows == 0)
        quit();
}

void WtkGetWindowRect(WtkWindow const *window, int *x, int *y, int *w, int *h) {
    if (x) *x = window ? window->desc.x : -1;
    if (y) *y = window ? window->desc.y : -1;
    if (w) *w = window ? window->desc.w : -1;
    if (h) *h = window ? window->desc.h : -1;
}

int WtkGetWindowShouldClose(WtkWindow const *window) {
    return window ? window->closed : 1;
}

void WtkSetWindowOrigin(WtkWindow *window, int x, int y) {
    if (!window || x < 0 || y < 0) return;

    setWindowOrigin(window, x, y);
    window->desc.x = x;
    window->desc.y = y;
}

void WtkSetWindowSize(WtkWindow *window, int w, int h) {
    if (!window || w < 0 || h < 0) return;

    setWindowSize(window, w, h);
    window->desc.w = w;
    window->desc.h = h;
}

void WtkSetWindowTitle(WtkWindow *window, char const *title) {
    if (!window || !title) return;

    setWindowTitle(window, title);
    window->desc.title = title;
}

void WtkSetWindowShouldClose(WtkWindow *window, int should_close) {
    if (window)
        window->closed = should_close;
}