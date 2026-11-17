#include "glx/drawable.h"
#include "core/context.h"
#include "core/render_diag.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <xcb/xcb.h>

static pthread_mutex_t registry_lock = PTHREAD_MUTEX_INITIALIZER;
static struct mgl_glx_drawable_record *registry;
static atomic_uint next_id = ATOMIC_VAR_INIT(0x60000000u);

static int query_x_drawable(Display *dpy, Drawable drawable, int require_window, int *screen,
                            VisualID *visual_id, unsigned *width, unsigned *height,
                            unsigned *depth) {
    xcb_connection_t *conn;
    xcb_get_geometry_reply_t *geometry;
    xcb_get_window_attributes_reply_t *attributes;
    xcb_generic_error_t *error = NULL;
    int selected = -1, i, result = 0;
    if (dpy == NULL || drawable == 0)
        return 0;
    XSync(dpy, False);
    conn = xcb_connect(DisplayString(dpy), NULL);
    if (conn == NULL || xcb_connection_has_error(conn)) {
        if (conn != NULL)
            xcb_disconnect(conn);
        return 0;
    }
    geometry =
        xcb_get_geometry_reply(conn, xcb_get_geometry(conn, (xcb_drawable_t)drawable), &error);
    free(error);
    error = NULL;
    if (geometry == NULL)
        goto done;
    for (i = 0; i < ScreenCount(dpy); ++i) {
        if (RootWindow(dpy, i) == geometry->root) {
            selected = i;
            break;
        }
    }
    if (selected < 0) {
        free(geometry);
        goto done;
    }
    attributes = xcb_get_window_attributes_reply(
        conn, xcb_get_window_attributes(conn, (xcb_window_t)drawable), &error);
    free(error);
    if ((attributes != NULL) != require_window) {
        free(attributes);
        free(geometry);
        goto done;
    }
    if (screen != NULL)
        *screen = selected;
    if (visual_id != NULL)
        *visual_id = attributes != NULL ? (VisualID)attributes->visual : 0;
    if (width != NULL)
        *width = geometry->width;
    if (height != NULL)
        *height = geometry->height;
    if (depth != NULL)
        *depth = geometry->depth;
    result = 1;
    free(attributes);
    free(geometry);
done:
    xcb_disconnect(conn);
    return result;
}

int mgl_glx_native_window_info(Display *dpy, Window window, int *screen, VisualID *visual_id,
                               unsigned *width, unsigned *height) {
    return query_x_drawable(dpy, window, 1, screen, visual_id, width, height, NULL);
}

int mgl_glx_native_pixmap_info(Display *dpy, Pixmap pixmap, int *screen, unsigned *width,
                               unsigned *height, unsigned *depth) {
    return query_x_drawable(dpy, pixmap, 0, screen, NULL, width, height, depth);
}

static struct mgl_glx_drawable_record *find_locked(Display *dpy, GLXDrawable drawable) {
    struct mgl_glx_drawable_record *it;
    for (it = registry; it != NULL; it = it->next) {
        if (it->id == drawable && (dpy == NULL || it->display == dpy))
            return it;
    }
    return NULL;
}

GLXDrawable mgl_glx_drawable_create(Display *dpy, enum mgl_glx_drawable_kind kind,
                                    Drawable native_drawable, unsigned width, unsigned height) {
    struct mgl_glx_drawable_record *rec = calloc(1, sizeof(*rec));
    GLXDrawable id;
    if (rec == NULL)
        return 0;
    do {
        id = (GLXDrawable)atomic_fetch_add_explicit(&next_id, 1u, memory_order_relaxed);
    } while (id == 0);
    rec->id = id;
    rec->display = dpy;
    rec->kind = kind;
    rec->native_drawable = native_drawable;
    rec->screen = -1;
    rec->width = width == 0u ? 1u : width;
    rec->height = height == 0u ? 1u : height;
    rec->swap_interval = 1;
    (void)pthread_mutex_lock(&registry_lock);
    if (kind == MGL_GLX_DRAWABLE_WINDOW && native_drawable != 0) {
        const struct mgl_glx_drawable_record *existing;
        for (existing = registry; existing != NULL; existing = existing->next) {
            if (existing->display == dpy && existing->kind == kind &&
                existing->native_drawable == native_drawable) {
                (void)pthread_mutex_unlock(&registry_lock);
                free(rec);
                return 0;
            }
        }
    }
    rec->next = registry;
    registry = rec;
    (void)pthread_mutex_unlock(&registry_lock);
    return id;
}

