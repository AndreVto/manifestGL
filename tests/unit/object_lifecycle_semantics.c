#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "objects/object_store.h"
#include "texture/texture_store.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static GLuint next_native_name = 900u;
static unsigned gen_fb_calls, gen_vao_calls;
static unsigned delete_fb_calls, delete_vao_calls;
static unsigned bind_fb_calls, bind_vao_calls;
static GLuint last_deleted_fb, last_deleted_vao;
static GLuint last_bound_fb, last_bound_vao;
static GLenum last_fb_target;

static void fake_gen_fb(GLsizei n, GLuint *out) {
    GLsizei i;
    ++gen_fb_calls;
    for (i = 0; i < n; ++i)
        out[i] = next_native_name++;
}

static void fake_gen_vao(GLsizei n, GLuint *out) {
    GLsizei i;
    ++gen_vao_calls;
    for (i = 0; i < n; ++i)
        out[i] = next_native_name++;
}

static void fake_delete_fb(GLsizei n, const GLuint *names) {
    ++delete_fb_calls;
    if (n > 0 && names != NULL)
        last_deleted_fb = names[0];
}

static void fake_delete_vao(GLsizei n, const GLuint *names) {
    ++delete_vao_calls;
    if (n > 0 && names != NULL)
        last_deleted_vao = names[0];
}

static void fake_bind_fb(GLenum target, GLuint name) {
    ++bind_fb_calls;
    last_fb_target = target;
    last_bound_fb = name;
}

static void fake_bind_vao(GLuint name) {
    ++bind_vao_calls;
    last_bound_vao = name;
}

static void fake_bind_buffer(GLenum target, GLuint name) {
    (void)target;
    (void)name;
}

static int fail(int n, const char *why) {
    fprintf(stderr, "object lifecycle semantics failure %d: %s\n", n, why);
    return n;
}

static int expect_error(GLenum expected, int n, const char *why) {
    GLenum got = glGetError();
    if (got != expected) {
        fprintf(stderr, "object lifecycle semantics failure %d: %s (got 0x%x expected 0x%x)\n", n,
                why, (unsigned)got, (unsigned)expected);
        return n;
    }
    return 0;
}

