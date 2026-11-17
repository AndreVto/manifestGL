#ifndef MGL_FRAMEBUFFER_ATTACHMENT_CASES_H
#define MGL_FRAMEBUFFER_ATTACHMENT_CASES_H
#include "mgl/gl.h"
#include <stdio.h>

static void framebuffer_attachment_trace(void) {
    GLuint tex = 0, cube = 0, fb = 0;
    GLint result = -100;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    printf("image %04x\n", glGetError());
    glGenFramebuffers(1, &fb);
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    printf("bind %04x\n", glGetError());
    glGetFramebufferParameteriv(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, &result);
    printf("width_initial %d %04x\n", result, glGetError());
    glGetFramebufferParameteriv(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS,
                                &result);
    printf("fixed_initial %d %04x\n", result, glGetError());
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS, 1);
    printf("fixed_set %04x\n", glGetError());
    glGetFramebufferParameteriv(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_FIXED_SAMPLE_LOCATIONS,
                                &result);
    printf("fixed_get %d %04x\n", result, glGetError());
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, 17);
    printf("width_set %04x\n", glGetError());
    glGetFramebufferParameteriv(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, &result);
    printf("width_get %d %04x\n", result, glGetError());
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_LAYERS, 1);
    printf("layers_set %04x\n", glGetError());
    glGetFramebufferParameteriv(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_LAYERS, &result);
    printf("layers_get %d %04x\n", result, glGetError());
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
    printf("attach %04x\n", glGetError());
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &result);
    printf("object %d %04x\n", (int)(result == (GLint)tex), glGetError());
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_LAYERED, &result);
    printf("layered %d %04x\n", result, glGetError());
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, -1);
    printf("negative_level %04x\n", glGetError());
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &result);
    printf("unchanged %d %04x\n", (int)(result == (GLint)tex), glGetError());
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, -2);
    printf("negative_width %04x\n", glGetError());
    glFramebufferParameteri(GL_FRAMEBUFFER, 0xDEAD, 2);
    printf("bad_pname %04x\n", glGetError());
    glGenTextures(1, &cube);
    glBindTexture(GL_TEXTURE_CUBE_MAP, cube);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, cube, 0);
    printf("cube_attach %04x\n", glGetError());
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_LAYERED, &result);
    printf("cube_layered %d %04x\n", result, glGetError());
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, 0, 0);
    printf("detach %04x\n", glGetError());
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                          GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &result);
    printf("empty %d %04x\n", result, glGetError());
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glFramebufferParameteri(GL_FRAMEBUFFER, GL_FRAMEBUFFER_DEFAULT_WIDTH, 17);
    printf("default_fbo %04x\n", glGetError());
    glDeleteFramebuffers(1, &fb);
    glDeleteTextures(1, &tex);
    glDeleteTextures(1, &cube);
}