int mgl_glx_window_has_config(Display *dpy, Window window) {
    const struct mgl_glx_drawable_record *rec;
    int found = 0;
    (void)pthread_mutex_lock(&registry_lock);
    for (rec = registry; rec != NULL; rec = rec->next) {
        if (rec->display == dpy && rec->kind == MGL_GLX_DRAWABLE_WINDOW &&
            rec->native_drawable == window) {
            found = 1;
            break;
        }
    }
    (void)pthread_mutex_unlock(&registry_lock);
    return found;
}

int mgl_glx_drawable_set_format(Display *dpy, GLXDrawable drawable, int screen, int fbconfig_id,
                                VisualID visual_id) {
    struct mgl_glx_drawable_record *rec;
    int found;
    (void)pthread_mutex_lock(&registry_lock);
    rec = find_locked(dpy, drawable);
    found = rec != NULL;
    if (found) {
        rec->screen = screen;
        rec->fbconfig_id = fbconfig_id;
        rec->visual_id = visual_id;
    }
    (void)pthread_mutex_unlock(&registry_lock);
    return found;
}

int mgl_glx_drawable_get_format(Display *dpy, GLXDrawable drawable, int *screen, int *fbconfig_id,
                                VisualID *visual_id) {
    struct mgl_glx_drawable_record *rec;
    int local_screen = -1, local_fbconfig = 0, found;
    VisualID local_visual = 0;
    (void)pthread_mutex_lock(&registry_lock);
    rec = find_locked(dpy, drawable);
    found = rec != NULL;
    if (found) {
        local_screen = rec->screen;
        local_fbconfig = rec->fbconfig_id;
        local_visual = rec->visual_id;
    }
    (void)pthread_mutex_unlock(&registry_lock);
    if (!found && !mgl_glx_native_window_info(dpy, (Window)drawable, &local_screen, &local_visual,
                                              NULL, NULL))
        return 0;
    if (screen != NULL)
        *screen = local_screen;
    if (fbconfig_id != NULL)
        *fbconfig_id = local_fbconfig;
    if (visual_id != NULL)
        *visual_id = local_visual;
    return 1;
}

void mgl_glx_drawable_destroy(Display *dpy, GLXDrawable drawable) {
    struct mgl_glx_drawable_record **link;
    (void)pthread_mutex_lock(&registry_lock);
    link = &registry;
    while (*link != NULL) {
        if ((*link)->id == drawable && (dpy == NULL || (*link)->display == dpy)) {
            struct mgl_glx_drawable_record *victim = *link;
            *link = victim->next;
            if (victim->watch_display != NULL)
                XCloseDisplay(victim->watch_display);
            free(victim);
            break;
        }
        link = &(*link)->next;
    }
    (void)pthread_mutex_unlock(&registry_lock);
}

static int query_native_geometry(Display *dpy, Drawable drawable, unsigned *width,
                                 unsigned *height) {
    MGL_DEEP_SCOPE(mgl_context_current(), MGL_DEEP_GLX_DRAWABLE_INFO);
    Window root;
    int x, y;
    unsigned border, depth;
    unsigned w = 0, h = 0;
    if (dpy == NULL || drawable == 0)
        return 0;
    if (!XGetGeometry(dpy, drawable, &root, &x, &y, &w, &h, &border, &depth))
        return 0;
    if (width != NULL)
        *width = w;
    if (height != NULL)
        *height = h;
    return 1;
}

static void drain_watch_locked(struct mgl_glx_drawable_record *rec, int read_socket) {
    XEvent event;
    if (rec->watch_display == NULL || rec->watch_destroyed)
        return;
    if (read_socket)
        (void)XEventsQueued(rec->watch_display, QueuedAfterReading);
    while (XEventsQueued(rec->watch_display, QueuedAlready) > 0) {
        XNextEvent(rec->watch_display, &event);
        if (event.xany.window != rec->native_drawable)
            continue;
        if (event.type == ConfigureNotify) {
            if (event.xconfigure.width > 0 && event.xconfigure.height > 0) {
                rec->width = (unsigned)event.xconfigure.width;
                rec->height = (unsigned)event.xconfigure.height;
            }
        } else if (event.type == DestroyNotify) {
            rec->watch_destroyed = 1;
        }
    }
}

