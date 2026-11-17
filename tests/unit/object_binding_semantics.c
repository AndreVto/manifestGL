#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"

#include <stdio.h>
#include <string.h>

static GLuint next_native_name = 700u;
static unsigned gen_fb_calls, gen_rb_calls, gen_vao_calls;
static unsigned bind_fb_calls, bind_rb_calls, bind_vao_calls;
static GLenum last_fb_target, last_rb_target;
static GLuint last_fb_name, last_rb_name, last_vao_name;

static void fake_gen_fb(GLsizei n, GLuint *out) {
    GLsizei i;
    ++gen_fb_calls;
    for (i = 0; i < n; ++i)
        out[i] = next_native_name++;
}

static void fake_gen_rb(GLsizei n, GLuint *out) {
    GLsizei i;
    ++gen_rb_calls;
    for (i = 0; i < n; ++i)
        out[i] = next_native_name++;
}

static void fake_gen_vao(GLsizei n, GLuint *out) {
    GLsizei i;
    ++gen_vao_calls;
    for (i = 0; i < n; ++i)
        out[i] = next_native_name++;
}

static void fake_bind_fb(GLenum target, GLuint name) {
    ++bind_fb_calls;
    last_fb_target = target;
    last_fb_name = name;
}

static void fake_bind_rb(GLenum target, GLuint name) {
    ++bind_rb_calls;
    last_rb_target = target;
    last_rb_name = name;
}