static int legacy_alias_trace(void) {
    typedef void (*gen_buffers_fn)(GLsizei, GLuint *);
    typedef void (*bind_buffer_fn)(GLenum, GLuint);
    typedef void (*buffer_data_fn)(GLenum, GLsizeiptr, const void *, GLenum);
    typedef void (*buffer_subdata_fn)(GLenum, GLintptr, GLsizeiptr, const void *);
    typedef void (*get_buffer_fn)(GLenum, GLenum, GLint *);
    typedef void *(*map_buffer_fn)(GLenum, GLenum);
    typedef GLboolean (*unmap_buffer_fn)(GLenum);
    typedef void (*delete_buffers_fn)(GLsizei, const GLuint *);
    typedef void (*gen_fb_fn)(GLsizei, GLuint *);
    typedef void (*bind_fb_fn)(GLenum, GLuint);
    typedef void (*attach_fn)(GLenum, GLenum, GLenum, GLuint, GLint);
    typedef void (*get_attachment_fn)(GLenum, GLenum, GLenum, GLint *);
    typedef void (*delete_fb_fn)(GLsizei, const GLuint *);
    typedef GLboolean (*is_fb_fn)(GLuint);
    typedef void (*fb_texture_fn)(GLenum, GLenum, GLuint, GLint);
    typedef void (*gen_rb_fn)(GLsizei, GLuint *);
    typedef void (*bind_rb_fn)(GLenum, GLuint);
    typedef void (*storage_rb_fn)(GLenum, GLenum, GLsizei, GLsizei);
    typedef void (*get_rb_fn)(GLenum, GLenum, GLint *);
    typedef void (*delete_rb_fn)(GLsizei, const GLuint *);
    gen_buffers_fn gen_buffers =
        (gen_buffers_fn)glXGetProcAddress((const GLubyte *)"glGenBuffersARB");
    bind_buffer_fn bind_buffer =
        (bind_buffer_fn)glXGetProcAddress((const GLubyte *)"glBindBufferARB");
    buffer_data_fn buffer_data =
        (buffer_data_fn)glXGetProcAddress((const GLubyte *)"glBufferDataARB");
    buffer_subdata_fn buffer_subdata =
        (buffer_subdata_fn)glXGetProcAddress((const GLubyte *)"glBufferSubDataARB");
    get_buffer_fn get_buffer =
        (get_buffer_fn)glXGetProcAddress((const GLubyte *)"glGetBufferParameterivARB");
    map_buffer_fn map_buffer = (map_buffer_fn)glXGetProcAddress((const GLubyte *)"glMapBufferARB");
    unmap_buffer_fn unmap_buffer =
        (unmap_buffer_fn)glXGetProcAddress((const GLubyte *)"glUnmapBufferARB");
    delete_buffers_fn delete_buffers =
        (delete_buffers_fn)glXGetProcAddress((const GLubyte *)"glDeleteBuffersARB");
    gen_fb_fn gen_fb = (gen_fb_fn)glXGetProcAddress((const GLubyte *)"glGenFramebuffersEXT");
    bind_fb_fn bind_fb = (bind_fb_fn)glXGetProcAddress((const GLubyte *)"glBindFramebufferEXT");
    attach_fn attach = (attach_fn)glXGetProcAddress((const GLubyte *)"glFramebufferTexture2DEXT");
    get_attachment_fn get_attachment = (get_attachment_fn)glXGetProcAddress(
        (const GLubyte *)"glGetFramebufferAttachmentParameterivEXT");
    delete_fb_fn delete_fb =
        (delete_fb_fn)glXGetProcAddress((const GLubyte *)"glDeleteFramebuffersEXT");
    is_fb_fn is_fb = (is_fb_fn)glXGetProcAddress((const GLubyte *)"glIsFramebufferEXT");
    fb_texture_fn attach_arb =
        (fb_texture_fn)glXGetProcAddress((const GLubyte *)"glFramebufferTextureARB");
    gen_rb_fn gen_rb = (gen_rb_fn)glXGetProcAddress((const GLubyte *)"glGenRenderbuffersEXT");
    bind_rb_fn bind_rb = (bind_rb_fn)glXGetProcAddress((const GLubyte *)"glBindRenderbufferEXT");
    storage_rb_fn storage_rb =
        (storage_rb_fn)glXGetProcAddress((const GLubyte *)"glRenderbufferStorageEXT");
    get_rb_fn get_rb =
        (get_rb_fn)glXGetProcAddress((const GLubyte *)"glGetRenderbufferParameterivEXT");
    delete_rb_fn delete_rb =
        (delete_rb_fn)glXGetProcAddress((const GLubyte *)"glDeleteRenderbuffersEXT");
    GLuint buffer = 0, fb = 0, tex = 0, rb = 0;
    GLint v = 0;
    unsigned char data[4] = {1, 2, 3, 4};
    unsigned char *mapped;
    if (!gen_buffers || !bind_buffer || !buffer_data || !buffer_subdata || !get_buffer ||
        !map_buffer || !unmap_buffer || !delete_buffers || !gen_fb || !bind_fb || !attach ||
        !get_attachment || !delete_fb || !is_fb || !attach_arb || !gen_rb || !bind_rb ||
        !storage_rb || !get_rb || !delete_rb)
        return 2;
    {
        const char *names[] = {"glGenBuffersARB",
                               "glDeleteBuffersARB",
                               "glBindBufferARB",
                               "glIsBufferARB",
                               "glBufferDataARB",
                               "glBufferSubDataARB",
                               "glGetBufferSubDataARB",
                               "glMapBufferARB",
                               "glUnmapBufferARB",
                               "glGetBufferParameterivARB",
                               "glGetBufferPointervARB",
                               "glGenFramebuffersEXT",
                               "glDeleteFramebuffersEXT",
                               "glBindFramebufferEXT",
                               "glIsFramebufferEXT",
                               "glCheckFramebufferStatusEXT",
                               "glFramebufferRenderbufferEXT",
                               "glBlitFramebufferEXT",
                               "glGetFramebufferAttachmentParameterivEXT",
                               "glGenRenderbuffersEXT",
                               "glDeleteRenderbuffersEXT",
                               "glBindRenderbufferEXT",
                               "glIsRenderbufferEXT",
                               "glRenderbufferStorageEXT",
                               "glRenderbufferStorageMultisampleEXT",
                               "glGetRenderbufferParameterivEXT",
                               "glFramebufferTexture1DEXT",
                               "glFramebufferTexture2DEXT",
                               "glFramebufferTexture3DEXT",
                               "glFramebufferTextureARB",
                               "glFramebufferTextureLayerARB",
                               "glFramebufferTextureFaceARB",
                               "glGenerateMipmapEXT",
                               "glTexImage3DEXT",
                               "glTexSubImage3DEXT",
                               "glActiveTextureARB",
                               "glClientActiveTextureARB",
                               "glBlendColorEXT",
                               "glBlendEquationEXT",
                               "glBlendEquationSeparateEXT",
                               "glBlendFuncSeparateEXT",
                               "glSampleCoverageARB"};
        unsigned found = 0;
        for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
            found += glXGetProcAddress((const GLubyte *)names[i]) != NULL;
        printf("legacy_api_symbols %u/%u\n", found, (unsigned)(sizeof(names) / sizeof(names[0])));
        if (found != sizeof(names) / sizeof(names[0]))
            return 3;
    }
    gen_buffers(1, &buffer);
    bind_buffer(GL_ARRAY_BUFFER, buffer);
    buffer_data(GL_ARRAY_BUFFER, 4, data, GL_STATIC_DRAW);
    get_buffer(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &v);
    printf("arb_buffer_size %d %04x\n", v, glGetError());
    data[1] = 9;
    buffer_subdata(GL_ARRAY_BUFFER, 1, 1, &data[1]);
    mapped = (unsigned char *)map_buffer(GL_ARRAY_BUFFER, GL_READ_ONLY);
    printf("arb_buffer_map %d %d %04x\n", (int)(mapped != NULL), mapped ? mapped[1] : -1,
           glGetError());
    if (mapped)
        printf("arb_buffer_unmap %d %04x\n", (int)unmap_buffer(GL_ARRAY_BUFFER), glGetError());
    buffer_data(GL_ARRAY_BUFFER, -1, NULL, GL_STATIC_DRAW);
    printf("arb_bad_size %04x\n", glGetError());
    get_buffer(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &v);
    printf("arb_buffer_preserved %d %04x\n", v, glGetError());
    bind_buffer(GL_ARRAY_BUFFER, 0);
    delete_buffers(1, &buffer);
    printf("arb_buffer_delete %04x\n", glGetError());

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    printf("ext_image %04x\n", glGetError());
    gen_fb(1, &fb);
    bind_fb(GL_FRAMEBUFFER, fb);
    printf("ext_is_fbo %d %04x\n", (int)is_fb(fb), glGetError());
    attach(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    printf("ext_attach %04x\n", glGetError());
    get_attachment(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &v);
    printf("ext_object_type %04x %04x\n", v, glGetError());
    get_attachment(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &v);
    printf("ext_object_name %d %04x\n", (int)(v == (GLint)tex), glGetError());
    attach(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, -1);
    printf("ext_bad_level %04x\n", glGetError());
    get_attachment(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &v);
    printf("ext_preserved %d %04x\n", (int)(v == (GLint)tex), glGetError());
    attach_arb(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
    printf("arb_attach %04x\n", glGetError());
    get_attachment(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &v);
    printf("arb_attach_name %d %04x\n", (int)(v == (GLint)tex), glGetError());
    gen_rb(1, &rb);
    bind_rb(GL_RENDERBUFFER, rb);
    storage_rb(GL_RENDERBUFFER, GL_RGBA8, 4, 4);
    printf("ext_renderbuffer_storage %04x\n", glGetError());
    get_rb(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &v);
    printf("ext_renderbuffer_width %d %04x\n", v, glGetError());
    bind_rb(GL_RENDERBUFFER, 0);
    delete_rb(1, &rb);
    printf("ext_renderbuffer_cleanup %04x\n", glGetError());
    bind_fb(GL_FRAMEBUFFER, 0);
    delete_fb(1, &fb);
    glDeleteTextures(1, &tex);
    printf("ext_cleanup %04x\n", glGetError());
    return 0;
}
#endif