int main(void) {
    struct mgl_context ctx;
    struct mgl_native_dispatch native;
    struct mgl_framebuffer *fb_obj;
    struct mgl_vertex_array *vao_obj;
    struct mgl_texture *reserved;
    GLuint fb = 0u, vao = 0u, textures[2] = {0u, 0u};
    GLuint list = 0u, list_fb = 0u, list_vao = 0u, list_texture = 0u;
    GLuint fb_native = 0u, vao_native = 0u;
    GLuint replay_fb_native = 0u, replay_vao_native = 0u;
    unsigned before_delete_fb, before_delete_vao, before_bind_fb, before_bind_vao;
    unsigned next_texture_after_compile;
    int rc;

    memset(&ctx, 0, sizeof(ctx));
    memset(&native, 0, sizeof(native));
    native.glesGenFramebuffers = fake_gen_fb;
    native.glesDeleteFramebuffers = fake_delete_fb;
    native.glesBindFramebuffer = fake_bind_fb;
    native.glesGenVertexArrays = fake_gen_vao;
    native.glesDeleteVertexArrays = fake_delete_vao;
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

    /* Texture name generation reserves unused names without realizing texture objects. */
    reserved = mgl_texture_ensure(&ctx, 40u, 0u, 0);
    if (reserved == NULL)
        return fail(2, "reserve texture name");
    glGenTextures(2, textures);
    if ((rc = expect_error(GL_NO_ERROR, 3, "GenTextures direct")) != 0)
        return rc;
    if (textures[0] == 0u || textures[1] == 0u || textures[0] == textures[1] ||
        textures[0] <= 40u || textures[1] <= 40u)
        return fail(4, "generated texture names not fresh");
    if (mgl_texture_find(&ctx, textures[0]) == NULL || mgl_texture_find(&ctx, textures[1]) == NULL)
        return fail(5, "generated texture names not reserved");
    if (glIsTexture(textures[0]) || glIsTexture(textures[1]) || glGetError() != GL_NO_ERROR)
        return fail(6, "generated textures realized too early");

    glGenFramebuffers(1, &fb);
    glGenVertexArrays(1, &vao);
    if ((rc = expect_error(GL_NO_ERROR, 7, "object generation")) != 0 || fb == 0u || vao == 0u)
        return fail(7, "generated object names");
    fb_obj = mgl_framebuffer_find(&ctx, fb);
    vao_obj = mgl_vao_find(&ctx, vao);
    if (fb_obj == NULL || vao_obj == NULL)
        return fail(8, "generated object lookup");
    fb_native = fb_obj->native_name;
    vao_native = vao_obj->native_name;

    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glBindVertexArray(vao);
    if ((rc = expect_error(GL_NO_ERROR, 9, "bind objects")) != 0)
        return rc;
    if (ctx.draw_framebuffer != fb || ctx.read_framebuffer != fb || ctx.current_vao != vao_obj)
        return fail(10, "canonical binding state");

    /* Begin/End rejection must win before ordinary value validation and preserve state. */
    before_delete_fb = delete_fb_calls;
    before_delete_vao = delete_vao_calls;
    before_bind_fb = bind_fb_calls;
    before_bind_vao = bind_vao_calls;
    glBegin(GL_POINTS);
    glDeleteFramebuffers(-1, &fb);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 11, "DeleteFramebuffers Begin/End")) != 0 ||
        delete_fb_calls != before_delete_fb || bind_fb_calls != before_bind_fb ||
        mgl_framebuffer_find(&ctx, fb) == NULL || ctx.draw_framebuffer != fb ||
        ctx.read_framebuffer != fb)
        return fail(11, "DeleteFramebuffers mutated in Begin/End");
    glBegin(GL_POINTS);
    glDeleteVertexArrays(-1, &vao);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 12, "DeleteVertexArrays Begin/End")) != 0 ||
        delete_vao_calls != before_delete_vao || bind_vao_calls != before_bind_vao ||
        mgl_vao_find(&ctx, vao) == NULL || ctx.current_vao != vao_obj)
        return fail(12, "DeleteVertexArrays mutated in Begin/End");
    list_texture = 0u;
    glBegin(GL_POINTS);
    glGenTextures(-1, &list_texture);
    glEnd();
    if ((rc = expect_error(GL_INVALID_OPERATION, 13, "GenTextures Begin/End")) != 0 ||
        list_texture != 0u)
        return fail(13, "GenTextures produced a name in Begin/End");

    last_deleted_fb = 0u;
    last_deleted_vao = 0u;
    before_delete_fb = delete_fb_calls;
    before_delete_vao = delete_vao_calls;
    glDeleteFramebuffers(1, &fb);
    glDeleteVertexArrays(1, &vao);
    if ((rc = expect_error(GL_NO_ERROR, 14, "direct delete")) != 0)
        return rc;
    if (delete_fb_calls != before_delete_fb + 1u || delete_vao_calls != before_delete_vao + 1u ||
        last_deleted_fb != fb_native || last_deleted_vao != vao_native)
        return fail(15, "native delete translation");
    if (mgl_framebuffer_find(&ctx, fb) != NULL || mgl_vao_find(&ctx, vao) != NULL ||
        ctx.draw_framebuffer != 0u || ctx.read_framebuffer != 0u ||
        ctx.current_vao != &ctx.default_vao)
        return fail(16, "delete canonical lifetime/binding state");
    if (last_fb_target != GL_FRAMEBUFFER || last_bound_fb != 0u || last_bound_vao != 0u)
        return fail(17, "delete default native rebind");

    before_delete_fb = delete_fb_calls;
    before_delete_vao = delete_vao_calls;
    glDeleteFramebuffers(1, &fb);
    glDeleteFramebuffers(1, (const GLuint[]){0u});
    glDeleteFramebuffers(1, (const GLuint[]){99991u});
    glDeleteVertexArrays(1, &vao);
    glDeleteVertexArrays(1, (const GLuint[]){0u});
    glDeleteVertexArrays(1, (const GLuint[]){99992u});
    if ((rc = expect_error(GL_NO_ERROR, 18, "ignored deletes")) != 0 ||
        delete_fb_calls != before_delete_fb || delete_vao_calls != before_delete_vao)
        return fail(18, "ignored names reached native");

    /* These object-lifecycle commands execute while compiling a list and are not retained. */
    glGenFramebuffers(1, &list_fb);
    glGenVertexArrays(1, &list_vao);
    if (list_fb == 0u || list_vao == 0u || glGetError() != GL_NO_ERROR)
        return fail(19, "display-list object setup");
    list = glGenLists(1);
    if (list == 0u)
        return fail(20, "display-list allocation");
    list_texture = 0u;
    before_delete_fb = delete_fb_calls;
    before_delete_vao = delete_vao_calls;
    glNewList(list, GL_COMPILE);
    glGenTextures(1, &list_texture);
    glDeleteFramebuffers(1, &list_fb);
    glDeleteVertexArrays(1, &list_vao);
    glEndList();
    if ((rc = expect_error(GL_NO_ERROR, 21, "display-list compile")) != 0)
        return rc;
    if (list_texture == 0u || mgl_texture_find(&ctx, list_texture) == NULL ||
        glIsTexture(list_texture) || delete_fb_calls != before_delete_fb + 1u ||
        delete_vao_calls != before_delete_vao + 1u || mgl_framebuffer_find(&ctx, list_fb) != NULL ||
        mgl_vao_find(&ctx, list_vao) != NULL)
        return fail(22, "display-list commands did not execute immediately");
    if (glGetError() != GL_NO_ERROR)
        return fail(23, "display-list compile query error");

    next_texture_after_compile =
        atomic_load_explicit(&ctx.share_group->next_texture_name, memory_order_relaxed);
    fb_obj = mgl_framebuffer_ensure(&ctx, list_fb, 1);
    vao_obj = mgl_vao_create_reserved(&ctx, list_vao);
    if (fb_obj == NULL || vao_obj == NULL || fb_obj->native_name == 0u ||
        vao_obj->native_name == 0u)
        return fail(24, "recreate deleted names");
    replay_fb_native = fb_obj->native_name;
    replay_vao_native = vao_obj->native_name;
    before_delete_fb = delete_fb_calls;
    before_delete_vao = delete_vao_calls;
    glCallList(list);
    if ((rc = expect_error(GL_NO_ERROR, 25, "display-list replay")) != 0)
        return rc;
    if (atomic_load_explicit(&ctx.share_group->next_texture_name, memory_order_relaxed) !=
            next_texture_after_compile ||
        delete_fb_calls != before_delete_fb || delete_vao_calls != before_delete_vao ||
        mgl_framebuffer_find(&ctx, list_fb) == NULL || mgl_vao_find(&ctx, list_vao) == NULL ||
        mgl_framebuffer_find(&ctx, list_fb)->native_name != replay_fb_native ||
        mgl_vao_find(&ctx, list_vao)->native_name != replay_vao_native)
        return fail(26, "object-lifecycle command retained in display list");

    if (glXGetProcAddress((const GLubyte *)"glGenTextures") == NULL ||
        glXGetProcAddress((const GLubyte *)"glDeleteFramebuffers") == NULL ||
        glXGetProcAddress((const GLubyte *)"glDeleteVertexArrays") == NULL)
        return fail(27, "proc-address surface");

    glDeleteFramebuffers(1, &list_fb);
    glDeleteVertexArrays(1, &list_vao);
    glDeleteLists(list, 1);
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL object lifecycle semantics unit test: PASS");
    return 0;
}
