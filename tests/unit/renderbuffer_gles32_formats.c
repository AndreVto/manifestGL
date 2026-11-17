#include "mgl/gl.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static struct {
    GLuint next;
    GLuint bound;
    GLuint last_query_binding;
    GLenum format;
    GLenum queried;
    unsigned storage_calls;
    unsigned multisample_calls;
    unsigned query_calls;
} cap;

static void fake_gen(GLsizei n, GLuint *names) {
    for (GLsizei i = 0; i < n; i++)
        names[i] = ++cap.next;
}

static void fake_delete(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static void fake_bind(GLenum target, GLuint name) {
    (void)target;
    cap.bound = name;
}

static void fake_storage(GLenum target, GLenum internalformat, GLsizei w, GLsizei h) {
    (void)target;
    (void)w;
    (void)h;
    cap.format = internalformat;
    cap.storage_calls++;
}

static void fake_storage_ms(GLenum target, GLsizei samples, GLenum internalformat, GLsizei w,
                            GLsizei h) {
    (void)target;
    (void)samples;
    (void)w;
    (void)h;
    cap.format = internalformat;
    cap.multisample_calls++;
}

static void fake_get(GLenum target, GLenum pname, GLint *result) {
    (void)target;
    cap.queried = pname;
    cap.query_calls++;
    cap.last_query_binding = cap.bound;
    *result = pname == GL_RENDERBUFFER_SAMPLES ? 1 : 7;
}

static int optional_formats_available;

static const GLubyte *fake_extensions(GLenum pname) {
    if (pname != GL_EXTENSIONS)
        return NULL;
    return (const GLubyte *)(optional_formats_available
                                 ? "GL_EXT_render_snorm GL_EXT_texture_norm16"
                                 : "");
}

static const GLenum formats[] = {GL_R8,
                                 GL_RG8,
                                 GL_RGB8,
                                 GL_RGBA8,
                                 GL_RGB565,
                                 GL_RGBA4,
                                 GL_RGB5_A1,
                                 GL_RGB10_A2,
                                 GL_RGB10_A2UI,
                                 GL_SRGB8_ALPHA8,
                                 GL_R16F,
                                 GL_RG16F,
                                 GL_RGBA16F,
                                 GL_R32F,
                                 GL_RG32F,
                                 GL_RGBA32F,
                                 GL_R11F_G11F_B10F,
                                 GL_R8I,
                                 GL_R8UI,
                                 GL_R16I,
                                 GL_R16UI,
                                 GL_R32I,
                                 GL_R32UI,
                                 GL_RG8I,
                                 GL_RG8UI,
                                 GL_RG16I,
                                 GL_RG16UI,
                                 GL_RG32I,
                                 GL_RG32UI,
                                 GL_RGBA8I,
                                 GL_RGBA8UI,
                                 GL_RGBA16I,
                                 GL_RGBA16UI,
                                 GL_RGBA32I,
                                 GL_RGBA32UI,
                                 GL_DEPTH_COMPONENT16,
                                 GL_DEPTH_COMPONENT24,
                                 GL_DEPTH_COMPONENT32F,
                                 GL_STENCIL_INDEX8,
                                 GL_DEPTH24_STENCIL8,
                                 GL_DEPTH32F_STENCIL8};
static const GLenum components[] = {GL_RENDERBUFFER_RED_SIZE,   GL_RENDERBUFFER_GREEN_SIZE,
                                    GL_RENDERBUFFER_BLUE_SIZE,  GL_RENDERBUFFER_ALPHA_SIZE,
                                    GL_RENDERBUFFER_DEPTH_SIZE, GL_RENDERBUFFER_STENCIL_SIZE};

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    GLuint names[2] = {0, 0};
    unsigned checks = 0, failures = 0;
    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    memset(&cap, 0, sizeof(cap));
    native.glesGenRenderbuffers = fake_gen;
    native.glesDeleteRenderbuffers = fake_delete;
    native.glesBindRenderbuffer = fake_bind;
    native.glesRenderbufferStorage = fake_storage;
    native.glesRenderbufferStorageMultisample = fake_storage_ms;
    native.glesGetRenderbufferParameteriv = fake_get;
    native.glesGetString = fake_extensions;
    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return 1;
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    mgl_context_set_current_for_test(&ctx);
    glCreateRenderbuffers(2, names);
    glBindRenderbuffer(GL_RENDERBUFFER, names[0]);
    if (glGetError() != GL_NO_ERROR) {
        fprintf(stderr, "renderbuffer initialization failed\n");
        return 1;
    }
    for (size_t i = 0; i < sizeof(formats) / sizeof(formats[0]); i++) {
        const GLenum format = formats[i];
        GLint value = 0;
        unsigned before = cap.storage_calls;
        glRenderbufferStorage(GL_RENDERBUFFER, format, 16, 8);
        checks++;
        if (glGetError() != GL_NO_ERROR || cap.storage_calls != before + 1 ||
            cap.format != format) {
            fprintf(stderr, "bound storage rejected GLES renderbuffer format 0x%x\n", format);
            failures++;
        }
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &value);
        checks++;
        if (glGetError() != GL_NO_ERROR || value != (GLint)format) {
            fprintf(stderr, "bound format query 0x%x\n", format);
            failures++;
        }
        before = cap.storage_calls;
        glNamedRenderbufferStorage(names[1], format, 8, 4);
        checks++;
        {
            struct mgl_renderbuffer *bound = mgl_renderbuffer_find(&ctx, names[0]);
            checks++;
            if (glGetError() != GL_NO_ERROR || cap.storage_calls != before + 1 ||
                cap.format != format || bound == NULL || cap.bound != bound->native_name) {
                fprintf(stderr, "named storage 0x%x\n", format);
                failures++;
            }
        }
        glGetNamedRenderbufferParameteriv(names[1], GL_RENDERBUFFER_INTERNAL_FORMAT, &value);
        checks++;
        if (glGetError() != GL_NO_ERROR || value != (GLint)format) {
            fprintf(stderr, "named format query 0x%x\n", format);
            failures++;
        }
        before = cap.storage_calls;
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, 0, format, 8, 4);
        checks++;
        if (glGetError() != GL_NO_ERROR || cap.storage_calls != before + 1 ||
            cap.format != format) {
            fprintf(stderr, "zero-sample storage 0x%x\n", format);
            failures++;
        }
        before = cap.storage_calls;
        glNamedRenderbufferStorageMultisample(names[1], 0, format, 8, 4);
        checks++;
        if (glGetError() != GL_NO_ERROR || cap.storage_calls != before + 1 ||
            cap.format != format) {
            fprintf(stderr, "named zero-sample storage 0x%x\n", format);
            failures++;
        }
    }
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 8, 8);
    (void)glGetError();
    for (size_t i = 0; i < sizeof(components) / sizeof(components[0]); i++) {
        GLint value = 0;
        unsigned before = cap.query_calls;
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, components[i], &value);
        checks++;
        if (glGetError() != GL_NO_ERROR || cap.query_calls != before + 1 ||
            cap.queried != components[i] || value != 7) {
            fprintf(stderr, "bound component query 0x%x\n", components[i]);
            failures++;
        }
        before = cap.query_calls;
        glGetNamedRenderbufferParameteriv(names[1], components[i], &value);
        checks++;
        if (glGetError() != GL_NO_ERROR || cap.query_calls != before + 1 ||
            cap.queried != components[i] || value != 7) {
            fprintf(stderr, "named component query 0x%x\n", components[i]);
            failures++;
        }
    }
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 1, GL_RGBA8, 8, 8);
    checks++;
    if (glGetError() != GL_NO_ERROR || cap.multisample_calls != 1) {
        fprintf(stderr, "multisample RGBA8\n");
        failures++;
    }
    glNamedRenderbufferStorageMultisample(names[1], 1, GL_RGBA8, 8, 8);
    checks++;
    if (glGetError() != GL_NO_ERROR || cap.multisample_calls != 2) {
        fprintf(stderr, "named multisample RGBA8\n");
        failures++;
    }
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8_SNORM, 8, 8);
    checks++;
    if (glGetError() != GL_INVALID_ENUM) {
        fprintf(stderr, "unsupported SNORM renderbuffer\n");
        failures++;
    }
    {
        static const struct {
            GLenum format, native_format;
        } aliases[] = {
            {GL_RGB5, GL_RGB565}, {GL_RGB16F, GL_RGBA16F},
            {GL_RGB32F_ARB, GL_RGBA32F}, {GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT32F},
            {GL_DEPTH_COMPONENT, GL_DEPTH_COMPONENT24}, {GL_STENCIL_INDEX, GL_STENCIL_INDEX8}
        };
        for (size_t i = 0; i < sizeof(aliases) / sizeof(aliases[0]); ++i) {
            unsigned before = cap.storage_calls;
            glRenderbufferStorage(GL_RENDERBUFFER, aliases[i].format, 8, 8);
            checks++;
            if (glGetError() != GL_NO_ERROR || cap.storage_calls != before + 1u ||
                cap.format != aliases[i].native_format) {
                fprintf(stderr, "desktop renderbuffer conversion 0x%x\n", aliases[i].format);
                failures++;
            }
        }
    }
    {
        static const GLenum optional_formats[] = {
            GL_R8_SNORM, GL_RG8_SNORM, GL_RGBA8_SNORM,
            GL_R16, GL_RG16, GL_RGBA16,
            GL_R16_SNORM, GL_RG16_SNORM, GL_RGBA16_SNORM
        };
        optional_formats_available = 1;
        for (size_t i = 0; i < sizeof(optional_formats) / sizeof(optional_formats[0]); ++i) {
            unsigned before = cap.storage_calls;
            glRenderbufferStorage(GL_RENDERBUFFER, optional_formats[i], 8, 8);
            checks++;
            if (glGetError() != GL_NO_ERROR || cap.storage_calls != before + 1u ||
                cap.format != optional_formats[i]) {
                fprintf(stderr, "extension renderbuffer format 0x%x\n", optional_formats[i]);
                failures++;
            }
        }
        optional_formats_available = 0;
    }
    glDeleteRenderbuffers(2, names);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    printf("GLES 3.2 renderbuffer format matrix: %u checks, %u failures\n", checks, failures);
    return failures ? 1 : 0;
}