static void fake_bind_vao(GLuint name) {
    ++bind_vao_calls;
    last_vao_name = name;
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static void fake_delete(GLsizei n, const GLuint *names) {
    (void)n;
    (void)names;
}

static int fail(int n, const char *why) {
    fprintf(stderr, "object binding semantics failure %d: %s\n", n, why);
    return n;
}

static int expect_error(GLenum expected, int n, const char *why) {
    GLenum got = glGetError();
    if (got != expected) {
        fprintf(stderr, "object binding semantics failure %d: %s (got 0x%x expected 0x%x)\n", n,
                why, (unsigned)got, (unsigned)expected);
        return n;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_framebuffer *fb_obj;
    struct mgl_renderbuffer *rb_obj;
    struct mgl_vertex_array *vao_obj;
    GLuint fb = 0u, rb = 0u, vao = 0u;
    GLuint list, list_fb = 0u, list_rb = 0u, list_vao = 0u;
    GLint value;
    unsigned before_gen_fb, before_gen_rb, before_gen_vao;
    unsigned before_bind_fb, before_bind_rb, before_bind_vao;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenFramebuffers = fake_gen_fb;
    native.glesDeleteFramebuffers = fake_delete;
    native.glesBindFramebuffer = fake_bind_fb;
    native.glesGenRenderbuffers = fake_gen_rb;
    native.glesDeleteRenderbuffers = fake_delete;
    native.glesBindRenderbuffer = fake_bind_rb;
    native.glesGenVertexArrays = fake_gen_vao;
    native.glesDeleteVertexArrays = fake_delete;
    native.glesBindVertexArray = fake_bind_vao;
    native.glesBindBuffer = fake_bind_buffer;

    ctx.native_dispatch = &native;
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_state_init(&ctx.canonical_state, 320, 240);
    mgl_compat_state_init(&ctx.compat);
    mgl_pixel_transfer_init(&ctx.pixel_transfer);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.next_vao_name = 1u;
    ctx.next_framebuffer_name = 1u;
    mgl_context_set_current_for_test(&ctx);

    glGenFramebuffers(1, &fb);
    glGenRenderbuffers(1, &rb);
    glGenVertexArrays(1, &vao);
    if ((rc = expect_error(GL_NO_ERROR, 2, "direct generation")) != 0 || fb == 0u || rb == 0u ||
        vao == 0u)
        return fail(2, "generated names");
    if (gen_fb_calls != 1u || gen_rb_calls != 1u || gen_vao_calls != 1u)
        return fail(3, "native generation counts");
    if (glIsFramebuffer(fb) || glIsRenderbuffer(rb) || glIsVertexArray(vao) ||
        glGetError() != GL_NO_ERROR)
        return fail(4, "generated names realized before bind");

    fb_obj = mgl_framebuffer_find(&ctx, fb);
    rb_obj = mgl_renderbuffer_find(&ctx, rb);
    vao_obj = mgl_vao_find(&ctx, vao);
    if (fb_obj == NULL || rb_obj == NULL || vao_obj == NULL)
        return fail(5, "reserved object lookup");

    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glBindVertexArray(vao);
    if ((rc = expect_error(GL_NO_ERROR, 6, "direct binds")) != 0)
        return rc;
    if (last_fb_name != fb_obj->native_name || last_rb_target != GL_RENDERBUFFER ||
        last_rb_name != rb_obj->native_name || last_vao_name != vao_obj->native_name)
        return fail(7, "interface/native name translation");
    if (!glIsFramebuffer(fb) || !glIsRenderbuffer(rb) || !glIsVertexArray(vao) ||
        glGetError() != GL_NO_ERROR)
        return fail(8, "bind realization");

    value = -1;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)fb)
        return fail(9, "draw framebuffer binding query");
    value = -1;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)fb)
        return fail(10, "read framebuffer binding query");
    value = -1;
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)rb)
        return fail(11, "renderbuffer binding query");
    value = -1;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &value);
    if (glGetError() != GL_NO_ERROR || value != (GLint)vao)
        return fail(12, "VAO binding query");

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0u);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0u);
    glBindRenderbuffer(GL_RENDERBUFFER, 0u);
    glBindVertexArray(0u);
    if ((rc = expect_error(GL_NO_ERROR, 13, "zero binds")) != 0 || ctx.draw_framebuffer != 0u ||
        ctx.read_framebuffer != 0u || ctx.renderbuffer != 0u || ctx.current_vao != &ctx.default_vao)
        return fail(13, "zero binding state");

    before_gen_fb = gen_fb_calls;
    before_bind_fb = bind_fb_calls;
    glBindFramebuffer(GL_FRAMEBUFFER, 999001u);
    if ((rc = expect_error(GL_INVALID_OPERATION, 14, "arbitrary framebuffer name")) != 0 ||
        gen_fb_calls != before_gen_fb || bind_fb_calls != before_bind_fb ||
        mgl_framebuffer_find(&ctx, 999001u) != NULL)
        return fail(14, "arbitrary framebuffer was realized");
    glBindFramebuffer(0xdeadbeefu, fb);
    if ((rc = expect_error(GL_INVALID_ENUM, 15, "framebuffer target validation")) != 0)
        return rc;

    before_gen_rb = gen_rb_calls;
    before_bind_rb = bind_rb_calls;
    glBindRenderbuffer(GL_RENDERBUFFER, 999002u);
    if ((rc = expect_error(GL_INVALID_OPERATION, 16, "arbitrary renderbuffer name")) != 0 ||
        gen_rb_calls != before_gen_rb || bind_rb_calls != before_bind_rb ||
        mgl_renderbuffer_find(&ctx, 999002u) != NULL)
        return fail(16, "arbitrary renderbuffer was realized");
    glBindRenderbuffer(GL_FRAMEBUFFER, rb);
    if ((rc = expect_error(GL_INVALID_ENUM, 17, "renderbuffer target validation")) != 0)
        return rc;

    before_bind_vao = bind_vao_calls;
    glBindVertexArray(999003u);
    if ((rc = expect_error(GL_INVALID_OPERATION, 18, "arbitrary VAO name")) != 0 ||
        bind_vao_calls != before_bind_vao || mgl_vao_find(&ctx, 999003u) != NULL)
        return fail(18, "arbitrary VAO was realized");

    /* Begin/End legality wins before argument/name validation and leaves state/native untouched. */
    before_gen_fb = gen_fb_calls;
    fb_obj = mgl_framebuffer_find(&ctx, fb);
    glBegin(GL_POINTS);
    glGenFramebuffers(-1, &list_fb);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 19, "GenFramebuffers Begin/End")) != 0 ||
        gen_fb_calls != before_gen_fb)
        return fail(19, "GenFramebuffers reached native in Begin/End");
    before_gen_rb = gen_rb_calls;
    glBegin(GL_POINTS);
    glGenRenderbuffers(-1, &list_rb);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 20, "GenRenderbuffers Begin/End")) != 0 ||
        gen_rb_calls != before_gen_rb)
        return fail(20, "GenRenderbuffers reached native in Begin/End");
    before_gen_vao = gen_vao_calls;
    glBegin(GL_POINTS);
    glGenVertexArrays(-1, &list_vao);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 21, "GenVertexArrays Begin/End")) != 0 ||
        gen_vao_calls != before_gen_vao)
        return fail(21, "GenVertexArrays reached native in Begin/End");

    before_bind_fb = bind_fb_calls;
    glBegin(GL_POINTS);
    glBindFramebuffer(0xdeadbeefu, 999001u);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 22, "BindFramebuffer Begin/End ordering")) != 0 ||
        bind_fb_calls != before_bind_fb || ctx.draw_framebuffer != 0u || ctx.read_framebuffer != 0u)
        return fail(22, "BindFramebuffer mutated inside Begin/End");
    before_bind_rb = bind_rb_calls;
    glBegin(GL_POINTS);
    glBindRenderbuffer(GL_FRAMEBUFFER, 999002u);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 23, "BindRenderbuffer Begin/End ordering")) != 0 ||
        bind_rb_calls != before_bind_rb || ctx.renderbuffer != 0u)
        return fail(23, "BindRenderbuffer mutated inside Begin/End");
    before_bind_vao = bind_vao_calls;
    glBegin(GL_POINTS);
    glBindVertexArray(vao);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 24, "BindVertexArray Begin/End")) != 0 ||
        bind_vao_calls != before_bind_vao || ctx.current_vao != &ctx.default_vao)
        return fail(24, "BindVertexArray mutated inside Begin/End");

    list = glGenLists(1);
    if (list == 0u)
        return fail(25, "display-list allocation");
    before_gen_fb = gen_fb_calls;
    before_gen_rb = gen_rb_calls;
    before_gen_vao = gen_vao_calls;
    before_bind_fb = bind_fb_calls;
    before_bind_rb = bind_rb_calls;
    before_bind_vao = bind_vao_calls;
    glNewList(list, GL_COMPILE);
    glGenFramebuffers(1, &list_fb);
    glGenRenderbuffers(1, &list_rb);
    glGenVertexArrays(1, &list_vao);
    glBindFramebuffer(GL_FRAMEBUFFER, list_fb);
    glBindRenderbuffer(GL_RENDERBUFFER, list_rb);
    glBindVertexArray(list_vao);
    glEndList();
    if ((rc = expect_error(GL_NO_ERROR, 26, "display-list compile")) != 0)
        return rc;
    if (gen_fb_calls != before_gen_fb + 1u || gen_rb_calls != before_gen_rb + 1u ||
        gen_vao_calls != before_gen_vao + 1u || bind_fb_calls <= before_bind_fb ||
        bind_rb_calls != before_bind_rb + 1u || bind_vao_calls != before_bind_vao + 1u)
        return fail(27, "display-list compile did not execute object commands immediately");
    if (ctx.draw_framebuffer != list_fb || ctx.read_framebuffer != list_fb ||
        ctx.renderbuffer != list_rb || ctx.current_vao == NULL || ctx.current_vao->name != list_vao)
        return fail(28, "display-list compile-time binding state");

    glBindFramebuffer(GL_FRAMEBUFFER, 0u);
    glBindRenderbuffer(GL_RENDERBUFFER, 0u);
    glBindVertexArray(0u);
    before_gen_fb = gen_fb_calls;
    before_gen_rb = gen_rb_calls;
    before_gen_vao = gen_vao_calls;
    before_bind_fb = bind_fb_calls;
    before_bind_rb = bind_rb_calls;
    before_bind_vao = bind_vao_calls;
    glCallList(list);
    if ((rc = expect_error(GL_NO_ERROR, 29, "display-list replay")) != 0)
        return rc;
    if (gen_fb_calls != before_gen_fb || gen_rb_calls != before_gen_rb ||
        gen_vao_calls != before_gen_vao || bind_fb_calls != before_bind_fb ||
        bind_rb_calls != before_bind_rb || bind_vao_calls != before_bind_vao ||
        ctx.draw_framebuffer != 0u || ctx.read_framebuffer != 0u || ctx.renderbuffer != 0u ||
        ctx.current_vao != &ctx.default_vao)
        return fail(30, "object commands were incorrectly retained in display list");

    if (glXGetProcAddress((const GLubyte *)"glGenFramebuffers") == NULL ||
        glXGetProcAddress((const GLubyte *)"glBindFramebuffer") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGenRenderbuffers") == NULL ||
        glXGetProcAddress((const GLubyte *)"glBindRenderbuffer") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGenVertexArrays") == NULL ||
        glXGetProcAddress((const GLubyte *)"glBindVertexArray") == NULL)
        return fail(31, "proc-address surface");

    glDeleteFramebuffers(1, &fb);
    glDeleteRenderbuffers(1, &rb);
    glDeleteVertexArrays(1, &vao);
    glDeleteFramebuffers(1, &list_fb);
    glDeleteRenderbuffers(1, &list_rb);
    glDeleteVertexArrays(1, &list_vao);
    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL object binding semantics unit test: PASS");
    return 0;
}
