# wtk

A small, simple, cross-platform window and OpenGL context library

```c
#include "wtk/wtk.h"

void handler(WtkWindow *window, WtkEventType type, WtkEvent const *event) {
    if (type == WtkEventType_KeyDown && event->key.code == WtkKey_Escape)
        WtkSetWindowShouldClose(window, true);
}

int main(void) {
    WtkWindow *window = WtkCreateWindow(&(WtkWindowDesc){
        .title = "My Title",
        .event_handler = handler
    });
    WtkMakeCurrent(window);

    while (!WtkGetWindowShouldClose(window)) {
        WtkSwapBuffers(window);
        WtkPollEvents();
    }

    WtkDeleteWindow(window);
}
```

## Building
### Unix
- `make` will build the library and the examples
- `make libwtk.a` will build just the library

### Windows
`wtk` only supports MacOS and Linux right now as those are the platforms I use, but I want to get a Windows backend up and running at some point.