int mgl_glx_drawable_watch(Display *dpy, GLXDrawable drawable) {
    struct mgl_glx_drawable_record *rec;
    Display *monitor;
    Window root;
    int x, y;
    unsigned width, height, border, depth;
    const char *name;
    Drawable native_drawable;
    int result = 0;
    if (dpy == NULL)
        return 0;
    (void)pthread_mutex_lock(&registry_lock);
    rec = find_locked(dpy, drawable);
    if (rec == NULL || rec->kind != MGL_GLX_DRAWABLE_WINDOW || rec->native_drawable == 0) {
        (void)pthread_mutex_unlock(&registry_lock);
        return 0;
    }
    if (rec->watch_display != NULL) {
        (void)pthread_mutex_unlock(&registry_lock);
        return 1;
    }
    native_drawable = rec->native_drawable;
    (void)pthread_mutex_unlock(&registry_lock);
    name = DisplayString(dpy);
    monitor = XOpenDisplay(name);
    if (monitor == NULL)
        return 0;
    XSelectInput(monitor, (Window)native_drawable, StructureNotifyMask);
    if (XGetGeometry(monitor, native_drawable, &root, &x, &y, &width, &height, &border, &depth) &&
        width != 0u && height != 0u) {
        XEvent event;
        while (XEventsQueued(monitor, QueuedAlready) > 0)
            XNextEvent(monitor, &event);
        (void)pthread_mutex_lock(&registry_lock);
        rec = find_locked(dpy, drawable);
        if (rec != NULL && rec->native_drawable == native_drawable && rec->watch_display == NULL) {
            rec->watch_display = monitor;
            rec->watch_destroyed = 0;
            rec->width = width;
            rec->height = height;
            result = 1;
        }
        (void)pthread_mutex_unlock(&registry_lock);
    }
    if (!result)
        XCloseDisplay(monitor);
    return result;
}

int mgl_glx_drawable_watch_poll(Display *dpy, GLXDrawable drawable) {
    struct mgl_glx_drawable_record *rec;
    int result = 0;
    (void)pthread_mutex_lock(&registry_lock);
    rec = find_locked(dpy, drawable);
    if (rec != NULL) {
        if (rec->watch_display != NULL) {
            drain_watch_locked(rec, 1);
            result = rec->watch_destroyed ? -1 : 1;
        } else if (rec->native_drawable == 0) {
            result = 1;
        }
    }
    (void)pthread_mutex_unlock(&registry_lock);
    return result;
}

int mgl_glx_drawable_refresh(Display *dpy, GLXDrawable drawable) {
    struct mgl_glx_drawable_record *rec;
    Drawable native_drawable = 0;
    enum mgl_glx_drawable_kind kind = MGL_GLX_DRAWABLE_PBUFFER;
    unsigned width, height;
    int updated = 0;
    (void)pthread_mutex_lock(&registry_lock);
    rec = find_locked(dpy, drawable);
    if (rec != NULL) {
        native_drawable = rec->native_drawable;
        kind = rec->kind;
    }
    (void)pthread_mutex_unlock(&registry_lock);
    if (rec == NULL || kind == MGL_GLX_DRAWABLE_PBUFFER || native_drawable == 0)
        return 0;
    (void)pthread_mutex_lock(&registry_lock);
    rec = find_locked(dpy, drawable);
    if (rec != NULL && rec->native_drawable == native_drawable && rec->kind == kind &&
        rec->watch_display != NULL) {
        if (!rec->watch_destroyed &&
            query_native_geometry(rec->watch_display, native_drawable, &width, &height)) {
            drain_watch_locked(rec, 0);
            if (!rec->watch_destroyed) {
                rec->width = width;
                rec->height = height;
                updated = 1;
            }
        }
        (void)pthread_mutex_unlock(&registry_lock);
        return updated;
    }
    (void)pthread_mutex_unlock(&registry_lock);
    if (!query_native_geometry(dpy, native_drawable, &width, &height))
        return 0;
    (void)pthread_mutex_lock(&registry_lock);
    rec = find_locked(dpy, drawable);
    if (rec != NULL && rec->native_drawable == native_drawable && rec->kind == kind) {
        rec->width = width;
        rec->height = height;
        updated = 1;
    }
    (void)pthread_mutex_unlock(&registry_lock);
    return updated;
}

int mgl_glx_drawable_info(Display *dpy, GLXDrawable drawable, enum mgl_glx_drawable_kind *kind,
                          Drawable *native_drawable, unsigned *width, unsigned *height,
                          int *swap_interval) {
    struct mgl_glx_drawable_record *rec;
    enum mgl_glx_drawable_kind local_kind = MGL_GLX_DRAWABLE_WINDOW;
    Drawable local_native = drawable;
    unsigned local_width = 1u, local_height = 1u;
    int local_interval = 1;
    int found = 0;

    (void)pthread_mutex_lock(&registry_lock);
    rec = find_locked(dpy, drawable);
    if (rec != NULL && rec->watch_destroyed) {
        (void)pthread_mutex_unlock(&registry_lock);
        return 0;
    }
    if (rec != NULL) {
        local_kind = rec->kind;
        local_native = rec->native_drawable;
        local_width = rec->width;
        local_height = rec->height;
        local_interval = rec->swap_interval;
        found = 1;
    }
    (void)pthread_mutex_unlock(&registry_lock);

    if (!found &&
        !mgl_glx_native_window_info(dpy, (Window)drawable, NULL, NULL, &local_width, &local_height))
        return 0;

    if (kind != NULL)
        *kind = local_kind;
    if (native_drawable != NULL)
        *native_drawable = local_native;
    if (width != NULL)
        *width = local_width;
    if (height != NULL)
        *height = local_height;
    if (swap_interval != NULL)
        *swap_interval = local_interval;
    return 1;
}

