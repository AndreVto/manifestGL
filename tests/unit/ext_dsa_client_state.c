#include "mgl/gl.h"
#include "mgl/glx.h"
#include "core/context.h"
#include "core/error.h"
#include "objects/object_store.h"
#include "shader/translator.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int fail(int code, const char *message) {
    fprintf(stderr, "EXT DSA client-state failure %d: %s\n", code, message);
    return code;
}

int main(void) {
    struct mgl_context ctx;
    GLfloat vertices[12] = {0};
    GLfloat generic[16] = {0};
    GLfloat tex2[8] = {0};
    GLfloat tex5[8] = {0};
    GLuint named_vao = 0u;
    GLuint list;
    void *ptr = NULL;

    memset(&ctx, 0, sizeof(ctx));
    ctx.share_group = mgl_share_group_create();
    if (ctx.share_group == NULL)
        return fail(1, "share group");
    mgl_error_init(&ctx.errors);
    mgl_compat_state_init(&ctx.compat);
    mgl_vertex_array_init(&ctx.default_vao);
    ctx.current_vao = &ctx.default_vao;
    ctx.pack_alignment = 4;
    ctx.unpack_alignment = 4;
    mgl_context_set_current_for_test(&ctx);

    ctx.array_buffer = 17u;
    ctx.current_vao->element_array_buffer = 18u;
    ctx.pixel_pack_buffer = 19u;
    ctx.pixel_unpack_buffer = 20u;
    ctx.client_active_texture = 3u;
    ctx.pack_alignment = 8;
    ctx.unpack_alignment = 2;
    glVertexPointer(3, GL_FLOAT, 12, vertices);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexAttribPointer(2u, 3, GL_FLOAT, GL_TRUE, 16, generic);
    glEnableVertexAttribArray(2u);

    glPushClientAttribDefaultEXT(GL_CLIENT_VERTEX_ARRAY_BIT | GL_CLIENT_PIXEL_STORE_BIT);
    if (ctx.client_attrib_stack_depth != 1u)
        return fail(2, "push depth");
    if (ctx.array_buffer != 0u || ctx.current_vao->element_array_buffer != 0u ||
        ctx.client_active_texture != 0u)
        return fail(3, "vertex selectors not reset");
    if (ctx.pixel_pack_buffer != 0u || ctx.pixel_unpack_buffer != 0u || ctx.pack_alignment != 4 ||
        ctx.unpack_alignment != 4)
        return fail(4, "pixel store not reset");
    if (ctx.current_vao->compat[MGL_COMPAT_SEM_POSITION].enabled ||
        ctx.current_vao->compat[MGL_COMPAT_SEM_POSITION].size != 4 ||
        ctx.current_vao->compat[MGL_COMPAT_SEM_POSITION].type != GL_FLOAT ||
        ctx.current_vao->compat[MGL_COMPAT_SEM_POSITION].data.client_ptr != NULL ||
        ctx.current_vao->compat[MGL_COMPAT_SEM_POSITION].data.buffer_name != 0u)
        return fail(5, "compat array defaults");
    if (ctx.current_vao->generic[2].enabled || ctx.current_vao->generic[2].size != 4 ||
        ctx.current_vao->generic[2].type != GL_FLOAT || ctx.current_vao->generic[2].normalized ||
        ctx.current_vao->generic[2].stride != 0 ||
        ctx.current_vao->generic[2].data.buffer_name != 0u)
        return fail(6, "generic array defaults");

    glPopClientAttrib();
    if (ctx.client_attrib_stack_depth != 0u)
        return fail(7, "pop depth");
    if (ctx.array_buffer != 17u || ctx.current_vao->element_array_buffer != 18u ||
        ctx.client_active_texture != 3u)
        return fail(8, "vertex state restore");
    if (ctx.pixel_pack_buffer != 19u || ctx.pixel_unpack_buffer != 20u || ctx.pack_alignment != 8 ||
        ctx.unpack_alignment != 2)
        return fail(9, "pixel state restore");
    if (!ctx.current_vao->compat[MGL_COMPAT_SEM_POSITION].enabled ||
        ctx.current_vao->compat[MGL_COMPAT_SEM_POSITION].size != 3 ||
        ctx.current_vao->compat[MGL_COMPAT_SEM_POSITION].data.buffer_name != 17u)
        return fail(10, "compat array restore");
    if (!ctx.current_vao->generic[2].enabled || ctx.current_vao->generic[2].size != 3 ||
        ctx.current_vao->generic[2].data.buffer_name != 17u)
        return fail(11, "generic array restore");

    /* EXT_direct_state_access indexed client texture state must not disturb
     * CLIENT_ACTIVE_TEXTURE and must validate array/index independently. */
    ctx.array_buffer = 0u;
    glClientActiveTexture(GL_TEXTURE2);
    glTexCoordPointer(2, GL_FLOAT, 0, tex2);
    glClientActiveTexture(GL_TEXTURE5);
    glTexCoordPointer(2, GL_FLOAT, 0, tex5);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientStateIndexed(GL_TEXTURE_COORD_ARRAY, 2u);
    if (ctx.client_active_texture != 5u ||
        !ctx.current_vao->compat[MGL_COMPAT_SEM_TEXCOORD0 + 2u].enabled ||
        ctx.current_vao->compat[MGL_COMPAT_SEM_TEXCOORD0 + 5u].enabled)
        return fail(12, "indexed client enable selector isolation");
    glEnableClientStateiEXT(GL_TEXTURE_COORD_ARRAY, 5u);
    glDisableClientStateIndexedEXT(GL_TEXTURE_COORD_ARRAY, 2u);
    if (ctx.client_active_texture != 5u ||
        ctx.current_vao->compat[MGL_COMPAT_SEM_TEXCOORD0 + 2u].enabled ||
        !ctx.current_vao->compat[MGL_COMPAT_SEM_TEXCOORD0 + 5u].enabled)
        return fail(13, "indexed client alias semantics");
    glEnableClientStateIndexed(GL_VERTEX_ARRAY, 0u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return fail(14, "indexed client invalid array");
    glEnableClientStateIndexed(GL_TEXTURE_COORD_ARRAY, MGL_TEXTURE_UNITS);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(15, "indexed client invalid index");

    ptr = (void *)0x1;
    glGetPointerIndexedv(GL_TEXTURE_COORD_ARRAY_POINTER, 2u, &ptr);
    if (ptr != tex2 || ctx.client_active_texture != 5u)
        return fail(16, "indexed pointer client query");
    ctx.array_buffer = 29u;
    glClientActiveTexture(GL_TEXTURE3);
    glTexCoordPointer(4, GL_FLOAT, 0, (const void *)(uintptr_t)48u);
    ctx.array_buffer = 0u;
    ptr = NULL;
    glGetPointerIndexedvEXT(GL_TEXTURE_COORD_ARRAY_POINTER, 3u, &ptr);
    if (ptr != (void *)(uintptr_t)48u || ctx.client_active_texture != 3u)
        return fail(17, "indexed pointer VBO offset query");
    ptr = (void *)0x1234;
    glGetPointerIndexedv(GL_VERTEX_ARRAY_POINTER, 3u, &ptr);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM || ptr != (void *)0x1234)
        return fail(18, "indexed pointer invalid pname preservation");
    glGetPointerIndexedv(GL_TEXTURE_COORD_ARRAY_POINTER, MGL_TEXTURE_UNITS, &ptr);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE || ptr != (void *)0x1234)
        return fail(19, "indexed pointer invalid index preservation");

    glClientActiveTexture(GL_TEXTURE2);
    ptr = NULL;
    glGetPointerv(GL_TEXTURE_COORD_ARRAY_POINTER, &ptr);
    if (ptr != tex2)
        return fail(20, "GetPointerv texture selector query");
    glVertexPointer(3, GL_FLOAT, 0, vertices);
    ptr = NULL;
    glGetPointerv(GL_VERTEX_ARRAY_POINTER, &ptr);
    if (ptr != vertices)
        return fail(21, "GetPointerv vertex pointer query");

    /* Named VAO client enables initialize a generated name without binding it. */
    glGenVertexArrays(1, &named_vao);
    if (named_vao == 0u || glIsVertexArray(named_vao) != GL_FALSE)
        return fail(22, "generated VAO initial state");
    if (ctx.current_vao != &ctx.default_vao)
        return fail(23, "unexpected current VAO");
    glClientActiveTexture(GL_TEXTURE3);
    glEnableVertexArray(named_vao, GL_VERTEX_ARRAY);
    glEnableVertexArrayEXT(named_vao, GL_TEXTURE4);
    glEnableVertexArray(named_vao, GL_TEXTURE_COORD_ARRAY);
    {
        struct mgl_vertex_array *named = mgl_vao_find(&ctx, named_vao);
        if (named == NULL || glIsVertexArray(named_vao) != GL_TRUE ||
            !named->compat[MGL_COMPAT_SEM_POSITION].enabled ||
            !named->compat[MGL_COMPAT_SEM_TEXCOORD0 + 4u].enabled ||
            !named->compat[MGL_COMPAT_SEM_TEXCOORD0 + 3u].enabled ||
            ctx.current_vao != &ctx.default_vao || ctx.client_active_texture != 3u)
            return fail(24, "named VAO client enables");
        glDisableVertexArrayEXT(named_vao, GL_TEXTURE4);
        if (named->compat[MGL_COMPAT_SEM_TEXCOORD0 + 4u].enabled)
            return fail(25, "named VAO client disable");
    }
    glEnableVertexArray(0u, GL_VERTEX_ARRAY);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_OPERATION)
        return fail(26, "named VAO zero error");
    glEnableVertexArray(named_vao, 0xdeadbeefu);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_ENUM)
        return fail(27, "named VAO invalid array");

    /* DSA generic-attrib enables share the generated-name initialization rule. */
    glDeleteVertexArrays(1, &named_vao);
    glGenVertexArrays(1, &named_vao);
    if (named_vao == 0u || glIsVertexArray(named_vao) != GL_FALSE)
        return fail(28, "second generated VAO initial state");
    glEnableVertexArrayAttrib(named_vao, 1u);
    if (glIsVertexArray(named_vao) != GL_TRUE || !mgl_vao_find(&ctx, named_vao)->generic[1].enabled)
        return fail(29, "named VAO attrib generated-name initialization");

    list = glGenLists(1);
    if (list == 0u)
        return fail(30, "glGenLists");
    ctx.pack_alignment = 8;
    glNewList(list, GL_COMPILE);
    glClientAttribDefaultEXT(GL_CLIENT_PIXEL_STORE_BIT);
    glEndList();
    if (ctx.pack_alignment != 4 || ctx.pixel_pack_buffer != 0u || ctx.pixel_unpack_buffer != 0u)
        return fail(31, "ClientAttribDefaultEXT did not execute during list compilation");
    ctx.pack_alignment = 8;
    glCallList(list);
    if (ctx.pack_alignment != 8)
        return fail(32, "ClientAttribDefaultEXT was incorrectly retained in display list");

    glClientAttribDefaultEXT(0x80000000u);
    if (mgl_error_take(&ctx.errors) != GL_INVALID_VALUE)
        return fail(33, "invalid mask error");

    if (glXGetProcAddress((const GLubyte *)"glClientAttribDefaultEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glPushClientAttribDefaultEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glEnableClientStateIndexed") == NULL ||
        glXGetProcAddress((const GLubyte *)"glDisableClientStateIndexedEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glEnableClientStatei") == NULL ||
        glXGetProcAddress((const GLubyte *)"glDisableClientStateiEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glEnableVertexArray") == NULL ||
        glXGetProcAddress((const GLubyte *)"glDisableVertexArrayEXT") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetPointerIndexedv") == NULL ||
        glXGetProcAddress((const GLubyte *)"glGetPointerIndexedvEXT") == NULL)
        return fail(34, "proc-address exposure");

    glDeleteVertexArrays(1, &named_vao);
    glDeleteLists(list, 1);
    if (mgl_error_take(&ctx.errors) != GL_NO_ERROR)
        return fail(35, "unexpected GL error");
    mgl_context_set_current_for_test(NULL);
    mgl_share_group_release(ctx.share_group);
    puts("mGL EXT DSA client-state unit test: PASS");
    return 0;
}