int mgl_glx_drawable_set_pbuffer_attributes(Display *dpy, GLXDrawable drawable,
                                            int preserved_contents, int largest_pbuffer) {
    struct mgl_glx_drawable_record *rec;
    int found;
    (void)pthread_mutex_lock(&registry_lock);
    rec = find_locked(dpy, drawable);
    found = rec != NULL && rec->kind == MGL_GLX_DRAWABLE_PBUFFER;
    if (found) {
        rec->preserved_contents = preserved_contents;
        rec->largest_pbuffer = largest_pbuffer;
    }
    (void)pthread_mutex_unlock(&registry_lock);
    return found;
}

int mgl_glx_drawable_get_pbuffer_attributes(Display *dpy, GLXDrawable drawable,
                                            int *preserved_contents, int *largest_pbuffer) {
    struct mgl_glx_drawable_record *rec;
    int found;
    (void)pthread_mutex_lock(&registry_lock);
    rec = find_locked(dpy, drawable);
    found = rec != NULL && rec->kind == MGL_GLX_DRAWABLE_PBUFFER;
    if (found) {
        if (preserved_contents != NULL)
            *preserved_contents = rec->preserved_contents;
        if (largest_pbuffer != NULL)
            *largest_pbuffer = rec->largest_pbuffer;
    }
    (void)pthread_mutex_unlock(&registry_lock);
    return found;
}

int mgl_glx_drawable_set_swap_interval(Display *dpy, GLXDrawable drawable, int interval) {
    struct mgl_glx_drawable_record *rec;
    int result = 0;
    (void)pthread_mutex_lock(&registry_lock);
    rec = find_locked(dpy, drawable);
    if (rec != NULL) {
        rec->swap_interval = interval;
        rec->swap_interval_explicit = 1;
        result = 1;
    }
    (void)pthread_mutex_unlock(&registry_lock);
    return result;
}

/* Preserve an application-selected interval across native registration. */
int mgl_glx_drawable_set_initial_swap_interval(Display *dpy, GLXDrawable drawable, int interval) {
    struct mgl_glx_drawable_record *rec;
    int result = 0;
    (void)pthread_mutex_lock(&registry_lock);
    rec = find_locked(dpy, drawable);
    if (rec != NULL) {
        if (!rec->swap_interval_explicit)
            rec->swap_interval = interval;
        result = 1;
    }
    (void)pthread_mutex_unlock(&registry_lock);
    return result;
}

int mgl_glx_drawable_set_event_mask(Display *dpy, GLXDrawable drawable, unsigned long event_mask) {
    struct mgl_glx_drawable_record *rec;
    int result = 0;
    (void)pthread_mutex_lock(&registry_lock);
    rec = find_locked(dpy, drawable);
    if (rec != NULL &&
        (rec->kind == MGL_GLX_DRAWABLE_WINDOW || rec->kind == MGL_GLX_DRAWABLE_PBUFFER)) {
        rec->event_mask = event_mask;
        result = 1;
    }
    (void)pthread_mutex_unlock(&registry_lock);
    return result;
}

int mgl_glx_drawable_get_event_mask(Display *dpy, GLXDrawable drawable, unsigned long *event_mask) {
    struct mgl_glx_drawable_record *rec;
    int result = 0;
    (void)pthread_mutex_lock(&registry_lock);
    rec = find_locked(dpy, drawable);
    if (rec != NULL &&
        (rec->kind == MGL_GLX_DRAWABLE_WINDOW || rec->kind == MGL_GLX_DRAWABLE_PBUFFER)) {
        if (event_mask != NULL)
            *event_mask = rec->event_mask;
        result = 1;
    }
    (void)pthread_mutex_unlock(&registry_lock);
    return result;
}

int mgl_glx_drawable_native_identity(Display *dpy, GLXDrawable drawable,
                                     enum mgl_glx_drawable_kind *kind, Drawable *native_drawable) {
    struct mgl_glx_drawable_record *rec;
    int found = 0;
    (void)pthread_mutex_lock(&registry_lock);
    rec = find_locked(dpy, drawable);
    if (rec != NULL) {
        if (kind != NULL)
            *kind = rec->kind;
        if (native_drawable != NULL)
            *native_drawable = rec->native_drawable;
        found = 1;
    }
    (void)pthread_mutex_unlock(&registry_lock);
    return found;
}
