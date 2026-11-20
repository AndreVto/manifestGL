#include "core/render_diag.h"
#include "list/display_list.h"
#include "mgl/gl.h"

#include "core/context.h"
#include "compat/raster.h"
#include "texture/texture_store.h"
#include "state/state.h"
#include "vertex/draw_snapshot.h"

#include <stdlib.h>
#include <string.h>

enum mgl_list_command_type {
    MGL_LIST_CMD_ACTIVE_TEXTURE,
    MGL_LIST_CMD_MATRIX_MODE,
    MGL_LIST_CMD_LOAD_IDENTITY,
    MGL_LIST_CMD_LOAD_MATRIXF,
    MGL_LIST_CMD_MULT_MATRIXF,
    MGL_LIST_CMD_PUSH_MATRIX,
    MGL_LIST_CMD_POP_MATRIX,
    MGL_LIST_CMD_MATRIX_LOAD_EXT,
    MGL_LIST_CMD_MATRIX_MULT_EXT,
    MGL_LIST_CMD_MATRIX_LOAD_IDENTITY_EXT,
    MGL_LIST_CMD_MATRIX_PUSH_EXT,
    MGL_LIST_CMD_MATRIX_POP_EXT,
    MGL_LIST_CMD_COLOR4F,
    MGL_LIST_CMD_SECONDARY3F,
    MGL_LIST_CMD_NORMAL3F,
    MGL_LIST_CMD_TEXCOORD4F,
    MGL_LIST_CMD_FOG_COORD,
    MGL_LIST_CMD_ENABLE,
    MGL_LIST_CMD_DISABLE,
    MGL_LIST_CMD_ENABLE_INDEXED,
    MGL_LIST_CMD_ENABLE_INDEXED_EXT,
    MGL_LIST_CMD_COLOR_MASK_INDEXED,
    MGL_LIST_CMD_BLEND_FUNC_INDEXED,
    MGL_LIST_CMD_BLEND_EQUATION_INDEXED,
    MGL_LIST_CMD_CLEAR_BUFFER_IV,
    MGL_LIST_CMD_CLEAR_BUFFER_UIV,
    MGL_LIST_CMD_CLEAR_BUFFER_FV,
    MGL_LIST_CMD_CLEAR_BUFFER_FI,
    MGL_LIST_CMD_DRAW_BUFFER,
    MGL_LIST_CMD_DRAW_BUFFERS,
    MGL_LIST_CMD_READ_BUFFER,
    MGL_LIST_CMD_PUSH_ATTRIB,
    MGL_LIST_CMD_POP_ATTRIB,
    MGL_LIST_CMD_PIXEL_TRANSFER,
    MGL_LIST_CMD_PIXEL_MAP,
    MGL_LIST_CMD_ALPHA_FUNC,
    MGL_LIST_CMD_CLEAR_INDEX,
    MGL_LIST_CMD_CLEAR_COLOR,
    MGL_LIST_CMD_CLEAR_DEPTH,
    MGL_LIST_CMD_CLEAR_STENCIL,
    MGL_LIST_CMD_CLEAR,
    MGL_LIST_CMD_VIEWPORT,
    MGL_LIST_CMD_SCISSOR,
    MGL_LIST_CMD_DEPTH_FUNC,
    MGL_LIST_CMD_DEPTH_MASK,
    MGL_LIST_CMD_INDEX_MASK,
    MGL_LIST_CMD_COLOR_MASK,
    MGL_LIST_CMD_CULL_FACE,
    MGL_LIST_CMD_FRONT_FACE,
    MGL_LIST_CMD_FOG,
    MGL_LIST_CMD_LIGHT,
    MGL_LIST_CMD_LIGHT_MODEL,
    MGL_LIST_CMD_MATERIAL,
    MGL_LIST_CMD_COLOR_MATERIAL,
    MGL_LIST_CMD_SHADE_MODEL,
    MGL_LIST_CMD_TEX_ENV,
    MGL_LIST_CMD_MULTI_TEX_ENV_EXT,
    MGL_LIST_CMD_TEXGEN,
    MGL_LIST_CMD_MULTI_TEXGEN_EXT,
    MGL_LIST_CMD_PROGRAM_UNIFORM_EXT,
    MGL_LIST_CMD_ARB_PROGRAM_PARAMETERS,
    MGL_LIST_CMD_ARB_PROGRAM_STRING,
    MGL_LIST_CMD_CLIP_PLANE,
    MGL_LIST_CMD_STENCIL_FUNC,
    MGL_LIST_CMD_STENCIL_MASK,
    MGL_LIST_CMD_STENCIL_OP,
    MGL_LIST_CMD_BLEND_FUNC_SEPARATE,
    MGL_LIST_CMD_BLEND_EQUATION_SEPARATE,
    MGL_LIST_CMD_DEPTH_RANGE,
    MGL_LIST_CMD_BLEND_COLOR,
    MGL_LIST_CMD_LINE_WIDTH,
    MGL_LIST_CMD_POLYGON_OFFSET,
    MGL_LIST_CMD_SAMPLE_COVERAGE,
    MGL_LIST_CMD_HINT,
    MGL_LIST_CMD_SET_FENCE_NV,
    MGL_LIST_CMD_MAP1,
    MGL_LIST_CMD_MAP2,
    MGL_LIST_CMD_EVAL_COORD1,
    MGL_LIST_CMD_EVAL_COORD2,
    MGL_LIST_CMD_MAP_GRID1,
    MGL_LIST_CMD_MAP_GRID2,
    MGL_LIST_CMD_EVAL_POINT1,
    MGL_LIST_CMD_EVAL_POINT2,
    MGL_LIST_CMD_EVAL_MESH1,
    MGL_LIST_CMD_EVAL_MESH2,
    MGL_LIST_CMD_RASTER_POS4D,
    MGL_LIST_CMD_WINDOW_POS3F,
    MGL_LIST_CMD_PIXEL_ZOOM,
    MGL_LIST_CMD_DRAW_PIXELS,
    MGL_LIST_CMD_BITMAP,
    MGL_LIST_CMD_COPY_PIXELS,
    MGL_LIST_CMD_BEGIN,
    MGL_LIST_CMD_VERTEX4F,
    MGL_LIST_CMD_END,
    MGL_LIST_CMD_BIND_TEXTURE,
    MGL_LIST_CMD_BIND_MULTI_TEXTURE,
    MGL_LIST_CMD_MULTI_TEX_BUFFER,
    MGL_LIST_CMD_TEXTURE_BUFFER_EXT,
    MGL_LIST_CMD_PRIORITIZE_TEXTURES,
    MGL_LIST_CMD_TEX_PARAMETER,
    MGL_LIST_CMD_MULTI_TEX_PARAMETER,
    MGL_LIST_CMD_TEXTURE_PARAMETER_EXT,
    MGL_LIST_CMD_MULTI_TEX_IMAGE_2D,
    MGL_LIST_CMD_MULTI_TEX_SUB_IMAGE_2D,
    MGL_LIST_CMD_MULTI_TEX_IMAGE_3D,
    MGL_LIST_CMD_MULTI_TEX_SUB_IMAGE_3D,
    MGL_LIST_CMD_COMPRESSED_MULTI_TEX_IMAGE_2D,
    MGL_LIST_CMD_COMPRESSED_MULTI_TEX_SUB_IMAGE_2D,
    MGL_LIST_CMD_COMPRESSED_MULTI_TEX_IMAGE_3D,
    MGL_LIST_CMD_COMPRESSED_MULTI_TEX_SUB_IMAGE_3D,
    MGL_LIST_CMD_TEX_IMAGE_2D,
    MGL_LIST_CMD_TEX_SUB_IMAGE_2D,
    MGL_LIST_CMD_TEX_IMAGE_3D,
    MGL_LIST_CMD_TEX_SUB_IMAGE_3D,
    MGL_LIST_CMD_COMPRESSED_TEX_IMAGE_2D,
    MGL_LIST_CMD_COMPRESSED_TEX_IMAGE_3D,
    MGL_LIST_CMD_COMPRESSED_TEX_SUB_IMAGE_2D,
    MGL_LIST_CMD_COMPRESSED_TEX_SUB_IMAGE_3D,
    MGL_LIST_CMD_COPY_TEX_IMAGE_2D,
    MGL_LIST_CMD_COPY_TEX_SUB_IMAGE_2D,
    MGL_LIST_CMD_COPY_TEX_SUB_IMAGE_3D,
    MGL_LIST_CMD_COMPRESSED_TEXTURE_IMAGE_2D_EXT,
    MGL_LIST_CMD_COMPRESSED_TEXTURE_SUB_IMAGE_2D_EXT,
    MGL_LIST_CMD_COMPRESSED_TEXTURE_IMAGE_3D_EXT,
    MGL_LIST_CMD_COMPRESSED_TEXTURE_SUB_IMAGE_3D_EXT,
    MGL_LIST_CMD_TEXTURE_IMAGE_2D_EXT,
    MGL_LIST_CMD_TEXTURE_SUB_IMAGE_2D_EXT,
    MGL_LIST_CMD_TEXTURE_IMAGE_3D_EXT,
    MGL_LIST_CMD_TEXTURE_SUB_IMAGE_3D_EXT,
    MGL_LIST_CMD_COPY_TEXTURE_IMAGE_2D_EXT,
    MGL_LIST_CMD_COPY_TEXTURE_SUB_IMAGE_2D_EXT,
    MGL_LIST_CMD_COPY_TEXTURE_SUB_IMAGE_3D_EXT,
    MGL_LIST_CMD_COPY_MULTI_TEX_IMAGE_2D,
    MGL_LIST_CMD_COPY_MULTI_TEX_SUB_IMAGE_2D,
    MGL_LIST_CMD_COPY_MULTI_TEX_SUB_IMAGE_3D,
    MGL_LIST_CMD_DRAW_SNAPSHOT,
    MGL_LIST_CMD_INIT_NAMES,
    MGL_LIST_CMD_PUSH_NAME,
    MGL_LIST_CMD_POP_NAME,
    MGL_LIST_CMD_LOAD_NAME,
    MGL_LIST_CMD_CALL_LIST,
    MGL_LIST_CMD_CALL_LISTS,
    MGL_LIST_CMD_LIST_BASE
};

struct mgl_list_command {
    enum mgl_list_command_type type;

    union {
        GLenum enum_value;
        GLfloat matrix[16];

        struct {
            GLenum matrix_mode;
            GLfloat matrix[16];
        } matrix_ext;
        struct {
            GLuint name;
            GLenum condition;
        } fence;

        GLfloat color[4];
        GLfloat vec4[4];

        struct {
            unsigned unit;
            GLfloat value[4];
        } texcoord;

        GLenum cap;

        struct {
            GLenum cap;
            GLuint index;
            GLboolean enabled;
        } indexed_enable;

        struct {
            GLuint index;
            GLboolean mask[4];
        } indexed_color_mask;

        struct {
            GLuint index;
            GLenum src_rgb;
            GLenum dst_rgb;
            GLenum src_alpha;
            GLenum dst_alpha;
        } indexed_blend_func;

        struct {
            GLuint index;
            GLenum rgb;
            GLenum alpha;
        } indexed_blend_equation;

        struct {
            GLenum buffer;
            GLint drawbuffer;
            GLint value[4];
        } clear_buffer_iv;

        struct {
            GLenum buffer;
            GLint drawbuffer;
            GLuint value[4];
        } clear_buffer_uiv;

        struct {
            GLenum buffer;
            GLint drawbuffer;
            GLfloat value[4];
        } clear_buffer_fv;

        struct {
            GLenum buffer;
            GLint drawbuffer;
            GLfloat depth;
            GLint stencil;
        } clear_buffer_fi;

        struct {
            GLsizei count;
            GLenum buffers[MGL_MAX_DRAW_BUFFERS];
        } draw_buffers;

        GLenum begin_mode;

        struct {
            GLenum target;
            GLuint texture;
        } bind_texture;

        struct {
            GLenum texunit;
            GLenum target;
            GLuint texture;
        } bind_multi_texture;

        struct {
            GLenum texunit;
            GLenum target;
            GLenum internalformat;
            GLuint buffer;
        } multi_tex_buffer;

        struct {
            GLuint texture;
            GLenum target;
            GLenum internalformat;
            GLuint buffer;
        } texture_buffer_ext;

        struct {
            GLsizei count;
            GLuint *textures;
            GLfloat *priorities;
        } prioritize_textures;

        struct {
            GLenum target;
            GLenum pname;
            GLint iparam[4];
            GLuint uparam[4];
            GLfloat fparam[4];
            GLboolean floating;
            GLboolean vector4;
            GLboolean integer_typed;
            GLboolean unsigned_integer;
        } tex_parameter;

        struct {
            GLenum texunit;
            GLenum target;
            GLenum pname;
            GLint iparam[4];
            GLuint uparam[4];
            GLfloat fparam[4];
            GLboolean floating;
            GLboolean vector4;
            GLboolean integer_typed;
            GLboolean unsigned_integer;
        } multi_tex_parameter;

        struct {
            GLuint texture;
            GLenum target;
            GLenum pname;
            GLint iparam[4];
            GLuint uparam[4];
            GLfloat fparam[4];
            GLboolean floating;
            GLboolean vector4;
            GLboolean integer_typed;
            GLboolean unsigned_integer;
        } texture_parameter_ext;

        struct {
            GLenum texunit;
            GLenum target;
            GLint level;
            GLint internalformat;
            GLsizei width;
            GLsizei height;
            GLint border;
            GLenum format;
            GLenum data_type;
            void *data;
            size_t data_size;
        } multi_tex_image_2d;

        struct {
            GLenum texunit;
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLsizei width;
            GLsizei height;
            GLenum format;
            GLenum data_type;
            void *data;
            size_t data_size;
        } multi_tex_sub_image_2d;

        struct {
            GLenum texunit;
            GLenum target;
            GLint level;
            GLint internalformat;
            GLsizei width;
            GLsizei height;
            GLsizei depth;
            GLint border;
            GLenum format;
            GLenum data_type;
            void *data;
            size_t data_size;
        } multi_tex_image_3d;

        struct {
            GLenum texunit;
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLint zoffset;
            GLsizei width;
            GLsizei height;
            GLsizei depth;
            GLenum format;
            GLenum data_type;
            void *data;
            size_t data_size;
        } multi_tex_sub_image_3d;

        struct {
            GLenum texunit;
            GLenum target;
            GLint level;
            GLenum internalformat;
            GLsizei width;
            GLsizei height;
            GLint border;
            void *data;
            size_t data_size;
        } compressed_multi_tex_image_2d;

        struct {
            GLenum texunit;
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLsizei width;
            GLsizei height;
            GLenum format;
            void *data;
            size_t data_size;
        } compressed_multi_tex_sub_image_2d;

        struct {
            GLenum texunit;
            GLenum target;
            GLint level;
            GLenum internalformat;
            GLsizei width;
            GLsizei height;
            GLsizei depth;
            GLint border;
            void *data;
            size_t data_size;
        } compressed_multi_tex_image_3d;

        struct {
            GLenum texunit;
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLint zoffset;
            GLsizei width;
            GLsizei height;
            GLsizei depth;
            GLenum format;
            void *data;
            size_t data_size;
        } compressed_multi_tex_sub_image_3d;

        struct {
            GLenum target;
            GLint level;
            GLint internalformat;
            GLsizei width;
            GLsizei height;
            GLint border;
            GLenum format;
            GLenum data_type;
            void *data;
            size_t data_size;
        } tex_image_2d;

        struct {
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLsizei width;
            GLsizei height;
            GLenum format;
            GLenum data_type;
            void *data;
            size_t data_size;
        } tex_sub_image_2d;

        struct {
            GLenum target;
            GLint level;
            GLint internalformat;
            GLsizei width;
            GLsizei height;
            GLsizei depth;
            GLint border;
            GLenum format;
            GLenum data_type;
            void *data;
            size_t data_size;
        } tex_image_3d;

        struct {
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLint zoffset;
            GLsizei width;
            GLsizei height;
            GLsizei depth;
            GLenum format;
            GLenum data_type;
            void *data;
            size_t data_size;
        } tex_sub_image_3d;

        struct {
            GLenum target;
            GLint level;
            GLenum internalformat;
            GLsizei width;
            GLsizei height;
            GLint border;
            void *data;
            size_t data_size;
        } compressed_tex_image_2d;

        struct {
            GLenum target;
            GLint level;
            GLenum internalformat;
            GLsizei width;
            GLsizei height;
            GLsizei depth;
            GLint border;
            void *data;
            size_t data_size;
        } compressed_tex_image_3d;

        struct {
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLsizei width;
            GLsizei height;
            GLenum format;
            void *data;
            size_t data_size;
        } compressed_tex_sub_image_2d;

        struct {
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLint zoffset;
            GLsizei width;
            GLsizei height;
            GLsizei depth;
            GLenum format;
            void *data;
            size_t data_size;
        } compressed_tex_sub_image_3d;

        struct {
            GLenum target;
            GLint level;
            GLenum internalformat;
            GLint x;
            GLint y;
            GLsizei width;
            GLsizei height;
            GLint border;
        } copy_tex_image_2d;

        struct {
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLint x;
            GLint y;
            GLsizei width;
            GLsizei height;
        } copy_tex_sub_image_2d;

        struct {
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLint zoffset;
            GLint x;
            GLint y;
            GLsizei width;
            GLsizei height;
        } copy_tex_sub_image_3d;

        struct {
            GLuint texture;
            GLenum target;
            GLint level;
            GLint internalformat;
            GLsizei width;
            GLsizei height;
            GLint border;
            GLenum format;
            GLenum data_type;
            void *data;
            size_t data_size;
        } texture_image_2d_ext;

        struct {
            GLuint texture;
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLsizei width;
            GLsizei height;
            GLenum format;
            GLenum data_type;
            void *data;
            size_t data_size;
        } texture_sub_image_2d_ext;

        struct {
            GLuint texture;
            GLenum target;
            GLint level;
            GLint internalformat;
            GLsizei width;
            GLsizei height;
            GLsizei depth;
            GLint border;
            GLenum format;
            GLenum data_type;
            void *data;
            size_t data_size;
        } texture_image_3d_ext;

        struct {
            GLuint texture;
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLint zoffset;
            GLsizei width;
            GLsizei height;
            GLsizei depth;
            GLenum format;
            GLenum data_type;
            void *data;
            size_t data_size;
        } texture_sub_image_3d_ext;

        struct {
            GLuint texture;
            GLenum target;
            GLint level;
            GLenum internalformat;
            GLsizei width;
            GLsizei height;
            GLint border;
            void *data;
            size_t data_size;
        } compressed_texture_image_2d_ext;

        struct {
            GLuint texture;
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLsizei width;
            GLsizei height;
            GLenum format;
            void *data;
            size_t data_size;
        } compressed_texture_sub_image_2d_ext;

        struct {
            GLuint texture;
            GLenum target;
            GLint level;
            GLenum internalformat;
            GLsizei width;
            GLsizei height;
            GLsizei depth;
            GLint border;
            void *data;
            size_t data_size;
        } compressed_texture_image_3d_ext;

        struct {
            GLuint texture;
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLint zoffset;
            GLsizei width;
            GLsizei height;
            GLsizei depth;
            GLenum format;
            void *data;
            size_t data_size;
        } compressed_texture_sub_image_3d_ext;

        struct {
            GLuint texture;
            GLenum target;
            GLint level;
            GLenum internalformat;
            GLint x;
            GLint y;
            GLsizei width;
            GLsizei height;
            GLint border;
        } copy_texture_image_2d_ext;

        struct {
            GLuint texture;
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLint x;
            GLint y;
            GLsizei width;
            GLsizei height;
        } copy_texture_sub_image_2d_ext;

        struct {
            GLuint texture;
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLint zoffset;
            GLint x;
            GLint y;
            GLsizei width;
            GLsizei height;
        } copy_texture_sub_image_3d_ext;

        struct {
            GLenum texunit;
            GLenum target;
            GLint level;
            GLenum internalformat;
            GLint x;
            GLint y;
            GLsizei width;
            GLsizei height;
            GLint border;
        } copy_multi_tex_image_2d;

        struct {
            GLenum texunit;
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLint x;
            GLint y;
            GLsizei width;
            GLsizei height;
        } copy_multi_tex_sub_image_2d;

        struct {
            GLenum texunit;
            GLenum target;
            GLint level;
            GLint xoffset;
            GLint yoffset;
            GLint zoffset;
            GLint x;
            GLint y;
            GLsizei width;
            GLsizei height;
        } copy_multi_tex_sub_image_3d;

        struct {
            GLenum pname;
            GLfloat param;
        } pixel_transfer;

        struct {
            GLenum map;
            GLsizei size;
            GLdouble *values;
        } pixel_map;

        struct {
            GLenum func;
            GLfloat ref;
        } alpha_func;

        GLfloat clear_index;
        GLfloat clear_color[4];
        GLfloat clear_depth;
        GLint clear_stencil;
        GLbitfield clear_mask;

        struct {
            GLint x;
            GLint y;
            GLsizei width;
            GLsizei height;
        } viewport;

        struct {
            GLint x;
            GLint y;
            GLsizei width;
            GLsizei height;
        } scissor;

        GLenum state_enum;
        GLboolean state_boolean;
        GLuint index_mask;
        GLboolean color_mask[4];

        struct {
            GLenum pname;
            GLfloat params[4];
            unsigned count;
        } fog;

        struct {
            GLenum light;
            GLenum pname;
            GLfloat params[4];
            unsigned count;
        } light;

        struct {
            GLenum pname;
            GLfloat params[4];
            unsigned count;
        } light_model;

        struct {
            GLenum face;
            GLenum pname;
            GLfloat params[4];
            unsigned count;
        } material;

        struct {
            GLenum face;
            GLenum mode;
        } color_material;

        struct {
            GLenum mode;
        } shade_model;

        struct {
            GLenum target;
            GLenum pname;
            GLfloat params[4];
        } tex_env;

        struct {
            GLenum texunit;
            GLenum target;
            GLenum pname;
            GLfloat params[4];
        } multi_tex_env_ext;

        struct {
            GLenum coord;
            GLenum pname;
            GLfloat params[4];
            unsigned count;
        } texgen;

        struct {
            GLenum texunit;
            GLenum coord;
            GLenum pname;
            GLfloat params[4];
            unsigned count;
        } multi_texgen_ext;

        struct {
            GLuint program;
            GLint location;
            GLsizei count;
            unsigned components;
            unsigned value_kind;
            GLboolean scalar;
            GLboolean transpose;
            void *data;
            size_t data_size;
        } program_uniform_ext;

        struct {
            GLenum target;
            GLuint index;
            GLsizei count;
            GLboolean local;
            GLfloat *values;
        } arb_program_parameters;

        struct {
            GLenum target;
            GLenum format;
            GLsizei len;
            unsigned char *string;
        } arb_program_string;

        struct {
            GLenum plane;
            GLfloat equation[4];
        } clip_plane;

        struct {
            GLenum face;
            GLenum func;
            GLint ref;
            GLuint mask;
        } stencil_func;

        struct {
            GLenum face;
            GLuint mask;
        } stencil_mask;

        struct {
            GLenum face;
            GLenum sfail;
            GLenum dpfail;
            GLenum dppass;
        } stencil_op;

        struct {
            GLenum src_rgb;
            GLenum dst_rgb;
            GLenum src_alpha;
            GLenum dst_alpha;
        } blend_func;

        struct {
            GLenum rgb;
            GLenum alpha;
        } blend_equation;

        struct {
            GLfloat near_value;
            GLfloat far_value;
        } depth_range;

        GLfloat blend_color[4];
        GLfloat line_width;

        struct {
            GLfloat factor;
            GLfloat units;
        } polygon_offset;

        struct {
            GLfloat value;
            GLboolean invert;
        } sample_coverage;

        struct {
            GLenum target;
            GLenum mode;
        } hint;

        struct {
            GLenum target;
            GLdouble u1;
            GLdouble u2;
            GLint order;
            unsigned components;
            GLdouble *coeff;
        } map1;

        struct {
            GLenum target;
            GLdouble u1;
            GLdouble u2;
            GLdouble v1;
            GLdouble v2;
            GLint uorder;
            GLint vorder;
            unsigned components;
            GLdouble *coeff;
        } map2;

        struct {
            GLdouble u;
            GLdouble v;
        } eval_coord;

        struct {
            GLint un;
            GLdouble u1;
            GLdouble u2;
        } grid1;

        struct {
            GLint un;
            GLint vn;
            GLdouble u1;
            GLdouble u2;
            GLdouble v1;
            GLdouble v2;
        } grid2;

        struct {
            GLint i;
            GLint j;
        } eval_point;

        struct {
            GLenum mode;
            GLint i1;
            GLint i2;
            GLint j1;
            GLint j2;
        } eval_mesh;

        struct {
            GLdouble x;
            GLdouble y;
            GLdouble z;
            GLdouble w;
        } raster_pos;

        struct {
            GLfloat x;
            GLfloat y;
            GLfloat z;
        } window_pos;

        struct {
            GLfloat xfactor;
            GLfloat yfactor;
        } pixel_zoom;

        struct {
            GLsizei width;
            GLsizei height;
            GLenum format;
            GLenum data_type;
            void *data;
            size_t data_size;
        } draw_pixels;

        struct {
            GLsizei width;
            GLsizei height;
            GLfloat xorig;
            GLfloat yorig;
            GLfloat xmove;
            GLfloat ymove;
            void *data;
            size_t data_size;
        } bitmap;

        struct {
            GLint x;
            GLint y;
            GLsizei width;
            GLsizei height;
            GLenum copy_type;
        } copy_pixels;
        struct mgl_draw_snapshot *draw_snapshot;
        GLuint name_value;
        GLuint list_name;
        GLuint list_base;

        struct {
            GLsizei count;
            GLuint *offsets;
        } call_lists;
    } u;
};

struct mgl_display_list {
    GLuint name;
    struct mgl_list_command *commands;
    size_t count;
    size_t capacity;
    unsigned replay_refs;
    GLboolean retired;
    struct mgl_display_list *next;
};

static void destroy_command(struct mgl_list_command *command) {
    if (command == NULL)
        return;
    if (command->type == MGL_LIST_CMD_PRIORITIZE_TEXTURES) {
        free(command->u.prioritize_textures.textures);
        free(command->u.prioritize_textures.priorities);
    } else if (command->type == MGL_LIST_CMD_MULTI_TEX_IMAGE_2D)
        free(command->u.multi_tex_image_2d.data);
    else if (command->type == MGL_LIST_CMD_MULTI_TEX_SUB_IMAGE_2D)
        free(command->u.multi_tex_sub_image_2d.data);
    else if (command->type == MGL_LIST_CMD_MULTI_TEX_IMAGE_3D)
        free(command->u.multi_tex_image_3d.data);
    else if (command->type == MGL_LIST_CMD_MULTI_TEX_SUB_IMAGE_3D)
        free(command->u.multi_tex_sub_image_3d.data);
    else if (command->type == MGL_LIST_CMD_COMPRESSED_MULTI_TEX_IMAGE_2D)
        free(command->u.compressed_multi_tex_image_2d.data);
    else if (command->type == MGL_LIST_CMD_COMPRESSED_MULTI_TEX_SUB_IMAGE_2D)
        free(command->u.compressed_multi_tex_sub_image_2d.data);
    else if (command->type == MGL_LIST_CMD_COMPRESSED_MULTI_TEX_IMAGE_3D)
        free(command->u.compressed_multi_tex_image_3d.data);
    else if (command->type == MGL_LIST_CMD_COMPRESSED_MULTI_TEX_SUB_IMAGE_3D)
        free(command->u.compressed_multi_tex_sub_image_3d.data);
    else if (command->type == MGL_LIST_CMD_TEX_IMAGE_2D)
        free(command->u.tex_image_2d.data);
    else if (command->type == MGL_LIST_CMD_TEX_SUB_IMAGE_2D)
        free(command->u.tex_sub_image_2d.data);
    else if (command->type == MGL_LIST_CMD_TEX_IMAGE_3D)
        free(command->u.tex_image_3d.data);
    else if (command->type == MGL_LIST_CMD_TEX_SUB_IMAGE_3D)
        free(command->u.tex_sub_image_3d.data);
    else if (command->type == MGL_LIST_CMD_COMPRESSED_TEX_IMAGE_2D)
        free(command->u.compressed_tex_image_2d.data);
    else if (command->type == MGL_LIST_CMD_COMPRESSED_TEX_IMAGE_3D)
        free(command->u.compressed_tex_image_3d.data);
    else if (command->type == MGL_LIST_CMD_COMPRESSED_TEX_SUB_IMAGE_2D)
        free(command->u.compressed_tex_sub_image_2d.data);
    else if (command->type == MGL_LIST_CMD_COMPRESSED_TEX_SUB_IMAGE_3D)
        free(command->u.compressed_tex_sub_image_3d.data);
    else if (command->type == MGL_LIST_CMD_COMPRESSED_TEXTURE_IMAGE_2D_EXT)
        free(command->u.compressed_texture_image_2d_ext.data);
    else if (command->type == MGL_LIST_CMD_COMPRESSED_TEXTURE_SUB_IMAGE_2D_EXT)
        free(command->u.compressed_texture_sub_image_2d_ext.data);
    else if (command->type == MGL_LIST_CMD_COMPRESSED_TEXTURE_IMAGE_3D_EXT)
        free(command->u.compressed_texture_image_3d_ext.data);
    else if (command->type == MGL_LIST_CMD_COMPRESSED_TEXTURE_SUB_IMAGE_3D_EXT)
        free(command->u.compressed_texture_sub_image_3d_ext.data);
    else if (command->type == MGL_LIST_CMD_TEXTURE_IMAGE_2D_EXT)
        free(command->u.texture_image_2d_ext.data);
    else if (command->type == MGL_LIST_CMD_TEXTURE_SUB_IMAGE_2D_EXT)
        free(command->u.texture_sub_image_2d_ext.data);
    else if (command->type == MGL_LIST_CMD_TEXTURE_IMAGE_3D_EXT)
        free(command->u.texture_image_3d_ext.data);
    else if (command->type == MGL_LIST_CMD_TEXTURE_SUB_IMAGE_3D_EXT)
        free(command->u.texture_sub_image_3d_ext.data);
    else if (command->type == MGL_LIST_CMD_MAP1)
        free(command->u.map1.coeff);
    else if (command->type == MGL_LIST_CMD_MAP2)
        free(command->u.map2.coeff);
    else if (command->type == MGL_LIST_CMD_PROGRAM_UNIFORM_EXT)
        free(command->u.program_uniform_ext.data);
    else if (command->type == MGL_LIST_CMD_ARB_PROGRAM_PARAMETERS)
        free(command->u.arb_program_parameters.values);
    else if (command->type == MGL_LIST_CMD_ARB_PROGRAM_STRING)
        free(command->u.arb_program_string.string);
    else if (command->type == MGL_LIST_CMD_PIXEL_MAP)
        free(command->u.pixel_map.values);
    else if (command->type == MGL_LIST_CMD_DRAW_PIXELS)
        free(command->u.draw_pixels.data);
    else if (command->type == MGL_LIST_CMD_BITMAP)
        free(command->u.bitmap.data);
    else if (command->type == MGL_LIST_CMD_CALL_LISTS)
        free(command->u.call_lists.offsets);
    else if (command->type == MGL_LIST_CMD_DRAW_SNAPSHOT)
        mgl_draw_snapshot_destroy(command->u.draw_snapshot);
    memset(command, 0, sizeof(*command));
}

static void clear_list_commands(struct mgl_display_list *list) {
    size_t i;
    if (list == NULL)
        return;
    for (i = 0u; i < list->count; ++i)
        destroy_command(&list->commands[i]);
    free(list->commands);
    list->commands = NULL;
    list->count = 0u;
    list->capacity = 0u;
}

static struct mgl_display_list *find_list(struct mgl_share_group *group, GLuint name) {
    struct mgl_display_list *it;
    if (group == NULL || name == 0u)
        return NULL;
    for (it = group->display_lists; it != NULL; it = it->next)
        if (it->name == name)
            return it;
    return NULL;
}

static void destroy_list(struct mgl_display_list *list) {
    if (list == NULL)
        return;
    clear_list_commands(list);
    free(list);
}

static struct mgl_display_list *acquire_list(struct mgl_share_group *group, GLuint name) {
    struct mgl_display_list *list;
    if (group == NULL || name == 0u)
        return NULL;
    (void)pthread_mutex_lock(&group->lock);
    list = find_list(group, name);
    if (list != NULL)
        ++list->replay_refs;
    (void)pthread_mutex_unlock(&group->lock);
    return list;
}

static void release_list(struct mgl_share_group *group, struct mgl_display_list *list) {
    int destroy = 0;
    if (group == NULL || list == NULL)
        return;
    (void)pthread_mutex_lock(&group->lock);
    if (list->replay_refs > 0u)
        --list->replay_refs;
    if (list->replay_refs == 0u && list->retired)
        destroy = 1;
    (void)pthread_mutex_unlock(&group->lock);
    if (destroy)
        destroy_list(list);
}

static int compile_storage_failure(struct mgl_context *ctx) {
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    ctx->list_compile_failed = GL_TRUE;
    return ctx->list_compile_mode == GL_COMPILE;
}

static int append_command(struct mgl_context *ctx, const struct mgl_list_command *command) {
    struct mgl_display_list *list = ctx->compiling_list;
    struct mgl_list_command *next;
    size_t capacity;
    if (list == NULL || ctx->list_compile_failed)
        return 0;
    if (list->count == list->capacity) {
        capacity = list->capacity == 0u ? 16u : list->capacity * 2u;
        if (capacity < list->capacity || capacity > SIZE_MAX / sizeof(*next))
            return 0;
        next = realloc(list->commands, capacity * sizeof(*next));
        if (next == NULL)
            return 0;
        list->commands = next;
        list->capacity = capacity;
    }
    list->commands[list->count++] = *command;
    return 1;
}

static int record_command(struct mgl_context *ctx, const struct mgl_list_command *command) {
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    if (ctx->list_compile_failed)
        return ctx->list_compile_mode == GL_COMPILE;
    if (!append_command(ctx, command))
        return compile_storage_failure(ctx);
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_active_texture(struct mgl_context *ctx, GLenum texture) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_ACTIVE_TEXTURE;
    command.u.enum_value = texture;
    return record_command(ctx, &command);
}

int mgl_list_record_matrix_mode(struct mgl_context *ctx, GLenum mode) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MATRIX_MODE;
    command.u.enum_value = mode;
    return record_command(ctx, &command);
}

int mgl_list_record_load_identity(struct mgl_context *ctx) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_LOAD_IDENTITY;
    return record_command(ctx, &command);
}

int mgl_list_record_load_matrixf(struct mgl_context *ctx, const GLfloat *matrix) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_LOAD_MATRIXF;
    memcpy(command.u.matrix, matrix, sizeof(command.u.matrix));
    return record_command(ctx, &command);
}

int mgl_list_record_mult_matrixf(struct mgl_context *ctx, const GLfloat *matrix) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MULT_MATRIXF;
    memcpy(command.u.matrix, matrix, sizeof(command.u.matrix));
    return record_command(ctx, &command);
}

int mgl_list_record_push_matrix(struct mgl_context *ctx) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_PUSH_MATRIX;
    return record_command(ctx, &command);
}

int mgl_list_record_pop_matrix(struct mgl_context *ctx) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_POP_MATRIX;
    return record_command(ctx, &command);
}

int mgl_list_record_matrix_load_ext(struct mgl_context *ctx, GLenum matrix_mode,
                                    const GLfloat *matrix) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MATRIX_LOAD_EXT;
    command.u.matrix_ext.matrix_mode = matrix_mode;
    memcpy(command.u.matrix_ext.matrix, matrix, sizeof(command.u.matrix_ext.matrix));
    return record_command(ctx, &command);
}

int mgl_list_record_matrix_mult_ext(struct mgl_context *ctx, GLenum matrix_mode,
                                    const GLfloat *matrix) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MATRIX_MULT_EXT;
    command.u.matrix_ext.matrix_mode = matrix_mode;
    memcpy(command.u.matrix_ext.matrix, matrix, sizeof(command.u.matrix_ext.matrix));
    return record_command(ctx, &command);
}

static int record_matrix_mode_only_ext(struct mgl_context *ctx, enum mgl_list_command_type type,
                                       GLenum matrix_mode) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = type;
    command.u.enum_value = matrix_mode;
    return record_command(ctx, &command);
}

int mgl_list_record_matrix_load_identity_ext(struct mgl_context *ctx, GLenum matrix_mode) {
    return record_matrix_mode_only_ext(ctx, MGL_LIST_CMD_MATRIX_LOAD_IDENTITY_EXT, matrix_mode);
}

int mgl_list_record_matrix_push_ext(struct mgl_context *ctx, GLenum matrix_mode) {
    return record_matrix_mode_only_ext(ctx, MGL_LIST_CMD_MATRIX_PUSH_EXT, matrix_mode);
}

int mgl_list_record_matrix_pop_ext(struct mgl_context *ctx, GLenum matrix_mode) {
    return record_matrix_mode_only_ext(ctx, MGL_LIST_CMD_MATRIX_POP_EXT, matrix_mode);
}

int mgl_list_record_color4f(struct mgl_context *ctx, GLfloat r, GLfloat g, GLfloat b, GLfloat a) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COLOR4F;
    command.u.color[0] = r;
    command.u.color[1] = g;
    command.u.color[2] = b;
    command.u.color[3] = a;
    return record_command(ctx, &command);
}

int mgl_list_record_secondary3f(struct mgl_context *ctx, GLfloat r, GLfloat g, GLfloat b) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_SECONDARY3F;
    command.u.vec4[0] = r;
    command.u.vec4[1] = g;
    command.u.vec4[2] = b;
    command.u.vec4[3] = 1.0f;
    return record_command(ctx, &command);
}

int mgl_list_record_normal3f(struct mgl_context *ctx, GLfloat x, GLfloat y, GLfloat z) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_NORMAL3F;
    command.u.vec4[0] = x;
    command.u.vec4[1] = y;
    command.u.vec4[2] = z;
    return record_command(ctx, &command);
}

int mgl_list_record_texcoord4f(struct mgl_context *ctx, unsigned unit, GLfloat s, GLfloat t,
                               GLfloat r, GLfloat q) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEXCOORD4F;
    command.u.texcoord.unit = unit;
    command.u.texcoord.value[0] = s;
    command.u.texcoord.value[1] = t;
    command.u.texcoord.value[2] = r;
    command.u.texcoord.value[3] = q;
    return record_command(ctx, &command);
}

int mgl_list_record_fog_coord(struct mgl_context *ctx, GLfloat coord) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_FOG_COORD;
    command.u.vec4[0] = coord;
    return record_command(ctx, &command);
}

int mgl_list_record_enable(struct mgl_context *ctx, GLenum cap, GLboolean enabled) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = enabled ? MGL_LIST_CMD_ENABLE : MGL_LIST_CMD_DISABLE;
    command.u.cap = cap;
    return record_command(ctx, &command);
}

int mgl_list_record_push_attrib(struct mgl_context *ctx, GLbitfield mask) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_PUSH_ATTRIB;
    command.u.enum_value = (GLenum)mask;
    return record_command(ctx, &command);
}

int mgl_list_record_pop_attrib(struct mgl_context *ctx) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_POP_ATTRIB;
    return record_command(ctx, &command);
}

int mgl_list_record_pixel_transfer(struct mgl_context *ctx, GLenum pname, GLfloat param) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_PIXEL_TRANSFER;
    command.u.pixel_transfer.pname = pname;
    command.u.pixel_transfer.param = param;
    return record_command(ctx, &command);
}

int mgl_list_record_pixel_map(struct mgl_context *ctx, GLenum map, GLsizei mapsize,
                              const GLdouble *values) {
    struct mgl_list_command command;
    size_t bytes;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    if (mapsize < 0 || (mapsize > 0 && values == NULL))
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_PIXEL_MAP;
    command.u.pixel_map.map = map;
    command.u.pixel_map.size = mapsize;
    bytes = (size_t)mapsize * sizeof(GLdouble);
    if (bytes != 0u) {
        command.u.pixel_map.values = malloc(bytes);
        if (command.u.pixel_map.values == NULL)
            return compile_storage_failure(ctx);
        memcpy(command.u.pixel_map.values, values, bytes);
    }
    if (!append_command(ctx, &command)) {
        free(command.u.pixel_map.values);
        return compile_storage_failure(ctx);
    }
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_alpha_func(struct mgl_context *ctx, GLenum func, GLfloat ref) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_ALPHA_FUNC;
    command.u.alpha_func.func = func;
    command.u.alpha_func.ref = ref;
    return record_command(ctx, &command);
}

int mgl_list_record_clear_index(struct mgl_context *ctx, GLfloat index) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_CLEAR_INDEX;
    command.u.clear_index = index;
    return record_command(ctx, &command);
}

int mgl_list_record_clear_color(struct mgl_context *ctx, GLfloat red, GLfloat green, GLfloat blue,
                                GLfloat alpha) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_CLEAR_COLOR;
    command.u.clear_color[0] = red;
    command.u.clear_color[1] = green;
    command.u.clear_color[2] = blue;
    command.u.clear_color[3] = alpha;
    return record_command(ctx, &command);
}

int mgl_list_record_clear_depth(struct mgl_context *ctx, GLfloat depth) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_CLEAR_DEPTH;
    command.u.clear_depth = depth;
    return record_command(ctx, &command);
}

int mgl_list_record_clear_stencil(struct mgl_context *ctx, GLint stencil) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_CLEAR_STENCIL;
    command.u.clear_stencil = stencil;
    return record_command(ctx, &command);
}

int mgl_list_record_clear(struct mgl_context *ctx, GLbitfield mask) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_CLEAR;
    command.u.clear_mask = mask;
    return record_command(ctx, &command);
}

int mgl_list_record_viewport(struct mgl_context *ctx, GLint x, GLint y, GLsizei width,
                             GLsizei height) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_VIEWPORT;
    command.u.viewport.x = x;
    command.u.viewport.y = y;
    command.u.viewport.width = width;
    command.u.viewport.height = height;
    return record_command(ctx, &command);
}

int mgl_list_record_scissor(struct mgl_context *ctx, GLint x, GLint y, GLsizei width,
                            GLsizei height) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_SCISSOR;
    command.u.scissor.x = x;
    command.u.scissor.y = y;
    command.u.scissor.width = width;
    command.u.scissor.height = height;
    return record_command(ctx, &command);
}

int mgl_list_record_depth_func(struct mgl_context *ctx, GLenum func) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_DEPTH_FUNC;
    command.u.state_enum = func;
    return record_command(ctx, &command);
}

int mgl_list_record_depth_mask(struct mgl_context *ctx, GLboolean flag) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_DEPTH_MASK;
    command.u.state_boolean = flag ? GL_TRUE : GL_FALSE;
    return record_command(ctx, &command);
}

int mgl_list_record_index_mask(struct mgl_context *ctx, GLuint mask) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_INDEX_MASK;
    command.u.index_mask = mask;
    return record_command(ctx, &command);
}

int mgl_list_record_color_mask(struct mgl_context *ctx, GLboolean red, GLboolean green,
                               GLboolean blue, GLboolean alpha) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COLOR_MASK;
    command.u.color_mask[0] = red ? GL_TRUE : GL_FALSE;
    command.u.color_mask[1] = green ? GL_TRUE : GL_FALSE;
    command.u.color_mask[2] = blue ? GL_TRUE : GL_FALSE;
    command.u.color_mask[3] = alpha ? GL_TRUE : GL_FALSE;
    return record_command(ctx, &command);
}

int mgl_list_record_cull_face(struct mgl_context *ctx, GLenum mode) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_CULL_FACE;
    command.u.state_enum = mode;
    return record_command(ctx, &command);
}

int mgl_list_record_front_face(struct mgl_context *ctx, GLenum mode) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_FRONT_FACE;
    command.u.state_enum = mode;
    return record_command(ctx, &command);
}

int mgl_list_record_fog(struct mgl_context *ctx, GLenum pname, const GLfloat *params,
                        unsigned count) {
    struct mgl_list_command command;
    unsigned i;
    if (params == NULL || count == 0u || count > 4u)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_FOG;
    command.u.fog.pname = pname;
    command.u.fog.count = count;
    for (i = 0u; i < count; ++i)
        command.u.fog.params[i] = params[i];
    return record_command(ctx, &command);
}

int mgl_list_record_light(struct mgl_context *ctx, GLenum light, GLenum pname,
                          const GLfloat *params, unsigned count) {
    struct mgl_list_command command;
    unsigned i;
    if (params == NULL || count == 0u || count > 4u)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_LIGHT;
    command.u.light.light = light;
    command.u.light.pname = pname;
    command.u.light.count = count;
    for (i = 0u; i < count; ++i)
        command.u.light.params[i] = params[i];
    return record_command(ctx, &command);
}

int mgl_list_record_light_model(struct mgl_context *ctx, GLenum pname, const GLfloat *params,
                                unsigned count) {
    struct mgl_list_command command;
    unsigned i;
    if (params == NULL || count == 0u || count > 4u)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_LIGHT_MODEL;
    command.u.light_model.pname = pname;
    command.u.light_model.count = count;
    for (i = 0u; i < count; ++i)
        command.u.light_model.params[i] = params[i];
    return record_command(ctx, &command);
}

int mgl_list_record_material(struct mgl_context *ctx, GLenum face, GLenum pname,
                             const GLfloat *params, unsigned count) {
    struct mgl_list_command command;
    unsigned i;
    if (params == NULL || count == 0u || count > 4u)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MATERIAL;
    command.u.material.face = face;
    command.u.material.pname = pname;
    command.u.material.count = count;
    for (i = 0u; i < count; ++i)
        command.u.material.params[i] = params[i];
    return record_command(ctx, &command);
}

int mgl_list_record_color_material(struct mgl_context *ctx, GLenum face, GLenum mode) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COLOR_MATERIAL;
    command.u.color_material.face = face;
    command.u.color_material.mode = mode;
    return record_command(ctx, &command);
}

int mgl_list_record_shade_model(struct mgl_context *ctx, GLenum mode) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_SHADE_MODEL;
    command.u.shade_model.mode = mode;
    return record_command(ctx, &command);
}

int mgl_list_record_tex_env(struct mgl_context *ctx, GLenum target, GLenum pname,
                            const GLfloat *params) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEX_ENV;
    command.u.tex_env.target = target;
    command.u.tex_env.pname = pname;
    if (params == NULL)
        return 0;
    memcpy(command.u.tex_env.params, params,
           (pname == GL_TEXTURE_ENV_COLOR ? 4u : 1u) * sizeof(GLfloat));
    return record_command(ctx, &command);
}

int mgl_list_record_multi_tex_env_ext(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                      GLenum pname, const GLfloat *params) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MULTI_TEX_ENV_EXT;
    command.u.multi_tex_env_ext.texunit = texunit;
    command.u.multi_tex_env_ext.target = target;
    command.u.multi_tex_env_ext.pname = pname;
    if (params == NULL)
        return 0;
    memcpy(command.u.multi_tex_env_ext.params, params,
           (pname == GL_TEXTURE_ENV_COLOR ? 4u : 1u) * sizeof(GLfloat));
    return record_command(ctx, &command);
}

int mgl_list_record_texgen(struct mgl_context *ctx, GLenum coord, GLenum pname,
                           const GLfloat *params, unsigned count) {
    struct mgl_list_command command;
    unsigned i;
    if (params == NULL || count == 0u || count > 4u)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEXGEN;
    command.u.texgen.coord = coord;
    command.u.texgen.pname = pname;
    command.u.texgen.count = count;
    for (i = 0u; i < count; ++i)
        command.u.texgen.params[i] = params[i];
    return record_command(ctx, &command);
}

int mgl_list_record_multi_texgen_ext(struct mgl_context *ctx, GLenum texunit, GLenum coord,
                                     GLenum pname, const GLfloat *params, unsigned count) {
    struct mgl_list_command command;
    unsigned i;
    if (params == NULL || count == 0u || count > 4u)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MULTI_TEXGEN_EXT;
    command.u.multi_texgen_ext.texunit = texunit;
    command.u.multi_texgen_ext.coord = coord;
    command.u.multi_texgen_ext.pname = pname;
    command.u.multi_texgen_ext.count = count;
    for (i = 0u; i < count; ++i)
        command.u.multi_texgen_ext.params[i] = params[i];
    return record_command(ctx, &command);
}

enum {
    MGL_LIST_PROGRAM_UNIFORM_FLOAT = 1u,
    MGL_LIST_PROGRAM_UNIFORM_INT = 2u,
    MGL_LIST_PROGRAM_UNIFORM_MATRIX = 3u
};

static int record_program_uniform_ext(struct mgl_context *ctx, GLuint program, GLint location,
                                      unsigned components, GLsizei count, unsigned value_kind,
                                      GLboolean scalar, GLboolean transpose, const void *values,
                                      size_t element_size) {
    struct mgl_list_command command;
    size_t elements;
    size_t bytes;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    if (count < 0 || components == 0u)
        return 0;
    if ((size_t)count > SIZE_MAX / (size_t)components) {
        return compile_storage_failure(ctx);
    }
    elements = (size_t)count * (size_t)components;
    if (elements > SIZE_MAX / element_size) {
        return compile_storage_failure(ctx);
    }
    bytes = elements * element_size;
    if (bytes != 0u && values == NULL)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_PROGRAM_UNIFORM_EXT;
    command.u.program_uniform_ext.program = program;
    command.u.program_uniform_ext.location = location;
    command.u.program_uniform_ext.count = count;
    command.u.program_uniform_ext.components = components;
    command.u.program_uniform_ext.value_kind = value_kind;
    command.u.program_uniform_ext.scalar = scalar;
    command.u.program_uniform_ext.transpose = transpose;
    command.u.program_uniform_ext.data_size = bytes;
    if (bytes != 0u) {
        command.u.program_uniform_ext.data = malloc(bytes);
        if (command.u.program_uniform_ext.data == NULL) {
            return compile_storage_failure(ctx);
        }
        memcpy(command.u.program_uniform_ext.data, values, bytes);
    }
    if (!append_command(ctx, &command)) {
        free(command.u.program_uniform_ext.data);
        return compile_storage_failure(ctx);
    }
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_program_uniform_f_ext(struct mgl_context *ctx, GLuint program, GLint location,
                                          unsigned components, GLsizei count, const GLfloat *values,
                                          GLboolean scalar) {
    return record_program_uniform_ext(ctx, program, location, components, count,
                                      MGL_LIST_PROGRAM_UNIFORM_FLOAT, scalar, GL_FALSE, values,
                                      sizeof(GLfloat));
}

int mgl_list_record_program_uniform_i_ext(struct mgl_context *ctx, GLuint program, GLint location,
                                          unsigned components, GLsizei count, const GLint *values,
                                          GLboolean scalar) {
    return record_program_uniform_ext(ctx, program, location, components, count,
                                      MGL_LIST_PROGRAM_UNIFORM_INT, scalar, GL_FALSE, values,
                                      sizeof(GLint));
}

int mgl_list_record_program_uniform_matrix_ext(struct mgl_context *ctx, GLuint program,
                                               GLint location, unsigned dimension, GLsizei count,
                                               GLboolean transpose, const GLfloat *values) {
    if (dimension < 2u || dimension > 4u)
        return 0;
    return record_program_uniform_ext(ctx, program, location, dimension * dimension, count,
                                      MGL_LIST_PROGRAM_UNIFORM_MATRIX, GL_FALSE, transpose, values,
                                      sizeof(GLfloat));
}

int mgl_list_record_arb_program_parameters(struct mgl_context *ctx, GLenum target, GLuint index,
                                           GLsizei count, const GLfloat *values, GLboolean local) {
    struct mgl_list_command command;
    size_t elements;
    size_t bytes;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    if (count <= 0 || values == NULL)
        return 0;
    elements = (size_t)count * 4u;
    if (elements > SIZE_MAX / sizeof(GLfloat))
        return compile_storage_failure(ctx);
    bytes = elements * sizeof(GLfloat);
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_ARB_PROGRAM_PARAMETERS;
    command.u.arb_program_parameters.target = target;
    command.u.arb_program_parameters.index = index;
    command.u.arb_program_parameters.count = count;
    command.u.arb_program_parameters.local = local;
    command.u.arb_program_parameters.values = malloc(bytes);
    if (command.u.arb_program_parameters.values == NULL)
        return compile_storage_failure(ctx);
    memcpy(command.u.arb_program_parameters.values, values, bytes);
    if (!append_command(ctx, &command)) {
        free(command.u.arb_program_parameters.values);
        return compile_storage_failure(ctx);
    }
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_arb_program_string(struct mgl_context *ctx, GLenum target, GLenum format,
                                       GLsizei len, const void *string) {
    struct mgl_list_command command;
    size_t bytes;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    if (len < 0 || (len > 0 && string == NULL))
        return 0;
    bytes = (size_t)len;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_ARB_PROGRAM_STRING;
    command.u.arb_program_string.target = target;
    command.u.arb_program_string.format = format;
    command.u.arb_program_string.len = len;
    if (bytes != 0u) {
        command.u.arb_program_string.string = malloc(bytes);
        if (command.u.arb_program_string.string == NULL)
            return compile_storage_failure(ctx);
        memcpy(command.u.arb_program_string.string, string, bytes);
    }
    if (!append_command(ctx, &command)) {
        free(command.u.arb_program_string.string);
        return compile_storage_failure(ctx);
    }
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_clip_plane(struct mgl_context *ctx, GLenum plane, const GLfloat *equation) {
    struct mgl_list_command command;
    if (equation == NULL)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_CLIP_PLANE;
    command.u.clip_plane.plane = plane;
    memcpy(command.u.clip_plane.equation, equation, sizeof(command.u.clip_plane.equation));
    return record_command(ctx, &command);
}

int mgl_list_record_stencil_func(struct mgl_context *ctx, GLenum face, GLenum func, GLint ref,
                                 GLuint mask) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_STENCIL_FUNC;
    command.u.stencil_func.face = face;
    command.u.stencil_func.func = func;
    command.u.stencil_func.ref = ref;
    command.u.stencil_func.mask = mask;
    return record_command(ctx, &command);
}

int mgl_list_record_stencil_mask(struct mgl_context *ctx, GLenum face, GLuint mask) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_STENCIL_MASK;
    command.u.stencil_mask.face = face;
    command.u.stencil_mask.mask = mask;
    return record_command(ctx, &command);
}

int mgl_list_record_stencil_op(struct mgl_context *ctx, GLenum face, GLenum sfail, GLenum dpfail,
                               GLenum dppass) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_STENCIL_OP;
    command.u.stencil_op.face = face;
    command.u.stencil_op.sfail = sfail;
    command.u.stencil_op.dpfail = dpfail;
    command.u.stencil_op.dppass = dppass;
    return record_command(ctx, &command);
}

int mgl_list_record_blend_func_separate(struct mgl_context *ctx, GLenum src_rgb, GLenum dst_rgb,
                                        GLenum src_alpha, GLenum dst_alpha) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_BLEND_FUNC_SEPARATE;
    command.u.blend_func.src_rgb = src_rgb;
    command.u.blend_func.dst_rgb = dst_rgb;
    command.u.blend_func.src_alpha = src_alpha;
    command.u.blend_func.dst_alpha = dst_alpha;
    return record_command(ctx, &command);
}

int mgl_list_record_blend_equation_separate(struct mgl_context *ctx, GLenum rgb, GLenum alpha) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_BLEND_EQUATION_SEPARATE;
    command.u.blend_equation.rgb = rgb;
    command.u.blend_equation.alpha = alpha;
    return record_command(ctx, &command);
}

int mgl_list_record_depth_range(struct mgl_context *ctx, GLfloat near_value, GLfloat far_value) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_DEPTH_RANGE;
    command.u.depth_range.near_value = near_value;
    command.u.depth_range.far_value = far_value;
    return record_command(ctx, &command);
}

int mgl_list_record_blend_color(struct mgl_context *ctx, GLfloat red, GLfloat green, GLfloat blue,
                                GLfloat alpha) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_BLEND_COLOR;
    command.u.blend_color[0] = red;
    command.u.blend_color[1] = green;
    command.u.blend_color[2] = blue;
    command.u.blend_color[3] = alpha;
    return record_command(ctx, &command);
}

int mgl_list_record_line_width(struct mgl_context *ctx, GLfloat width) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_LINE_WIDTH;
    command.u.line_width = width;
    return record_command(ctx, &command);
}

int mgl_list_record_polygon_offset(struct mgl_context *ctx, GLfloat factor, GLfloat units) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_POLYGON_OFFSET;
    command.u.polygon_offset.factor = factor;
    command.u.polygon_offset.units = units;
    return record_command(ctx, &command);
}

int mgl_list_record_sample_coverage(struct mgl_context *ctx, GLfloat value, GLboolean invert) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_SAMPLE_COVERAGE;
    command.u.sample_coverage.value = value;
    command.u.sample_coverage.invert = invert;
    return record_command(ctx, &command);
}

int mgl_list_record_hint(struct mgl_context *ctx, GLenum target, GLenum mode) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_HINT;
    command.u.hint.target = target;
    command.u.hint.mode = mode;
    return record_command(ctx, &command);
}

int mgl_list_record_set_fence_nv(struct mgl_context *ctx, GLuint name, GLenum condition) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_SET_FENCE_NV;
    command.u.fence.name = name;
    command.u.fence.condition = condition;
    return record_command(ctx, &command);
}

int mgl_list_record_enable_indexed(struct mgl_context *ctx, GLenum cap, GLuint index,
                                   GLboolean enabled, GLboolean ext_semantics) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = ext_semantics ? MGL_LIST_CMD_ENABLE_INDEXED_EXT : MGL_LIST_CMD_ENABLE_INDEXED;
    command.u.indexed_enable.cap = cap;
    command.u.indexed_enable.index = index;
    command.u.indexed_enable.enabled = enabled;
    return record_command(ctx, &command);
}

int mgl_list_record_color_mask_indexed(struct mgl_context *ctx, GLuint index, GLboolean red,
                                       GLboolean green, GLboolean blue, GLboolean alpha) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COLOR_MASK_INDEXED;
    command.u.indexed_color_mask.index = index;
    command.u.indexed_color_mask.mask[0] = red;
    command.u.indexed_color_mask.mask[1] = green;
    command.u.indexed_color_mask.mask[2] = blue;
    command.u.indexed_color_mask.mask[3] = alpha;
    return record_command(ctx, &command);
}

int mgl_list_record_blend_func_indexed(struct mgl_context *ctx, GLuint index, GLenum src_rgb,
                                       GLenum dst_rgb, GLenum src_alpha, GLenum dst_alpha) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_BLEND_FUNC_INDEXED;
    command.u.indexed_blend_func.index = index;
    command.u.indexed_blend_func.src_rgb = src_rgb;
    command.u.indexed_blend_func.dst_rgb = dst_rgb;
    command.u.indexed_blend_func.src_alpha = src_alpha;
    command.u.indexed_blend_func.dst_alpha = dst_alpha;
    return record_command(ctx, &command);
}

int mgl_list_record_blend_equation_indexed(struct mgl_context *ctx, GLuint index, GLenum rgb,
                                           GLenum alpha) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_BLEND_EQUATION_INDEXED;
    command.u.indexed_blend_equation.index = index;
    command.u.indexed_blend_equation.rgb = rgb;
    command.u.indexed_blend_equation.alpha = alpha;
    return record_command(ctx, &command);
}

int mgl_list_record_clear_buffer_iv(struct mgl_context *ctx, GLenum buffer, GLint drawbuffer,
                                    const GLint *value, unsigned count) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_CLEAR_BUFFER_IV;
    command.u.clear_buffer_iv.buffer = buffer;
    command.u.clear_buffer_iv.drawbuffer = drawbuffer;
    if (value != NULL && count != 0u)
        memcpy(command.u.clear_buffer_iv.value, value, count * sizeof(GLint));
    return record_command(ctx, &command);
}

int mgl_list_record_clear_buffer_uiv(struct mgl_context *ctx, GLenum buffer, GLint drawbuffer,
                                     const GLuint *value, unsigned count) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_CLEAR_BUFFER_UIV;
    command.u.clear_buffer_uiv.buffer = buffer;
    command.u.clear_buffer_uiv.drawbuffer = drawbuffer;
    if (value != NULL && count != 0u)
        memcpy(command.u.clear_buffer_uiv.value, value, count * sizeof(GLuint));
    return record_command(ctx, &command);
}

int mgl_list_record_clear_buffer_fv(struct mgl_context *ctx, GLenum buffer, GLint drawbuffer,
                                    const GLfloat *value, unsigned count) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_CLEAR_BUFFER_FV;
    command.u.clear_buffer_fv.buffer = buffer;
    command.u.clear_buffer_fv.drawbuffer = drawbuffer;
    if (value != NULL && count != 0u)
        memcpy(command.u.clear_buffer_fv.value, value, count * sizeof(GLfloat));
    return record_command(ctx, &command);
}

int mgl_list_record_clear_buffer_fi(struct mgl_context *ctx, GLenum buffer, GLint drawbuffer,
                                    GLfloat depth, GLint stencil) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_CLEAR_BUFFER_FI;
    command.u.clear_buffer_fi.buffer = buffer;
    command.u.clear_buffer_fi.drawbuffer = drawbuffer;
    command.u.clear_buffer_fi.depth = depth;
    command.u.clear_buffer_fi.stencil = stencil;
    return record_command(ctx, &command);
}

int mgl_list_record_draw_buffer(struct mgl_context *ctx, GLenum buffer) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_DRAW_BUFFER;
    command.u.enum_value = buffer;
    return record_command(ctx, &command);
}

int mgl_list_record_draw_buffers(struct mgl_context *ctx, GLsizei count, const GLenum *buffers) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_DRAW_BUFFERS;
    command.u.draw_buffers.count = count;
    if (count > 0 && buffers != NULL)
        memcpy(command.u.draw_buffers.buffers, buffers, (size_t)count * sizeof(GLenum));
    return record_command(ctx, &command);
}

int mgl_list_record_read_buffer(struct mgl_context *ctx, GLenum buffer) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_READ_BUFFER;
    command.u.enum_value = buffer;
    return record_command(ctx, &command);
}

int mgl_list_record_map1(struct mgl_context *ctx, GLenum target, GLdouble u1, GLdouble u2,
                         GLint order, unsigned components, const GLdouble *coeff) {
    struct mgl_list_command command;
    size_t count;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MAP1;
    command.u.map1.target = target;
    command.u.map1.u1 = u1;
    command.u.map1.u2 = u2;
    command.u.map1.order = order;
    command.u.map1.components = components;
    count = (size_t)order * components;
    command.u.map1.coeff = malloc(count * sizeof(GLdouble));
    if (command.u.map1.coeff == NULL)
        return compile_storage_failure(ctx);
    memcpy(command.u.map1.coeff, coeff, count * sizeof(GLdouble));
    if (!append_command(ctx, &command)) {
        free(command.u.map1.coeff);
        return compile_storage_failure(ctx);
    }
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_map2(struct mgl_context *ctx, GLenum target, GLdouble u1, GLdouble u2,
                         GLdouble v1, GLdouble v2, GLint uorder, GLint vorder, unsigned components,
                         const GLdouble *coeff) {
    struct mgl_list_command command;
    size_t count;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MAP2;
    command.u.map2.target = target;
    command.u.map2.u1 = u1;
    command.u.map2.u2 = u2;
    command.u.map2.v1 = v1;
    command.u.map2.v2 = v2;
    command.u.map2.uorder = uorder;
    command.u.map2.vorder = vorder;
    command.u.map2.components = components;
    count = (size_t)uorder * (size_t)vorder * components;
    command.u.map2.coeff = malloc(count * sizeof(GLdouble));
    if (command.u.map2.coeff == NULL)
        return compile_storage_failure(ctx);
    memcpy(command.u.map2.coeff, coeff, count * sizeof(GLdouble));
    if (!append_command(ctx, &command)) {
        free(command.u.map2.coeff);
        return compile_storage_failure(ctx);
    }
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_eval_coord1(struct mgl_context *ctx, GLdouble u) {
    struct mgl_list_command c;
    memset(&c, 0, sizeof(c));
    c.type = MGL_LIST_CMD_EVAL_COORD1;
    c.u.eval_coord.u = u;
    return record_command(ctx, &c);
}

int mgl_list_record_eval_coord2(struct mgl_context *ctx, GLdouble u, GLdouble v) {
    struct mgl_list_command c;
    memset(&c, 0, sizeof(c));
    c.type = MGL_LIST_CMD_EVAL_COORD2;
    c.u.eval_coord.u = u;
    c.u.eval_coord.v = v;
    return record_command(ctx, &c);
}

int mgl_list_record_map_grid1(struct mgl_context *ctx, GLint un, GLdouble u1, GLdouble u2) {
    struct mgl_list_command c;
    memset(&c, 0, sizeof(c));
    c.type = MGL_LIST_CMD_MAP_GRID1;
    c.u.grid1.un = un;
    c.u.grid1.u1 = u1;
    c.u.grid1.u2 = u2;
    return record_command(ctx, &c);
}

int mgl_list_record_map_grid2(struct mgl_context *ctx, GLint un, GLdouble u1, GLdouble u2, GLint vn,
                              GLdouble v1, GLdouble v2) {
    struct mgl_list_command c;
    memset(&c, 0, sizeof(c));
    c.type = MGL_LIST_CMD_MAP_GRID2;
    c.u.grid2.un = un;
    c.u.grid2.vn = vn;
    c.u.grid2.u1 = u1;
    c.u.grid2.u2 = u2;
    c.u.grid2.v1 = v1;
    c.u.grid2.v2 = v2;
    return record_command(ctx, &c);
}

int mgl_list_record_eval_point1(struct mgl_context *ctx, GLint i) {
    struct mgl_list_command c;
    memset(&c, 0, sizeof(c));
    c.type = MGL_LIST_CMD_EVAL_POINT1;
    c.u.eval_point.i = i;
    return record_command(ctx, &c);
}

int mgl_list_record_eval_point2(struct mgl_context *ctx, GLint i, GLint j) {
    struct mgl_list_command c;
    memset(&c, 0, sizeof(c));
    c.type = MGL_LIST_CMD_EVAL_POINT2;
    c.u.eval_point.i = i;
    c.u.eval_point.j = j;
    return record_command(ctx, &c);
}

int mgl_list_record_eval_mesh1(struct mgl_context *ctx, GLenum mode, GLint i1, GLint i2) {
    struct mgl_list_command c;
    memset(&c, 0, sizeof(c));
    c.type = MGL_LIST_CMD_EVAL_MESH1;
    c.u.eval_mesh.mode = mode;
    c.u.eval_mesh.i1 = i1;
    c.u.eval_mesh.i2 = i2;
    return record_command(ctx, &c);
}

int mgl_list_record_eval_mesh2(struct mgl_context *ctx, GLenum mode, GLint i1, GLint i2, GLint j1,
                               GLint j2) {
    struct mgl_list_command c;
    memset(&c, 0, sizeof(c));
    c.type = MGL_LIST_CMD_EVAL_MESH2;
    c.u.eval_mesh.mode = mode;
    c.u.eval_mesh.i1 = i1;
    c.u.eval_mesh.i2 = i2;
    c.u.eval_mesh.j1 = j1;
    c.u.eval_mesh.j2 = j2;
    return record_command(ctx, &c);
}

int mgl_list_record_raster_pos4d(struct mgl_context *ctx, GLdouble x, GLdouble y, GLdouble z,
                                 GLdouble w) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_RASTER_POS4D;
    command.u.raster_pos.x = x;
    command.u.raster_pos.y = y;
    command.u.raster_pos.z = z;
    command.u.raster_pos.w = w;
    return record_command(ctx, &command);
}

int mgl_list_record_window_pos3f(struct mgl_context *ctx, GLfloat x, GLfloat y, GLfloat z) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_WINDOW_POS3F;
    command.u.window_pos.x = x;
    command.u.window_pos.y = y;
    command.u.window_pos.z = z;
    return record_command(ctx, &command);
}

int mgl_list_record_pixel_zoom(struct mgl_context *ctx, GLfloat xfactor, GLfloat yfactor) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_PIXEL_ZOOM;
    command.u.pixel_zoom.xfactor = xfactor;
    command.u.pixel_zoom.yfactor = yfactor;
    return record_command(ctx, &command);
}

static int record_owned_payload(struct mgl_context *ctx, struct mgl_list_command *command,
                                const void *data, size_t data_size, void **slot) {
    int suppress;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    *slot = NULL;
    if (data_size != 0u) {
        *slot = malloc(data_size);
        if (*slot == NULL)
            return compile_storage_failure(ctx);
        memcpy(*slot, data, data_size);
    }
    if (!append_command(ctx, command)) {
        free(*slot);
        *slot = NULL;
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_draw_pixels(struct mgl_context *ctx, GLsizei width, GLsizei height,
                                GLenum format, GLenum type, const void *data, size_t data_size) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_DRAW_PIXELS;
    command.u.draw_pixels.width = width;
    command.u.draw_pixels.height = height;
    command.u.draw_pixels.format = format;
    command.u.draw_pixels.data_type = type;
    command.u.draw_pixels.data_size = data_size;
    return record_owned_payload(ctx, &command, data, data_size, &command.u.draw_pixels.data);
}

int mgl_list_record_bitmap(struct mgl_context *ctx, GLsizei width, GLsizei height, GLfloat xorig,
                           GLfloat yorig, GLfloat xmove, GLfloat ymove, const void *data,
                           size_t data_size) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_BITMAP;
    command.u.bitmap.width = width;
    command.u.bitmap.height = height;
    command.u.bitmap.xorig = xorig;
    command.u.bitmap.yorig = yorig;
    command.u.bitmap.xmove = xmove;
    command.u.bitmap.ymove = ymove;
    command.u.bitmap.data_size = data_size;
    return record_owned_payload(ctx, &command, data, data_size, &command.u.bitmap.data);
}

int mgl_list_record_copy_pixels(struct mgl_context *ctx, GLint x, GLint y, GLsizei width,
                                GLsizei height, GLenum type) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COPY_PIXELS;
    command.u.copy_pixels.x = x;
    command.u.copy_pixels.y = y;
    command.u.copy_pixels.width = width;
    command.u.copy_pixels.height = height;
    command.u.copy_pixels.copy_type = type;
    return record_command(ctx, &command);
}

int mgl_list_record_begin(struct mgl_context *ctx, GLenum mode) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_BEGIN;
    command.u.begin_mode = mode;
    return record_command(ctx, &command);
}

int mgl_list_record_vertex4f(struct mgl_context *ctx, GLfloat x, GLfloat y, GLfloat z, GLfloat w) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_VERTEX4F;
    command.u.vec4[0] = x;
    command.u.vec4[1] = y;
    command.u.vec4[2] = z;
    command.u.vec4[3] = w;
    return record_command(ctx, &command);
}

int mgl_list_record_end(struct mgl_context *ctx) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_END;
    return record_command(ctx, &command);
}

int mgl_list_record_bind_texture(struct mgl_context *ctx, GLenum target, GLuint texture) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_BIND_TEXTURE;
    command.u.bind_texture.target = target;
    command.u.bind_texture.texture = texture;
    return record_command(ctx, &command);
}

int mgl_list_record_bind_multi_texture(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                       GLuint texture) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_BIND_MULTI_TEXTURE;
    command.u.bind_multi_texture.texunit = texunit;
    command.u.bind_multi_texture.target = target;
    command.u.bind_multi_texture.texture = texture;
    return record_command(ctx, &command);
}

int mgl_list_record_multi_tex_buffer(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                     GLenum internalformat, GLuint buffer) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MULTI_TEX_BUFFER;
    command.u.multi_tex_buffer.texunit = texunit;
    command.u.multi_tex_buffer.target = target;
    command.u.multi_tex_buffer.internalformat = internalformat;
    command.u.multi_tex_buffer.buffer = buffer;
    return record_command(ctx, &command);
}

int mgl_list_record_texture_buffer_ext(struct mgl_context *ctx, GLuint texture, GLenum target,
                                       GLenum internalformat, GLuint buffer) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEXTURE_BUFFER_EXT;
    command.u.texture_buffer_ext.texture = texture;
    command.u.texture_buffer_ext.target = target;
    command.u.texture_buffer_ext.internalformat = internalformat;
    command.u.texture_buffer_ext.buffer = buffer;
    return record_command(ctx, &command);
}

int mgl_list_record_prioritize_textures(struct mgl_context *ctx, GLsizei n, const GLuint *textures,
                                        const GLfloat *priorities) {
    struct mgl_list_command command;
    size_t names_bytes;
    size_t priorities_bytes;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    if (n <= 0) {
        memset(&command, 0, sizeof(command));
        command.type = MGL_LIST_CMD_PRIORITIZE_TEXTURES;
        return record_command(ctx, &command);
    }
    if ((size_t)n > SIZE_MAX / sizeof(GLuint) || (size_t)n > SIZE_MAX / sizeof(GLfloat)) {
        return compile_storage_failure(ctx);
    }
    names_bytes = (size_t)n * sizeof(GLuint);
    priorities_bytes = (size_t)n * sizeof(GLfloat);
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_PRIORITIZE_TEXTURES;
    command.u.prioritize_textures.count = n;
    command.u.prioritize_textures.textures = malloc(names_bytes);
    command.u.prioritize_textures.priorities = malloc(priorities_bytes);
    if (command.u.prioritize_textures.textures == NULL ||
        command.u.prioritize_textures.priorities == NULL) {
        free(command.u.prioritize_textures.textures);
        free(command.u.prioritize_textures.priorities);
        return compile_storage_failure(ctx);
    }
    memcpy(command.u.prioritize_textures.textures, textures, names_bytes);
    memcpy(command.u.prioritize_textures.priorities, priorities, priorities_bytes);
    if (!append_command(ctx, &command)) {
        free(command.u.prioritize_textures.textures);
        free(command.u.prioritize_textures.priorities);
        return compile_storage_failure(ctx);
    }
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_tex_parameter(struct mgl_context *ctx, GLenum target, GLenum pname,
                                  GLint param) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEX_PARAMETER;
    command.u.tex_parameter.target = target;
    command.u.tex_parameter.pname = pname;
    command.u.tex_parameter.iparam[0] = param;
    command.u.tex_parameter.floating = GL_FALSE;
    return record_command(ctx, &command);
}

int mgl_list_record_tex_parameter_f(struct mgl_context *ctx, GLenum target, GLenum pname,
                                    GLfloat param) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEX_PARAMETER;
    command.u.tex_parameter.target = target;
    command.u.tex_parameter.pname = pname;
    command.u.tex_parameter.fparam[0] = param;
    command.u.tex_parameter.floating = GL_TRUE;
    return record_command(ctx, &command);
}

int mgl_list_record_tex_parameter_iv4(struct mgl_context *ctx, GLenum target, GLenum pname,
                                      const GLint *params) {
    struct mgl_list_command command;
    if (params == NULL)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEX_PARAMETER;
    command.u.tex_parameter.target = target;
    command.u.tex_parameter.pname = pname;
    memcpy(command.u.tex_parameter.iparam, params, sizeof(command.u.tex_parameter.iparam));
    command.u.tex_parameter.vector4 = GL_TRUE;
    return record_command(ctx, &command);
}

int mgl_list_record_tex_parameter_fv4(struct mgl_context *ctx, GLenum target, GLenum pname,
                                      const GLfloat *params) {
    struct mgl_list_command command;
    if (params == NULL)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEX_PARAMETER;
    command.u.tex_parameter.target = target;
    command.u.tex_parameter.pname = pname;
    memcpy(command.u.tex_parameter.fparam, params, sizeof(command.u.tex_parameter.fparam));
    command.u.tex_parameter.floating = GL_TRUE;
    command.u.tex_parameter.vector4 = GL_TRUE;
    return record_command(ctx, &command);
}

int mgl_list_record_tex_parameter_iiv(struct mgl_context *ctx, GLenum target, GLenum pname,
                                      const GLint *params) {
    struct mgl_list_command command;
    if (params == NULL)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEX_PARAMETER;
    command.u.tex_parameter.target = target;
    command.u.tex_parameter.pname = pname;
    memcpy(command.u.tex_parameter.iparam, params, sizeof(command.u.tex_parameter.iparam));
    command.u.tex_parameter.vector4 = pname == GL_TEXTURE_BORDER_COLOR ? GL_TRUE : GL_FALSE;
    command.u.tex_parameter.integer_typed = GL_TRUE;
    return record_command(ctx, &command);
}

int mgl_list_record_tex_parameter_iuiv(struct mgl_context *ctx, GLenum target, GLenum pname,
                                       const GLuint *params) {
    struct mgl_list_command command;
    if (params == NULL)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEX_PARAMETER;
    command.u.tex_parameter.target = target;
    command.u.tex_parameter.pname = pname;
    if (pname == GL_TEXTURE_BORDER_COLOR)
        memcpy(command.u.tex_parameter.uparam, params, sizeof(command.u.tex_parameter.uparam));
    else
        command.u.tex_parameter.uparam[0] = params[0];
    command.u.tex_parameter.vector4 = pname == GL_TEXTURE_BORDER_COLOR ? GL_TRUE : GL_FALSE;
    command.u.tex_parameter.integer_typed = GL_TRUE;
    command.u.tex_parameter.unsigned_integer = GL_TRUE;
    return record_command(ctx, &command);
}

int mgl_list_record_multi_tex_parameter(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                        GLenum pname, GLint param) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MULTI_TEX_PARAMETER;
    command.u.multi_tex_parameter.texunit = texunit;
    command.u.multi_tex_parameter.target = target;
    command.u.multi_tex_parameter.pname = pname;
    command.u.multi_tex_parameter.iparam[0] = param;
    command.u.multi_tex_parameter.floating = GL_FALSE;
    return record_command(ctx, &command);
}

int mgl_list_record_multi_tex_parameter_f(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                          GLenum pname, GLfloat param) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MULTI_TEX_PARAMETER;
    command.u.multi_tex_parameter.texunit = texunit;
    command.u.multi_tex_parameter.target = target;
    command.u.multi_tex_parameter.pname = pname;
    command.u.multi_tex_parameter.fparam[0] = param;
    command.u.multi_tex_parameter.floating = GL_TRUE;
    return record_command(ctx, &command);
}

int mgl_list_record_multi_tex_parameter_iv4(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                            GLenum pname, const GLint *params) {
    struct mgl_list_command command;
    if (params == NULL)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MULTI_TEX_PARAMETER;
    command.u.multi_tex_parameter.texunit = texunit;
    command.u.multi_tex_parameter.target = target;
    command.u.multi_tex_parameter.pname = pname;
    memcpy(command.u.multi_tex_parameter.iparam, params,
           sizeof(command.u.multi_tex_parameter.iparam));
    command.u.multi_tex_parameter.vector4 = GL_TRUE;
    return record_command(ctx, &command);
}

int mgl_list_record_multi_tex_parameter_fv4(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                            GLenum pname, const GLfloat *params) {
    struct mgl_list_command command;
    if (params == NULL)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MULTI_TEX_PARAMETER;
    command.u.multi_tex_parameter.texunit = texunit;
    command.u.multi_tex_parameter.target = target;
    command.u.multi_tex_parameter.pname = pname;
    memcpy(command.u.multi_tex_parameter.fparam, params,
           sizeof(command.u.multi_tex_parameter.fparam));
    command.u.multi_tex_parameter.floating = GL_TRUE;
    command.u.multi_tex_parameter.vector4 = GL_TRUE;
    return record_command(ctx, &command);
}

int mgl_list_record_multi_tex_parameter_iiv(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                            GLenum pname, const GLint *params) {
    struct mgl_list_command command;
    if (params == NULL)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MULTI_TEX_PARAMETER;
    command.u.multi_tex_parameter.texunit = texunit;
    command.u.multi_tex_parameter.target = target;
    command.u.multi_tex_parameter.pname = pname;
    memcpy(command.u.multi_tex_parameter.iparam, params,
           sizeof(command.u.multi_tex_parameter.iparam));
    command.u.multi_tex_parameter.vector4 = pname == GL_TEXTURE_BORDER_COLOR ? GL_TRUE : GL_FALSE;
    command.u.multi_tex_parameter.integer_typed = GL_TRUE;
    return record_command(ctx, &command);
}

int mgl_list_record_multi_tex_parameter_iuiv(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                             GLenum pname, const GLuint *params) {
    struct mgl_list_command command;
    if (params == NULL)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MULTI_TEX_PARAMETER;
    command.u.multi_tex_parameter.texunit = texunit;
    command.u.multi_tex_parameter.target = target;
    command.u.multi_tex_parameter.pname = pname;
    if (pname == GL_TEXTURE_BORDER_COLOR)
        memcpy(command.u.multi_tex_parameter.uparam, params,
               sizeof(command.u.multi_tex_parameter.uparam));
    else
        command.u.multi_tex_parameter.uparam[0] = params[0];
    command.u.multi_tex_parameter.vector4 = pname == GL_TEXTURE_BORDER_COLOR ? GL_TRUE : GL_FALSE;
    command.u.multi_tex_parameter.integer_typed = GL_TRUE;
    command.u.multi_tex_parameter.unsigned_integer = GL_TRUE;
    return record_command(ctx, &command);
}

int mgl_list_record_texture_parameter_ext(struct mgl_context *ctx, GLuint texture, GLenum target,
                                          GLenum pname, GLint param) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEXTURE_PARAMETER_EXT;
    command.u.texture_parameter_ext.texture = texture;
    command.u.texture_parameter_ext.target = target;
    command.u.texture_parameter_ext.pname = pname;
    command.u.texture_parameter_ext.iparam[0] = param;
    return record_command(ctx, &command);
}

int mgl_list_record_texture_parameter_ext_f(struct mgl_context *ctx, GLuint texture, GLenum target,
                                            GLenum pname, GLfloat param) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEXTURE_PARAMETER_EXT;
    command.u.texture_parameter_ext.texture = texture;
    command.u.texture_parameter_ext.target = target;
    command.u.texture_parameter_ext.pname = pname;
    command.u.texture_parameter_ext.fparam[0] = param;
    command.u.texture_parameter_ext.floating = GL_TRUE;
    return record_command(ctx, &command);
}

int mgl_list_record_texture_parameter_ext_iv4(struct mgl_context *ctx, GLuint texture,
                                              GLenum target, GLenum pname, const GLint *params) {
    struct mgl_list_command command;
    if (params == NULL)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEXTURE_PARAMETER_EXT;
    command.u.texture_parameter_ext.texture = texture;
    command.u.texture_parameter_ext.target = target;
    command.u.texture_parameter_ext.pname = pname;
    memcpy(command.u.texture_parameter_ext.iparam, params,
           sizeof(command.u.texture_parameter_ext.iparam));
    command.u.texture_parameter_ext.vector4 = GL_TRUE;
    return record_command(ctx, &command);
}

int mgl_list_record_texture_parameter_ext_fv4(struct mgl_context *ctx, GLuint texture,
                                              GLenum target, GLenum pname, const GLfloat *params) {
    struct mgl_list_command command;
    if (params == NULL)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEXTURE_PARAMETER_EXT;
    command.u.texture_parameter_ext.texture = texture;
    command.u.texture_parameter_ext.target = target;
    command.u.texture_parameter_ext.pname = pname;
    memcpy(command.u.texture_parameter_ext.fparam, params,
           sizeof(command.u.texture_parameter_ext.fparam));
    command.u.texture_parameter_ext.floating = GL_TRUE;
    command.u.texture_parameter_ext.vector4 = GL_TRUE;
    return record_command(ctx, &command);
}

int mgl_list_record_texture_parameter_ext_iiv(struct mgl_context *ctx, GLuint texture,
                                              GLenum target, GLenum pname, const GLint *params) {
    struct mgl_list_command command;
    if (params == NULL)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEXTURE_PARAMETER_EXT;
    command.u.texture_parameter_ext.texture = texture;
    command.u.texture_parameter_ext.target = target;
    command.u.texture_parameter_ext.pname = pname;
    memcpy(command.u.texture_parameter_ext.iparam, params,
           sizeof(command.u.texture_parameter_ext.iparam));
    command.u.texture_parameter_ext.vector4 = pname == GL_TEXTURE_BORDER_COLOR ? GL_TRUE : GL_FALSE;
    command.u.texture_parameter_ext.integer_typed = GL_TRUE;
    return record_command(ctx, &command);
}

int mgl_list_record_texture_parameter_ext_iuiv(struct mgl_context *ctx, GLuint texture,
                                               GLenum target, GLenum pname, const GLuint *params) {
    struct mgl_list_command command;
    if (params == NULL)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEXTURE_PARAMETER_EXT;
    command.u.texture_parameter_ext.texture = texture;
    command.u.texture_parameter_ext.target = target;
    command.u.texture_parameter_ext.pname = pname;
    if (pname == GL_TEXTURE_BORDER_COLOR)
        memcpy(command.u.texture_parameter_ext.uparam, params,
               sizeof(command.u.texture_parameter_ext.uparam));
    else
        command.u.texture_parameter_ext.uparam[0] = params[0];
    command.u.texture_parameter_ext.vector4 = pname == GL_TEXTURE_BORDER_COLOR ? GL_TRUE : GL_FALSE;
    command.u.texture_parameter_ext.integer_typed = GL_TRUE;
    command.u.texture_parameter_ext.unsigned_integer = GL_TRUE;
    return record_command(ctx, &command);
}

static int copy_payload(void **dst, const void *data, size_t data_size) {
    if (data_size == 0u) {
        *dst = NULL;
        return 1;
    }
    *dst = malloc(data_size);
    if (*dst == NULL)
        return 0;
    memcpy(*dst, data, data_size);
    return 1;
}

int mgl_list_record_multi_tex_image_2d(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                       GLint level, GLint internalformat, GLsizei width,
                                       GLsizei height, GLint border, GLenum format, GLenum type,
                                       const void *data, size_t data_size) {
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MULTI_TEX_IMAGE_2D;
    command.u.multi_tex_image_2d.texunit = texunit;
    command.u.multi_tex_image_2d.target = target;
    command.u.multi_tex_image_2d.level = level;
    command.u.multi_tex_image_2d.internalformat = internalformat;
    command.u.multi_tex_image_2d.width = width;
    command.u.multi_tex_image_2d.height = height;
    command.u.multi_tex_image_2d.border = border;
    command.u.multi_tex_image_2d.format = format;
    command.u.multi_tex_image_2d.data_type = type;
    command.u.multi_tex_image_2d.data_size = data_size;
    if (!copy_payload(&command.u.multi_tex_image_2d.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_multi_tex_sub_image_2d(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                           GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                                           GLsizei height, GLenum format, GLenum type,
                                           const void *data, size_t data_size) {
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MULTI_TEX_SUB_IMAGE_2D;
    command.u.multi_tex_sub_image_2d.texunit = texunit;
    command.u.multi_tex_sub_image_2d.target = target;
    command.u.multi_tex_sub_image_2d.level = level;
    command.u.multi_tex_sub_image_2d.xoffset = xoffset;
    command.u.multi_tex_sub_image_2d.yoffset = yoffset;
    command.u.multi_tex_sub_image_2d.width = width;
    command.u.multi_tex_sub_image_2d.height = height;
    command.u.multi_tex_sub_image_2d.format = format;
    command.u.multi_tex_sub_image_2d.data_type = type;
    command.u.multi_tex_sub_image_2d.data_size = data_size;
    if (!copy_payload(&command.u.multi_tex_sub_image_2d.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_multi_tex_image_3d(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                       GLint level, GLint internalformat, GLsizei width,
                                       GLsizei height, GLsizei depth, GLint border, GLenum format,
                                       GLenum type, const void *data, size_t data_size) {
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MULTI_TEX_IMAGE_3D;
    command.u.multi_tex_image_3d.texunit = texunit;
    command.u.multi_tex_image_3d.target = target;
    command.u.multi_tex_image_3d.level = level;
    command.u.multi_tex_image_3d.internalformat = internalformat;
    command.u.multi_tex_image_3d.width = width;
    command.u.multi_tex_image_3d.height = height;
    command.u.multi_tex_image_3d.depth = depth;
    command.u.multi_tex_image_3d.border = border;
    command.u.multi_tex_image_3d.format = format;
    command.u.multi_tex_image_3d.data_type = type;
    command.u.multi_tex_image_3d.data_size = data_size;
    if (!copy_payload(&command.u.multi_tex_image_3d.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_multi_tex_sub_image_3d(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                           GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                           GLsizei width, GLsizei height, GLsizei depth,
                                           GLenum format, GLenum type, const void *data,
                                           size_t data_size) {
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_MULTI_TEX_SUB_IMAGE_3D;
    command.u.multi_tex_sub_image_3d.texunit = texunit;
    command.u.multi_tex_sub_image_3d.target = target;
    command.u.multi_tex_sub_image_3d.level = level;
    command.u.multi_tex_sub_image_3d.xoffset = xoffset;
    command.u.multi_tex_sub_image_3d.yoffset = yoffset;
    command.u.multi_tex_sub_image_3d.zoffset = zoffset;
    command.u.multi_tex_sub_image_3d.width = width;
    command.u.multi_tex_sub_image_3d.height = height;
    command.u.multi_tex_sub_image_3d.depth = depth;
    command.u.multi_tex_sub_image_3d.format = format;
    command.u.multi_tex_sub_image_3d.data_type = type;
    command.u.multi_tex_sub_image_3d.data_size = data_size;
    if (!copy_payload(&command.u.multi_tex_sub_image_3d.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_compressed_multi_tex_image_2d(struct mgl_context *ctx, GLenum texunit,
                                                  GLenum target, GLint level, GLenum internalformat,
                                                  GLsizei width, GLsizei height, GLint border,
                                                  const void *data, size_t data_size) {
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COMPRESSED_MULTI_TEX_IMAGE_2D;
    command.u.compressed_multi_tex_image_2d.texunit = texunit;
    command.u.compressed_multi_tex_image_2d.target = target;
    command.u.compressed_multi_tex_image_2d.level = level;
    command.u.compressed_multi_tex_image_2d.internalformat = internalformat;
    command.u.compressed_multi_tex_image_2d.width = width;
    command.u.compressed_multi_tex_image_2d.height = height;
    command.u.compressed_multi_tex_image_2d.border = border;
    command.u.compressed_multi_tex_image_2d.data_size = data_size;
    if (!copy_payload(&command.u.compressed_multi_tex_image_2d.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_compressed_multi_tex_sub_image_2d(struct mgl_context *ctx, GLenum texunit,
                                                      GLenum target, GLint level, GLint xoffset,
                                                      GLint yoffset, GLsizei width, GLsizei height,
                                                      GLenum format, const void *data,
                                                      size_t data_size) {
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COMPRESSED_MULTI_TEX_SUB_IMAGE_2D;
    command.u.compressed_multi_tex_sub_image_2d.texunit = texunit;
    command.u.compressed_multi_tex_sub_image_2d.target = target;
    command.u.compressed_multi_tex_sub_image_2d.level = level;
    command.u.compressed_multi_tex_sub_image_2d.xoffset = xoffset;
    command.u.compressed_multi_tex_sub_image_2d.yoffset = yoffset;
    command.u.compressed_multi_tex_sub_image_2d.width = width;
    command.u.compressed_multi_tex_sub_image_2d.height = height;
    command.u.compressed_multi_tex_sub_image_2d.format = format;
    command.u.compressed_multi_tex_sub_image_2d.data_size = data_size;
    if (!copy_payload(&command.u.compressed_multi_tex_sub_image_2d.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_compressed_multi_tex_image_3d(struct mgl_context *ctx, GLenum texunit,
                                                  GLenum target, GLint level, GLenum internalformat,
                                                  GLsizei width, GLsizei height, GLsizei depth,
                                                  GLint border, const void *data,
                                                  size_t data_size) {
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COMPRESSED_MULTI_TEX_IMAGE_3D;
    command.u.compressed_multi_tex_image_3d.texunit = texunit;
    command.u.compressed_multi_tex_image_3d.target = target;
    command.u.compressed_multi_tex_image_3d.level = level;
    command.u.compressed_multi_tex_image_3d.internalformat = internalformat;
    command.u.compressed_multi_tex_image_3d.width = width;
    command.u.compressed_multi_tex_image_3d.height = height;
    command.u.compressed_multi_tex_image_3d.depth = depth;
    command.u.compressed_multi_tex_image_3d.border = border;
    command.u.compressed_multi_tex_image_3d.data_size = data_size;
    if (!copy_payload(&command.u.compressed_multi_tex_image_3d.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_compressed_multi_tex_sub_image_3d(struct mgl_context *ctx, GLenum texunit,
                                                      GLenum target, GLint level, GLint xoffset,
                                                      GLint yoffset, GLint zoffset, GLsizei width,
                                                      GLsizei height, GLsizei depth, GLenum format,
                                                      const void *data, size_t data_size) {
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COMPRESSED_MULTI_TEX_SUB_IMAGE_3D;
    command.u.compressed_multi_tex_sub_image_3d.texunit = texunit;
    command.u.compressed_multi_tex_sub_image_3d.target = target;
    command.u.compressed_multi_tex_sub_image_3d.level = level;
    command.u.compressed_multi_tex_sub_image_3d.xoffset = xoffset;
    command.u.compressed_multi_tex_sub_image_3d.yoffset = yoffset;
    command.u.compressed_multi_tex_sub_image_3d.zoffset = zoffset;
    command.u.compressed_multi_tex_sub_image_3d.width = width;
    command.u.compressed_multi_tex_sub_image_3d.height = height;
    command.u.compressed_multi_tex_sub_image_3d.depth = depth;
    command.u.compressed_multi_tex_sub_image_3d.format = format;
    command.u.compressed_multi_tex_sub_image_3d.data_size = data_size;
    if (!copy_payload(&command.u.compressed_multi_tex_sub_image_3d.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_tex_image_2d(struct mgl_context *ctx, GLenum target, GLint level,
                                 GLint internalformat, GLsizei width, GLsizei height, GLint border,
                                 GLenum format, GLenum type, const void *data, size_t data_size) {
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEX_IMAGE_2D;
    command.u.tex_image_2d.target = target;
    command.u.tex_image_2d.level = level;
    command.u.tex_image_2d.internalformat = internalformat;
    command.u.tex_image_2d.width = width;
    command.u.tex_image_2d.height = height;
    command.u.tex_image_2d.border = border;
    command.u.tex_image_2d.format = format;
    command.u.tex_image_2d.data_type = type;
    command.u.tex_image_2d.data_size = data_size;
    if (!copy_payload(&command.u.tex_image_2d.data, data, data_size)) {
        return compile_storage_failure(ctx);
    }
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_tex_sub_image_2d(struct mgl_context *ctx, GLenum target, GLint level,
                                     GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                                     GLenum format, GLenum type, const void *data,
                                     size_t data_size) {
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEX_SUB_IMAGE_2D;
    command.u.tex_sub_image_2d.target = target;
    command.u.tex_sub_image_2d.level = level;
    command.u.tex_sub_image_2d.xoffset = xoffset;
    command.u.tex_sub_image_2d.yoffset = yoffset;
    command.u.tex_sub_image_2d.width = width;
    command.u.tex_sub_image_2d.height = height;
    command.u.tex_sub_image_2d.format = format;
    command.u.tex_sub_image_2d.data_type = type;
    command.u.tex_sub_image_2d.data_size = data_size;
    if (!copy_payload(&command.u.tex_sub_image_2d.data, data, data_size)) {
        return compile_storage_failure(ctx);
    }
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_tex_image_3d(struct mgl_context *ctx, GLenum target, GLint level,
                                 GLint internalformat, GLsizei width, GLsizei height, GLsizei depth,
                                 GLint border, GLenum format, GLenum type, const void *data,
                                 size_t data_size) {
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEX_IMAGE_3D;
    command.u.tex_image_3d.target = target;
    command.u.tex_image_3d.level = level;
    command.u.tex_image_3d.internalformat = internalformat;
    command.u.tex_image_3d.width = width;
    command.u.tex_image_3d.height = height;
    command.u.tex_image_3d.depth = depth;
    command.u.tex_image_3d.border = border;
    command.u.tex_image_3d.format = format;
    command.u.tex_image_3d.data_type = type;
    command.u.tex_image_3d.data_size = data_size;
    if (!copy_payload(&command.u.tex_image_3d.data, data, data_size)) {
        return compile_storage_failure(ctx);
    }
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_tex_sub_image_3d(struct mgl_context *ctx, GLenum target, GLint level,
                                     GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width,
                                     GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                     const void *data, size_t data_size) {
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEX_SUB_IMAGE_3D;
    command.u.tex_sub_image_3d.target = target;
    command.u.tex_sub_image_3d.level = level;
    command.u.tex_sub_image_3d.xoffset = xoffset;
    command.u.tex_sub_image_3d.yoffset = yoffset;
    command.u.tex_sub_image_3d.zoffset = zoffset;
    command.u.tex_sub_image_3d.width = width;
    command.u.tex_sub_image_3d.height = height;
    command.u.tex_sub_image_3d.depth = depth;
    command.u.tex_sub_image_3d.format = format;
    command.u.tex_sub_image_3d.data_type = type;
    command.u.tex_sub_image_3d.data_size = data_size;
    if (!copy_payload(&command.u.tex_sub_image_3d.data, data, data_size)) {
        return compile_storage_failure(ctx);
    }
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_compressed_tex_image_2d(struct mgl_context *ctx, GLenum target, GLint level,
                                            GLenum internalformat, GLsizei width, GLsizei height,
                                            GLint border, const void *data, size_t data_size) {
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COMPRESSED_TEX_IMAGE_2D;
    command.u.compressed_tex_image_2d.target = target;
    command.u.compressed_tex_image_2d.level = level;
    command.u.compressed_tex_image_2d.internalformat = internalformat;
    command.u.compressed_tex_image_2d.width = width;
    command.u.compressed_tex_image_2d.height = height;
    command.u.compressed_tex_image_2d.border = border;
    command.u.compressed_tex_image_2d.data_size = data_size;
    if (!copy_payload(&command.u.compressed_tex_image_2d.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_compressed_tex_image_3d(struct mgl_context *ctx, GLenum target, GLint level,
                                            GLenum internalformat, GLsizei width, GLsizei height,
                                            GLsizei depth, GLint border, const void *data,
                                            size_t data_size) {
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COMPRESSED_TEX_IMAGE_3D;
    command.u.compressed_tex_image_3d.target = target;
    command.u.compressed_tex_image_3d.level = level;
    command.u.compressed_tex_image_3d.internalformat = internalformat;
    command.u.compressed_tex_image_3d.width = width;
    command.u.compressed_tex_image_3d.height = height;
    command.u.compressed_tex_image_3d.depth = depth;
    command.u.compressed_tex_image_3d.border = border;
    command.u.compressed_tex_image_3d.data_size = data_size;
    if (!copy_payload(&command.u.compressed_tex_image_3d.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_compressed_tex_sub_image_2d(struct mgl_context *ctx, GLenum target, GLint level,
                                                GLint xoffset, GLint yoffset, GLsizei width,
                                                GLsizei height, GLenum format, const void *data,
                                                size_t data_size) {
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COMPRESSED_TEX_SUB_IMAGE_2D;
    command.u.compressed_tex_sub_image_2d.target = target;
    command.u.compressed_tex_sub_image_2d.level = level;
    command.u.compressed_tex_sub_image_2d.xoffset = xoffset;
    command.u.compressed_tex_sub_image_2d.yoffset = yoffset;
    command.u.compressed_tex_sub_image_2d.width = width;
    command.u.compressed_tex_sub_image_2d.height = height;
    command.u.compressed_tex_sub_image_2d.format = format;
    command.u.compressed_tex_sub_image_2d.data_size = data_size;
    if (!copy_payload(&command.u.compressed_tex_sub_image_2d.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_compressed_tex_sub_image_3d(struct mgl_context *ctx, GLenum target, GLint level,
                                                GLint xoffset, GLint yoffset, GLint zoffset,
                                                GLsizei width, GLsizei height, GLsizei depth,
                                                GLenum format, const void *data, size_t data_size) {
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COMPRESSED_TEX_SUB_IMAGE_3D;
    command.u.compressed_tex_sub_image_3d.target = target;
    command.u.compressed_tex_sub_image_3d.level = level;
    command.u.compressed_tex_sub_image_3d.xoffset = xoffset;
    command.u.compressed_tex_sub_image_3d.yoffset = yoffset;
    command.u.compressed_tex_sub_image_3d.zoffset = zoffset;
    command.u.compressed_tex_sub_image_3d.width = width;
    command.u.compressed_tex_sub_image_3d.height = height;
    command.u.compressed_tex_sub_image_3d.depth = depth;
    command.u.compressed_tex_sub_image_3d.format = format;
    command.u.compressed_tex_sub_image_3d.data_size = data_size;
    if (!copy_payload(&command.u.compressed_tex_sub_image_3d.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_copy_tex_image_2d(struct mgl_context *ctx, GLenum target, GLint level,
                                      GLenum internalformat, GLint x, GLint y, GLsizei width,
                                      GLsizei height, GLint border) {
    struct mgl_list_command command;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COPY_TEX_IMAGE_2D;
    command.u.copy_tex_image_2d.target = target;
    command.u.copy_tex_image_2d.level = level;
    command.u.copy_tex_image_2d.internalformat = internalformat;
    command.u.copy_tex_image_2d.x = x;
    command.u.copy_tex_image_2d.y = y;
    command.u.copy_tex_image_2d.width = width;
    command.u.copy_tex_image_2d.height = height;
    command.u.copy_tex_image_2d.border = border;
    if (!append_command(ctx, &command))
        return compile_storage_failure(ctx);
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_copy_tex_sub_image_2d(struct mgl_context *ctx, GLenum target, GLint level,
                                          GLint xoffset, GLint yoffset, GLint x, GLint y,
                                          GLsizei width, GLsizei height) {
    struct mgl_list_command command;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COPY_TEX_SUB_IMAGE_2D;
    command.u.copy_tex_sub_image_2d.target = target;
    command.u.copy_tex_sub_image_2d.level = level;
    command.u.copy_tex_sub_image_2d.xoffset = xoffset;
    command.u.copy_tex_sub_image_2d.yoffset = yoffset;
    command.u.copy_tex_sub_image_2d.x = x;
    command.u.copy_tex_sub_image_2d.y = y;
    command.u.copy_tex_sub_image_2d.width = width;
    command.u.copy_tex_sub_image_2d.height = height;
    if (!append_command(ctx, &command))
        return compile_storage_failure(ctx);
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_copy_tex_sub_image_3d(struct mgl_context *ctx, GLenum target, GLint level,
                                          GLint xoffset, GLint yoffset, GLint zoffset, GLint x,
                                          GLint y, GLsizei width, GLsizei height) {
    struct mgl_list_command command;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COPY_TEX_SUB_IMAGE_3D;
    command.u.copy_tex_sub_image_3d.target = target;
    command.u.copy_tex_sub_image_3d.level = level;
    command.u.copy_tex_sub_image_3d.xoffset = xoffset;
    command.u.copy_tex_sub_image_3d.yoffset = yoffset;
    command.u.copy_tex_sub_image_3d.zoffset = zoffset;
    command.u.copy_tex_sub_image_3d.x = x;
    command.u.copy_tex_sub_image_3d.y = y;
    command.u.copy_tex_sub_image_3d.width = width;
    command.u.copy_tex_sub_image_3d.height = height;
    if (!append_command(ctx, &command))
        return compile_storage_failure(ctx);
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_compressed_texture_image_2d_ext(struct mgl_context *ctx, GLuint texture,
                                                    GLenum target, GLint level,
                                                    GLenum internalformat, GLsizei width,
                                                    GLsizei height, GLint border, const void *data,
                                                    size_t data_size) {
    struct mgl_list_command command;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COMPRESSED_TEXTURE_IMAGE_2D_EXT;
    command.u.compressed_texture_image_2d_ext.texture = texture;
    command.u.compressed_texture_image_2d_ext.target = target;
    command.u.compressed_texture_image_2d_ext.level = level;
    command.u.compressed_texture_image_2d_ext.internalformat = internalformat;
    command.u.compressed_texture_image_2d_ext.width = width;
    command.u.compressed_texture_image_2d_ext.height = height;
    command.u.compressed_texture_image_2d_ext.border = border;
    command.u.compressed_texture_image_2d_ext.data_size = data_size;
    if (!copy_payload(&command.u.compressed_texture_image_2d_ext.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_compressed_texture_sub_image_2d_ext(struct mgl_context *ctx, GLuint texture,
                                                        GLenum target, GLint level, GLint xoffset,
                                                        GLint yoffset, GLsizei width,
                                                        GLsizei height, GLenum format,
                                                        const void *data, size_t data_size) {
    struct mgl_list_command command;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COMPRESSED_TEXTURE_SUB_IMAGE_2D_EXT;
    command.u.compressed_texture_sub_image_2d_ext.texture = texture;
    command.u.compressed_texture_sub_image_2d_ext.target = target;
    command.u.compressed_texture_sub_image_2d_ext.level = level;
    command.u.compressed_texture_sub_image_2d_ext.xoffset = xoffset;
    command.u.compressed_texture_sub_image_2d_ext.yoffset = yoffset;
    command.u.compressed_texture_sub_image_2d_ext.width = width;
    command.u.compressed_texture_sub_image_2d_ext.height = height;
    command.u.compressed_texture_sub_image_2d_ext.format = format;
    command.u.compressed_texture_sub_image_2d_ext.data_size = data_size;
    if (!copy_payload(&command.u.compressed_texture_sub_image_2d_ext.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_compressed_texture_image_3d_ext(struct mgl_context *ctx, GLuint texture,
                                                    GLenum target, GLint level,
                                                    GLenum internalformat, GLsizei width,
                                                    GLsizei height, GLsizei depth, GLint border,
                                                    const void *data, size_t data_size) {
    struct mgl_list_command command;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COMPRESSED_TEXTURE_IMAGE_3D_EXT;
    command.u.compressed_texture_image_3d_ext.texture = texture;
    command.u.compressed_texture_image_3d_ext.target = target;
    command.u.compressed_texture_image_3d_ext.level = level;
    command.u.compressed_texture_image_3d_ext.internalformat = internalformat;
    command.u.compressed_texture_image_3d_ext.width = width;
    command.u.compressed_texture_image_3d_ext.height = height;
    command.u.compressed_texture_image_3d_ext.depth = depth;
    command.u.compressed_texture_image_3d_ext.border = border;
    command.u.compressed_texture_image_3d_ext.data_size = data_size;
    if (!copy_payload(&command.u.compressed_texture_image_3d_ext.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_compressed_texture_sub_image_3d_ext(struct mgl_context *ctx, GLuint texture,
                                                        GLenum target, GLint level, GLint xoffset,
                                                        GLint yoffset, GLint zoffset, GLsizei width,
                                                        GLsizei height, GLsizei depth,
                                                        GLenum format, const void *data,
                                                        size_t data_size) {
    struct mgl_list_command command;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COMPRESSED_TEXTURE_SUB_IMAGE_3D_EXT;
    command.u.compressed_texture_sub_image_3d_ext.texture = texture;
    command.u.compressed_texture_sub_image_3d_ext.target = target;
    command.u.compressed_texture_sub_image_3d_ext.level = level;
    command.u.compressed_texture_sub_image_3d_ext.xoffset = xoffset;
    command.u.compressed_texture_sub_image_3d_ext.yoffset = yoffset;
    command.u.compressed_texture_sub_image_3d_ext.zoffset = zoffset;
    command.u.compressed_texture_sub_image_3d_ext.width = width;
    command.u.compressed_texture_sub_image_3d_ext.height = height;
    command.u.compressed_texture_sub_image_3d_ext.depth = depth;
    command.u.compressed_texture_sub_image_3d_ext.format = format;
    command.u.compressed_texture_sub_image_3d_ext.data_size = data_size;
    if (!copy_payload(&command.u.compressed_texture_sub_image_3d_ext.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_texture_image_2d_ext(struct mgl_context *ctx, GLuint texture, GLenum target,
                                         GLint level, GLint internalformat, GLsizei width,
                                         GLsizei height, GLint border, GLenum format, GLenum type,
                                         const void *data, size_t data_size) {
    struct mgl_list_command command;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEXTURE_IMAGE_2D_EXT;
    command.u.texture_image_2d_ext.texture = texture;
    command.u.texture_image_2d_ext.target = target;
    command.u.texture_image_2d_ext.level = level;
    command.u.texture_image_2d_ext.internalformat = internalformat;
    command.u.texture_image_2d_ext.width = width;
    command.u.texture_image_2d_ext.height = height;
    command.u.texture_image_2d_ext.border = border;
    command.u.texture_image_2d_ext.format = format;
    command.u.texture_image_2d_ext.data_type = type;
    command.u.texture_image_2d_ext.data_size = data_size;
    if (!copy_payload(&command.u.texture_image_2d_ext.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_texture_sub_image_2d_ext(struct mgl_context *ctx, GLuint texture, GLenum target,
                                             GLint level, GLint xoffset, GLint yoffset,
                                             GLsizei width, GLsizei height, GLenum format,
                                             GLenum type, const void *data, size_t data_size) {
    struct mgl_list_command command;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEXTURE_SUB_IMAGE_2D_EXT;
    command.u.texture_sub_image_2d_ext.texture = texture;
    command.u.texture_sub_image_2d_ext.target = target;
    command.u.texture_sub_image_2d_ext.level = level;
    command.u.texture_sub_image_2d_ext.xoffset = xoffset;
    command.u.texture_sub_image_2d_ext.yoffset = yoffset;
    command.u.texture_sub_image_2d_ext.width = width;
    command.u.texture_sub_image_2d_ext.height = height;
    command.u.texture_sub_image_2d_ext.format = format;
    command.u.texture_sub_image_2d_ext.data_type = type;
    command.u.texture_sub_image_2d_ext.data_size = data_size;
    if (!copy_payload(&command.u.texture_sub_image_2d_ext.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_texture_image_3d_ext(struct mgl_context *ctx, GLuint texture, GLenum target,
                                         GLint level, GLint internalformat, GLsizei width,
                                         GLsizei height, GLsizei depth, GLint border, GLenum format,
                                         GLenum type, const void *data, size_t data_size) {
    struct mgl_list_command command;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEXTURE_IMAGE_3D_EXT;
    command.u.texture_image_3d_ext.texture = texture;
    command.u.texture_image_3d_ext.target = target;
    command.u.texture_image_3d_ext.level = level;
    command.u.texture_image_3d_ext.internalformat = internalformat;
    command.u.texture_image_3d_ext.width = width;
    command.u.texture_image_3d_ext.height = height;
    command.u.texture_image_3d_ext.depth = depth;
    command.u.texture_image_3d_ext.border = border;
    command.u.texture_image_3d_ext.format = format;
    command.u.texture_image_3d_ext.data_type = type;
    command.u.texture_image_3d_ext.data_size = data_size;
    if (!copy_payload(&command.u.texture_image_3d_ext.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_texture_sub_image_3d_ext(struct mgl_context *ctx, GLuint texture, GLenum target,
                                             GLint level, GLint xoffset, GLint yoffset,
                                             GLint zoffset, GLsizei width, GLsizei height,
                                             GLsizei depth, GLenum format, GLenum type,
                                             const void *data, size_t data_size) {
    struct mgl_list_command command;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_TEXTURE_SUB_IMAGE_3D_EXT;
    command.u.texture_sub_image_3d_ext.texture = texture;
    command.u.texture_sub_image_3d_ext.target = target;
    command.u.texture_sub_image_3d_ext.level = level;
    command.u.texture_sub_image_3d_ext.xoffset = xoffset;
    command.u.texture_sub_image_3d_ext.yoffset = yoffset;
    command.u.texture_sub_image_3d_ext.zoffset = zoffset;
    command.u.texture_sub_image_3d_ext.width = width;
    command.u.texture_sub_image_3d_ext.height = height;
    command.u.texture_sub_image_3d_ext.depth = depth;
    command.u.texture_sub_image_3d_ext.format = format;
    command.u.texture_sub_image_3d_ext.data_type = type;
    command.u.texture_sub_image_3d_ext.data_size = data_size;
    if (!copy_payload(&command.u.texture_sub_image_3d_ext.data, data, data_size))
        return compile_storage_failure(ctx);
    if (!append_command(ctx, &command)) {
        destroy_command(&command);
        return compile_storage_failure(ctx);
    }
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_copy_texture_image_2d_ext(struct mgl_context *ctx, GLuint texture,
                                              GLenum target, GLint level, GLenum internalformat,
                                              GLint x, GLint y, GLsizei width, GLsizei height,
                                              GLint border) {
    struct mgl_list_command command;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COPY_TEXTURE_IMAGE_2D_EXT;
    command.u.copy_texture_image_2d_ext.texture = texture;
    command.u.copy_texture_image_2d_ext.target = target;
    command.u.copy_texture_image_2d_ext.level = level;
    command.u.copy_texture_image_2d_ext.internalformat = internalformat;
    command.u.copy_texture_image_2d_ext.x = x;
    command.u.copy_texture_image_2d_ext.y = y;
    command.u.copy_texture_image_2d_ext.width = width;
    command.u.copy_texture_image_2d_ext.height = height;
    command.u.copy_texture_image_2d_ext.border = border;
    if (!append_command(ctx, &command))
        return compile_storage_failure(ctx);
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_copy_texture_sub_image_2d_ext(struct mgl_context *ctx, GLuint texture,
                                                  GLenum target, GLint level, GLint xoffset,
                                                  GLint yoffset, GLint x, GLint y, GLsizei width,
                                                  GLsizei height) {
    struct mgl_list_command command;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COPY_TEXTURE_SUB_IMAGE_2D_EXT;
    command.u.copy_texture_sub_image_2d_ext.texture = texture;
    command.u.copy_texture_sub_image_2d_ext.target = target;
    command.u.copy_texture_sub_image_2d_ext.level = level;
    command.u.copy_texture_sub_image_2d_ext.xoffset = xoffset;
    command.u.copy_texture_sub_image_2d_ext.yoffset = yoffset;
    command.u.copy_texture_sub_image_2d_ext.x = x;
    command.u.copy_texture_sub_image_2d_ext.y = y;
    command.u.copy_texture_sub_image_2d_ext.width = width;
    command.u.copy_texture_sub_image_2d_ext.height = height;
    if (!append_command(ctx, &command))
        return compile_storage_failure(ctx);
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_copy_texture_sub_image_3d_ext(struct mgl_context *ctx, GLuint texture,
                                                  GLenum target, GLint level, GLint xoffset,
                                                  GLint yoffset, GLint zoffset, GLint x, GLint y,
                                                  GLsizei width, GLsizei height) {
    struct mgl_list_command command;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COPY_TEXTURE_SUB_IMAGE_3D_EXT;
    command.u.copy_texture_sub_image_3d_ext.texture = texture;
    command.u.copy_texture_sub_image_3d_ext.target = target;
    command.u.copy_texture_sub_image_3d_ext.level = level;
    command.u.copy_texture_sub_image_3d_ext.xoffset = xoffset;
    command.u.copy_texture_sub_image_3d_ext.yoffset = yoffset;
    command.u.copy_texture_sub_image_3d_ext.zoffset = zoffset;
    command.u.copy_texture_sub_image_3d_ext.x = x;
    command.u.copy_texture_sub_image_3d_ext.y = y;
    command.u.copy_texture_sub_image_3d_ext.width = width;
    command.u.copy_texture_sub_image_3d_ext.height = height;
    if (!append_command(ctx, &command))
        return compile_storage_failure(ctx);
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_copy_multi_tex_image_2d(struct mgl_context *ctx, GLenum texunit, GLenum target,
                                            GLint level, GLenum internalformat, GLint x, GLint y,
                                            GLsizei width, GLsizei height, GLint border) {
    struct mgl_list_command command;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COPY_MULTI_TEX_IMAGE_2D;
    command.u.copy_multi_tex_image_2d.texunit = texunit;
    command.u.copy_multi_tex_image_2d.target = target;
    command.u.copy_multi_tex_image_2d.level = level;
    command.u.copy_multi_tex_image_2d.internalformat = internalformat;
    command.u.copy_multi_tex_image_2d.x = x;
    command.u.copy_multi_tex_image_2d.y = y;
    command.u.copy_multi_tex_image_2d.width = width;
    command.u.copy_multi_tex_image_2d.height = height;
    command.u.copy_multi_tex_image_2d.border = border;
    if (!append_command(ctx, &command))
        return compile_storage_failure(ctx);
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_copy_multi_tex_sub_image_2d(struct mgl_context *ctx, GLenum texunit,
                                                GLenum target, GLint level, GLint xoffset,
                                                GLint yoffset, GLint x, GLint y, GLsizei width,
                                                GLsizei height) {
    struct mgl_list_command command;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COPY_MULTI_TEX_SUB_IMAGE_2D;
    command.u.copy_multi_tex_sub_image_2d.texunit = texunit;
    command.u.copy_multi_tex_sub_image_2d.target = target;
    command.u.copy_multi_tex_sub_image_2d.level = level;
    command.u.copy_multi_tex_sub_image_2d.xoffset = xoffset;
    command.u.copy_multi_tex_sub_image_2d.yoffset = yoffset;
    command.u.copy_multi_tex_sub_image_2d.x = x;
    command.u.copy_multi_tex_sub_image_2d.y = y;
    command.u.copy_multi_tex_sub_image_2d.width = width;
    command.u.copy_multi_tex_sub_image_2d.height = height;
    if (!append_command(ctx, &command))
        return compile_storage_failure(ctx);
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_copy_multi_tex_sub_image_3d(struct mgl_context *ctx, GLenum texunit,
                                                GLenum target, GLint level, GLint xoffset,
                                                GLint yoffset, GLint zoffset, GLint x, GLint y,
                                                GLsizei width, GLsizei height) {
    struct mgl_list_command command;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying)
        return 0;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_COPY_MULTI_TEX_SUB_IMAGE_3D;
    command.u.copy_multi_tex_sub_image_3d.texunit = texunit;
    command.u.copy_multi_tex_sub_image_3d.target = target;
    command.u.copy_multi_tex_sub_image_3d.level = level;
    command.u.copy_multi_tex_sub_image_3d.xoffset = xoffset;
    command.u.copy_multi_tex_sub_image_3d.yoffset = yoffset;
    command.u.copy_multi_tex_sub_image_3d.zoffset = zoffset;
    command.u.copy_multi_tex_sub_image_3d.x = x;
    command.u.copy_multi_tex_sub_image_3d.y = y;
    command.u.copy_multi_tex_sub_image_3d.width = width;
    command.u.copy_multi_tex_sub_image_3d.height = height;
    if (!append_command(ctx, &command))
        return compile_storage_failure(ctx);
    return ctx->list_compile_mode == GL_COMPILE;
}

int mgl_list_record_draw_snapshot(struct mgl_context *ctx, struct mgl_draw_snapshot *snapshot) {
    struct mgl_list_command command;
    int suppress;
    if (snapshot == NULL)
        return 0;
    if (ctx == NULL || ctx->compiling_list == NULL || ctx->list_replaying) {
        mgl_draw_snapshot_destroy(snapshot);
        return 0;
    }
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_DRAW_SNAPSHOT;
    command.u.draw_snapshot = snapshot;
    if (!append_command(ctx, &command)) {
        mgl_draw_snapshot_destroy(snapshot);
        return compile_storage_failure(ctx);
    }
    suppress = ctx->list_compile_mode == GL_COMPILE;
    return suppress;
}

int mgl_list_record_init_names(struct mgl_context *ctx) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_INIT_NAMES;
    return record_command(ctx, &command);
}

int mgl_list_record_push_name(struct mgl_context *ctx, GLuint name) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_PUSH_NAME;
    command.u.name_value = name;
    return record_command(ctx, &command);
}

int mgl_list_record_pop_name(struct mgl_context *ctx) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_POP_NAME;
    return record_command(ctx, &command);
}

int mgl_list_record_load_name(struct mgl_context *ctx, GLuint name) {
    struct mgl_list_command command;
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_LOAD_NAME;
    command.u.name_value = name;
    return record_command(ctx, &command);
}

static void replay_list(struct mgl_context *ctx, GLuint name) {
    struct mgl_display_list *list;
    size_t i;
    if (ctx == NULL || name == 0u)
        return;
    if (ctx->list_replay_depth >= 64u) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    list = acquire_list(ctx->share_group, name);
    if (list == NULL)
        return;
    ++ctx->list_replay_depth;
    ctx->list_replaying = GL_TRUE;
    for (i = 0u; i < list->count; ++i) {
        const struct mgl_list_command *command = &list->commands[i];
        if (command->type == MGL_LIST_CMD_ACTIVE_TEXTURE) {
            glActiveTexture(command->u.enum_value);
        } else if (command->type == MGL_LIST_CMD_MATRIX_MODE) {
            glMatrixMode(command->u.enum_value);
        } else if (command->type == MGL_LIST_CMD_LOAD_IDENTITY) {
            glLoadIdentity();
        } else if (command->type == MGL_LIST_CMD_LOAD_MATRIXF) {
            glLoadMatrixf(command->u.matrix);
        } else if (command->type == MGL_LIST_CMD_MULT_MATRIXF) {
            glMultMatrixf(command->u.matrix);
        } else if (command->type == MGL_LIST_CMD_PUSH_MATRIX) {
            glPushMatrix();
        } else if (command->type == MGL_LIST_CMD_POP_MATRIX) {
            glPopMatrix();
        } else if (command->type == MGL_LIST_CMD_MATRIX_LOAD_EXT) {
            glMatrixLoadfEXT(command->u.matrix_ext.matrix_mode, command->u.matrix_ext.matrix);
        } else if (command->type == MGL_LIST_CMD_MATRIX_MULT_EXT) {
            glMatrixMultfEXT(command->u.matrix_ext.matrix_mode, command->u.matrix_ext.matrix);
        } else if (command->type == MGL_LIST_CMD_MATRIX_LOAD_IDENTITY_EXT) {
            glMatrixLoadIdentityEXT(command->u.enum_value);
        } else if (command->type == MGL_LIST_CMD_MATRIX_PUSH_EXT) {
            glMatrixPushEXT(command->u.enum_value);
        } else if (command->type == MGL_LIST_CMD_MATRIX_POP_EXT) {
            glMatrixPopEXT(command->u.enum_value);
        } else if (command->type == MGL_LIST_CMD_COLOR4F) {
            glColor4f(command->u.color[0], command->u.color[1], command->u.color[2],
                      command->u.color[3]);
        } else if (command->type == MGL_LIST_CMD_SECONDARY3F) {
            glSecondaryColor3f(command->u.vec4[0], command->u.vec4[1], command->u.vec4[2]);
        } else if (command->type == MGL_LIST_CMD_NORMAL3F) {
            glNormal3f(command->u.vec4[0], command->u.vec4[1], command->u.vec4[2]);
        } else if (command->type == MGL_LIST_CMD_TEXCOORD4F) {
            glMultiTexCoord4f(GL_TEXTURE0 + command->u.texcoord.unit, command->u.texcoord.value[0],
                              command->u.texcoord.value[1], command->u.texcoord.value[2],
                              command->u.texcoord.value[3]);
        } else if (command->type == MGL_LIST_CMD_FOG_COORD) {
            glFogCoordf(command->u.vec4[0]);
        } else if (command->type == MGL_LIST_CMD_ENABLE) {
            glEnable(command->u.cap);
        } else if (command->type == MGL_LIST_CMD_DISABLE) {
            glDisable(command->u.cap);
        } else if (command->type == MGL_LIST_CMD_PUSH_ATTRIB) {
            glPushAttrib((GLbitfield)command->u.enum_value);
        } else if (command->type == MGL_LIST_CMD_POP_ATTRIB) {
            glPopAttrib();
        } else if (command->type == MGL_LIST_CMD_PIXEL_TRANSFER) {
            glPixelTransferf(command->u.pixel_transfer.pname, command->u.pixel_transfer.param);
        } else if (command->type == MGL_LIST_CMD_PIXEL_MAP) {
            if (ctx->immediate.active)
                mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
            else
                (void)mgl_pixel_map_set_canonical(ctx, command->u.pixel_map.map,
                                                  command->u.pixel_map.size,
                                                  command->u.pixel_map.values);
        } else if (command->type == MGL_LIST_CMD_ALPHA_FUNC) {
            glAlphaFunc(command->u.alpha_func.func, command->u.alpha_func.ref);
        } else if (command->type == MGL_LIST_CMD_CLEAR_INDEX) {
            glClearIndex(command->u.clear_index);
        } else if (command->type == MGL_LIST_CMD_CLEAR_COLOR) {
            glClearColor(command->u.clear_color[0], command->u.clear_color[1],
                         command->u.clear_color[2], command->u.clear_color[3]);
        } else if (command->type == MGL_LIST_CMD_CLEAR_DEPTH) {
            glClearDepthf(command->u.clear_depth);
        } else if (command->type == MGL_LIST_CMD_CLEAR_STENCIL) {
            glClearStencil(command->u.clear_stencil);
        } else if (command->type == MGL_LIST_CMD_CLEAR) {
            glClear(command->u.clear_mask);
        } else if (command->type == MGL_LIST_CMD_VIEWPORT) {
            glViewport(command->u.viewport.x, command->u.viewport.y, command->u.viewport.width,
                       command->u.viewport.height);
        } else if (command->type == MGL_LIST_CMD_FOG) {
            if (command->u.fog.count == 4u)
                glFogfv(command->u.fog.pname, command->u.fog.params);
            else
                glFogf(command->u.fog.pname, command->u.fog.params[0]);
        } else if (command->type == MGL_LIST_CMD_LIGHT) {
            glLightfv(command->u.light.light, command->u.light.pname, command->u.light.params);
        } else if (command->type == MGL_LIST_CMD_LIGHT_MODEL) {
            glLightModelfv(command->u.light_model.pname, command->u.light_model.params);
        } else if (command->type == MGL_LIST_CMD_MATERIAL) {
            glMaterialfv(command->u.material.face, command->u.material.pname,
                         command->u.material.params);
        } else if (command->type == MGL_LIST_CMD_COLOR_MATERIAL) {
            glColorMaterial(command->u.color_material.face, command->u.color_material.mode);
        } else if (command->type == MGL_LIST_CMD_SHADE_MODEL) {
            glShadeModel(command->u.shade_model.mode);
        } else if (command->type == MGL_LIST_CMD_TEX_ENV) {
            glTexEnvfv(command->u.tex_env.target, command->u.tex_env.pname,
                       command->u.tex_env.params);
        } else if (command->type == MGL_LIST_CMD_MULTI_TEX_ENV_EXT) {
            glMultiTexEnvfvEXT(
                command->u.multi_tex_env_ext.texunit, command->u.multi_tex_env_ext.target,
                command->u.multi_tex_env_ext.pname, command->u.multi_tex_env_ext.params);
        } else if (command->type == MGL_LIST_CMD_TEXGEN) {
            glTexGenfv(command->u.texgen.coord, command->u.texgen.pname, command->u.texgen.params);
        } else if (command->type == MGL_LIST_CMD_MULTI_TEXGEN_EXT) {
            glMultiTexGenfvEXT(command->u.multi_texgen_ext.texunit,
                               command->u.multi_texgen_ext.coord, command->u.multi_texgen_ext.pname,
                               command->u.multi_texgen_ext.params);
        } else if (command->type == MGL_LIST_CMD_PROGRAM_UNIFORM_EXT) {
            const unsigned components = command->u.program_uniform_ext.components;
            const GLuint program = command->u.program_uniform_ext.program;
            const GLint location = command->u.program_uniform_ext.location;
            const GLsizei count = command->u.program_uniform_ext.count;
            if (command->u.program_uniform_ext.value_kind == MGL_LIST_PROGRAM_UNIFORM_FLOAT) {
                const GLfloat *v = (const GLfloat *)command->u.program_uniform_ext.data;
                if (command->u.program_uniform_ext.scalar) {
                    if (components == 1u)
                        glProgramUniform1fEXT(program, location, v[0]);
                    else if (components == 2u)
                        glProgramUniform2fEXT(program, location, v[0], v[1]);
                    else if (components == 3u)
                        glProgramUniform3fEXT(program, location, v[0], v[1], v[2]);
                    else if (components == 4u)
                        glProgramUniform4fEXT(program, location, v[0], v[1], v[2], v[3]);
                } else {
                    if (components == 1u)
                        glProgramUniform1fvEXT(program, location, count, v);
                    else if (components == 2u)
                        glProgramUniform2fvEXT(program, location, count, v);
                    else if (components == 3u)
                        glProgramUniform3fvEXT(program, location, count, v);
                    else if (components == 4u)
                        glProgramUniform4fvEXT(program, location, count, v);
                }
            } else if (command->u.program_uniform_ext.value_kind == MGL_LIST_PROGRAM_UNIFORM_INT) {
                const GLint *v = (const GLint *)command->u.program_uniform_ext.data;
                if (command->u.program_uniform_ext.scalar) {
                    if (components == 1u)
                        glProgramUniform1iEXT(program, location, v[0]);
                    else if (components == 2u)
                        glProgramUniform2iEXT(program, location, v[0], v[1]);
                    else if (components == 3u)
                        glProgramUniform3iEXT(program, location, v[0], v[1], v[2]);
                    else if (components == 4u)
                        glProgramUniform4iEXT(program, location, v[0], v[1], v[2], v[3]);
                } else {
                    if (components == 1u)
                        glProgramUniform1ivEXT(program, location, count, v);
                    else if (components == 2u)
                        glProgramUniform2ivEXT(program, location, count, v);
                    else if (components == 3u)
                        glProgramUniform3ivEXT(program, location, count, v);
                    else if (components == 4u)
                        glProgramUniform4ivEXT(program, location, count, v);
                }
            } else if (command->u.program_uniform_ext.value_kind ==
                       MGL_LIST_PROGRAM_UNIFORM_MATRIX) {
                const GLfloat *v = (const GLfloat *)command->u.program_uniform_ext.data;
                if (components == 4u)
                    glProgramUniformMatrix2fvEXT(program, location, count,
                                                 command->u.program_uniform_ext.transpose, v);
                else if (components == 9u)
                    glProgramUniformMatrix3fvEXT(program, location, count,
                                                 command->u.program_uniform_ext.transpose, v);
                else if (components == 16u)
                    glProgramUniformMatrix4fvEXT(program, location, count,
                                                 command->u.program_uniform_ext.transpose, v);
            }
        } else if (command->type == MGL_LIST_CMD_ARB_PROGRAM_PARAMETERS) {
            if (command->u.arb_program_parameters.local) {
                glProgramLocalParameters4fvEXT(command->u.arb_program_parameters.target,
                                               command->u.arb_program_parameters.index,
                                               command->u.arb_program_parameters.count,
                                               command->u.arb_program_parameters.values);
            } else {
                glProgramEnvParameters4fvEXT(command->u.arb_program_parameters.target,
                                             command->u.arb_program_parameters.index,
                                             command->u.arb_program_parameters.count,
                                             command->u.arb_program_parameters.values);
            }
        } else if (command->type == MGL_LIST_CMD_ARB_PROGRAM_STRING) {
            glProgramStringARB(
                command->u.arb_program_string.target, command->u.arb_program_string.format,
                command->u.arb_program_string.len, command->u.arb_program_string.string);
        } else if (command->type == MGL_LIST_CMD_CLIP_PLANE) {
            GLdouble equation[4];
            unsigned k;
            for (k = 0u; k < 4u; ++k)
                equation[k] = (GLdouble)command->u.clip_plane.equation[k];
            glClipPlane(command->u.clip_plane.plane, equation);
        } else if (command->type == MGL_LIST_CMD_SCISSOR) {
            glScissor(command->u.scissor.x, command->u.scissor.y, command->u.scissor.width,
                      command->u.scissor.height);
        } else if (command->type == MGL_LIST_CMD_DEPTH_FUNC) {
            glDepthFunc(command->u.state_enum);
        } else if (command->type == MGL_LIST_CMD_DEPTH_MASK) {
            glDepthMask(command->u.state_boolean);
        } else if (command->type == MGL_LIST_CMD_INDEX_MASK) {
            glIndexMask(command->u.index_mask);
        } else if (command->type == MGL_LIST_CMD_COLOR_MASK) {
            glColorMask(command->u.color_mask[0], command->u.color_mask[1],
                        command->u.color_mask[2], command->u.color_mask[3]);
        } else if (command->type == MGL_LIST_CMD_CULL_FACE) {
            glCullFace(command->u.state_enum);
        } else if (command->type == MGL_LIST_CMD_FRONT_FACE) {
            glFrontFace(command->u.state_enum);
        } else if (command->type == MGL_LIST_CMD_STENCIL_FUNC) {
            if (command->u.stencil_func.face == GL_NONE)
                glStencilFunc(command->u.stencil_func.func, command->u.stencil_func.ref,
                              command->u.stencil_func.mask);
            else
                glStencilFuncSeparate(command->u.stencil_func.face, command->u.stencil_func.func,
                                      command->u.stencil_func.ref, command->u.stencil_func.mask);
        } else if (command->type == MGL_LIST_CMD_STENCIL_MASK) {
            if (command->u.stencil_mask.face == GL_NONE)
                glStencilMask(command->u.stencil_mask.mask);
            else
                glStencilMaskSeparate(command->u.stencil_mask.face, command->u.stencil_mask.mask);
        } else if (command->type == MGL_LIST_CMD_STENCIL_OP) {
            if (command->u.stencil_op.face == GL_NONE)
                glStencilOp(command->u.stencil_op.sfail, command->u.stencil_op.dpfail,
                            command->u.stencil_op.dppass);
            else
                glStencilOpSeparate(command->u.stencil_op.face, command->u.stencil_op.sfail,
                                    command->u.stencil_op.dpfail, command->u.stencil_op.dppass);
        } else if (command->type == MGL_LIST_CMD_BLEND_FUNC_SEPARATE) {
            glBlendFuncSeparate(command->u.blend_func.src_rgb, command->u.blend_func.dst_rgb,
                                command->u.blend_func.src_alpha, command->u.blend_func.dst_alpha);
        } else if (command->type == MGL_LIST_CMD_BLEND_EQUATION_SEPARATE) {
            if (command->u.blend_equation.rgb == command->u.blend_equation.alpha &&
                command->u.blend_equation.rgb >= GL_MULTIPLY_KHR &&
                command->u.blend_equation.rgb <= GL_HSL_LUMINOSITY_KHR)
                glBlendEquation(command->u.blend_equation.rgb);
            else
                glBlendEquationSeparate(command->u.blend_equation.rgb, command->u.blend_equation.alpha);
        } else if (command->type == MGL_LIST_CMD_DEPTH_RANGE) {
            glDepthRangef(command->u.depth_range.near_value, command->u.depth_range.far_value);
        } else if (command->type == MGL_LIST_CMD_BLEND_COLOR) {
            glBlendColor(command->u.blend_color[0], command->u.blend_color[1],
                         command->u.blend_color[2], command->u.blend_color[3]);
        } else if (command->type == MGL_LIST_CMD_LINE_WIDTH) {
            glLineWidth(command->u.line_width);
        } else if (command->type == MGL_LIST_CMD_POLYGON_OFFSET) {
            glPolygonOffset(command->u.polygon_offset.factor, command->u.polygon_offset.units);
        } else if (command->type == MGL_LIST_CMD_SAMPLE_COVERAGE) {
            glSampleCoverage(command->u.sample_coverage.value, command->u.sample_coverage.invert);
        } else if (command->type == MGL_LIST_CMD_HINT) {
            glHint(command->u.hint.target, command->u.hint.mode);
        } else if (command->type == MGL_LIST_CMD_SET_FENCE_NV) {
            glSetFenceNV(command->u.fence.name, command->u.fence.condition);
        } else if (command->type == MGL_LIST_CMD_ENABLE_INDEXED) {
            if (command->u.indexed_enable.enabled)
                glEnablei(command->u.indexed_enable.cap, command->u.indexed_enable.index);
            else
                glDisablei(command->u.indexed_enable.cap, command->u.indexed_enable.index);
        } else if (command->type == MGL_LIST_CMD_ENABLE_INDEXED_EXT) {
            if (command->u.indexed_enable.enabled)
                glEnableIndexedEXT(command->u.indexed_enable.cap, command->u.indexed_enable.index);
            else
                glDisableIndexedEXT(command->u.indexed_enable.cap, command->u.indexed_enable.index);
        } else if (command->type == MGL_LIST_CMD_COLOR_MASK_INDEXED) {
            glColorMaski(command->u.indexed_color_mask.index, command->u.indexed_color_mask.mask[0],
                         command->u.indexed_color_mask.mask[1],
                         command->u.indexed_color_mask.mask[2],
                         command->u.indexed_color_mask.mask[3]);
        } else if (command->type == MGL_LIST_CMD_BLEND_FUNC_INDEXED) {
            glBlendFuncSeparatei(
                command->u.indexed_blend_func.index, command->u.indexed_blend_func.src_rgb,
                command->u.indexed_blend_func.dst_rgb, command->u.indexed_blend_func.src_alpha,
                command->u.indexed_blend_func.dst_alpha);
        } else if (command->type == MGL_LIST_CMD_BLEND_EQUATION_INDEXED) {
            if (command->u.indexed_blend_equation.rgb ==
                    command->u.indexed_blend_equation.alpha &&
                command->u.indexed_blend_equation.rgb >= GL_MULTIPLY_KHR &&
                command->u.indexed_blend_equation.rgb <= GL_HSL_LUMINOSITY_KHR)
                glBlendEquationi(command->u.indexed_blend_equation.index,
                                 command->u.indexed_blend_equation.rgb);
            else
                glBlendEquationSeparatei(command->u.indexed_blend_equation.index,
                                         command->u.indexed_blend_equation.rgb,
                                         command->u.indexed_blend_equation.alpha);
        } else if (command->type == MGL_LIST_CMD_CLEAR_BUFFER_IV) {
            glClearBufferiv(command->u.clear_buffer_iv.buffer,
                            command->u.clear_buffer_iv.drawbuffer,
                            command->u.clear_buffer_iv.value);
        } else if (command->type == MGL_LIST_CMD_CLEAR_BUFFER_UIV) {
            glClearBufferuiv(command->u.clear_buffer_uiv.buffer,
                             command->u.clear_buffer_uiv.drawbuffer,
                             command->u.clear_buffer_uiv.value);
        } else if (command->type == MGL_LIST_CMD_CLEAR_BUFFER_FV) {
            glClearBufferfv(command->u.clear_buffer_fv.buffer,
                            command->u.clear_buffer_fv.drawbuffer,
                            command->u.clear_buffer_fv.value);
        } else if (command->type == MGL_LIST_CMD_CLEAR_BUFFER_FI) {
            glClearBufferfi(command->u.clear_buffer_fi.buffer,
                            command->u.clear_buffer_fi.drawbuffer, command->u.clear_buffer_fi.depth,
                            command->u.clear_buffer_fi.stencil);
        } else if (command->type == MGL_LIST_CMD_DRAW_BUFFER) {
            glDrawBuffer(command->u.enum_value);
        } else if (command->type == MGL_LIST_CMD_DRAW_BUFFERS) {
            glDrawBuffers(command->u.draw_buffers.count, command->u.draw_buffers.buffers);
        } else if (command->type == MGL_LIST_CMD_READ_BUFFER) {
            glReadBuffer(command->u.enum_value);
        } else if (command->type == MGL_LIST_CMD_MAP1) {
            glMap1d(command->u.map1.target, command->u.map1.u1, command->u.map1.u2,
                    (GLint)command->u.map1.components, command->u.map1.order,
                    command->u.map1.coeff);
        } else if (command->type == MGL_LIST_CMD_MAP2) {
            GLint vstride = (GLint)command->u.map2.components;
            GLint ustride = command->u.map2.vorder * vstride;
            glMap2d(command->u.map2.target, command->u.map2.u1, command->u.map2.u2, ustride,
                    command->u.map2.uorder, command->u.map2.v1, command->u.map2.v2, vstride,
                    command->u.map2.vorder, command->u.map2.coeff);
        } else if (command->type == MGL_LIST_CMD_EVAL_COORD1) {
            glEvalCoord1d(command->u.eval_coord.u);
        } else if (command->type == MGL_LIST_CMD_EVAL_COORD2) {
            glEvalCoord2d(command->u.eval_coord.u, command->u.eval_coord.v);
        } else if (command->type == MGL_LIST_CMD_MAP_GRID1) {
            glMapGrid1d(command->u.grid1.un, command->u.grid1.u1, command->u.grid1.u2);
        } else if (command->type == MGL_LIST_CMD_MAP_GRID2) {
            glMapGrid2d(command->u.grid2.un, command->u.grid2.u1, command->u.grid2.u2,
                        command->u.grid2.vn, command->u.grid2.v1, command->u.grid2.v2);
        } else if (command->type == MGL_LIST_CMD_EVAL_POINT1) {
            glEvalPoint1(command->u.eval_point.i);
        } else if (command->type == MGL_LIST_CMD_EVAL_POINT2) {
            glEvalPoint2(command->u.eval_point.i, command->u.eval_point.j);
        } else if (command->type == MGL_LIST_CMD_EVAL_MESH1) {
            glEvalMesh1(command->u.eval_mesh.mode, command->u.eval_mesh.i1,
                        command->u.eval_mesh.i2);
        } else if (command->type == MGL_LIST_CMD_EVAL_MESH2) {
            glEvalMesh2(command->u.eval_mesh.mode, command->u.eval_mesh.i1, command->u.eval_mesh.i2,
                        command->u.eval_mesh.j1, command->u.eval_mesh.j2);
        } else if (command->type == MGL_LIST_CMD_RASTER_POS4D) {
            glRasterPos4d(command->u.raster_pos.x, command->u.raster_pos.y, command->u.raster_pos.z,
                          command->u.raster_pos.w);
        } else if (command->type == MGL_LIST_CMD_WINDOW_POS3F) {
            glWindowPos3f(command->u.window_pos.x, command->u.window_pos.y,
                          command->u.window_pos.z);
        } else if (command->type == MGL_LIST_CMD_PIXEL_ZOOM) {
            glPixelZoom(command->u.pixel_zoom.xfactor, command->u.pixel_zoom.yfactor);
        } else if (command->type == MGL_LIST_CMD_DRAW_PIXELS) {
            mgl_raster_draw_pixels_tight(
                ctx, command->u.draw_pixels.width, command->u.draw_pixels.height,
                command->u.draw_pixels.format, command->u.draw_pixels.data_type,
                command->u.draw_pixels.data, command->u.draw_pixels.data_size);
        } else if (command->type == MGL_LIST_CMD_BITMAP) {
            mgl_raster_bitmap_tight(ctx, command->u.bitmap.width, command->u.bitmap.height,
                                    command->u.bitmap.xorig, command->u.bitmap.yorig,
                                    command->u.bitmap.xmove, command->u.bitmap.ymove,
                                    command->u.bitmap.data, command->u.bitmap.data_size);
        } else if (command->type == MGL_LIST_CMD_COPY_PIXELS) {
            glCopyPixels(command->u.copy_pixels.x, command->u.copy_pixels.y,
                         command->u.copy_pixels.width, command->u.copy_pixels.height,
                         command->u.copy_pixels.copy_type);
        } else if (command->type == MGL_LIST_CMD_BEGIN) {
            glBegin(command->u.begin_mode);
        } else if (command->type == MGL_LIST_CMD_VERTEX4F) {
            glVertex4f(command->u.vec4[0], command->u.vec4[1], command->u.vec4[2],
                       command->u.vec4[3]);
        } else if (command->type == MGL_LIST_CMD_END) {
            glEnd();
        } else if (command->type == MGL_LIST_CMD_BIND_TEXTURE) {
            glBindTexture(command->u.bind_texture.target, command->u.bind_texture.texture);
        } else if (command->type == MGL_LIST_CMD_BIND_MULTI_TEXTURE) {
            glBindMultiTexture(command->u.bind_multi_texture.texunit,
                               command->u.bind_multi_texture.target,
                               command->u.bind_multi_texture.texture);
        } else if (command->type == MGL_LIST_CMD_MULTI_TEX_BUFFER) {
            glMultiTexBuffer(
                command->u.multi_tex_buffer.texunit, command->u.multi_tex_buffer.target,
                command->u.multi_tex_buffer.internalformat, command->u.multi_tex_buffer.buffer);
        } else if (command->type == MGL_LIST_CMD_TEXTURE_BUFFER_EXT) {
            glTextureBufferEXT(
                command->u.texture_buffer_ext.texture, command->u.texture_buffer_ext.target,
                command->u.texture_buffer_ext.internalformat, command->u.texture_buffer_ext.buffer);
        } else if (command->type == MGL_LIST_CMD_PRIORITIZE_TEXTURES) {
            glPrioritizeTextures(command->u.prioritize_textures.count,
                                 command->u.prioritize_textures.textures,
                                 command->u.prioritize_textures.priorities);
        } else if (command->type == MGL_LIST_CMD_TEX_PARAMETER) {
            if (command->u.tex_parameter.integer_typed && command->u.tex_parameter.unsigned_integer)
                glTexParameterIuiv(command->u.tex_parameter.target, command->u.tex_parameter.pname,
                                   command->u.tex_parameter.uparam);
            else if (command->u.tex_parameter.integer_typed)
                glTexParameterIiv(command->u.tex_parameter.target, command->u.tex_parameter.pname,
                                  command->u.tex_parameter.iparam);
            else if (command->u.tex_parameter.vector4 && command->u.tex_parameter.floating)
                glTexParameterfv(command->u.tex_parameter.target, command->u.tex_parameter.pname,
                                 command->u.tex_parameter.fparam);
            else if (command->u.tex_parameter.vector4)
                glTexParameteriv(command->u.tex_parameter.target, command->u.tex_parameter.pname,
                                 command->u.tex_parameter.iparam);
            else if (command->u.tex_parameter.floating)
                glTexParameterf(command->u.tex_parameter.target, command->u.tex_parameter.pname,
                                command->u.tex_parameter.fparam[0]);
            else
                glTexParameteri(command->u.tex_parameter.target, command->u.tex_parameter.pname,
                                command->u.tex_parameter.iparam[0]);
        } else if (command->type == MGL_LIST_CMD_MULTI_TEX_PARAMETER) {
            if (command->u.multi_tex_parameter.integer_typed &&
                command->u.multi_tex_parameter.unsigned_integer)
                glMultiTexParameterIuivEXT(
                    command->u.multi_tex_parameter.texunit, command->u.multi_tex_parameter.target,
                    command->u.multi_tex_parameter.pname, command->u.multi_tex_parameter.uparam);
            else if (command->u.multi_tex_parameter.integer_typed)
                glMultiTexParameterIivEXT(
                    command->u.multi_tex_parameter.texunit, command->u.multi_tex_parameter.target,
                    command->u.multi_tex_parameter.pname, command->u.multi_tex_parameter.iparam);
            else if (command->u.multi_tex_parameter.vector4 &&
                     command->u.multi_tex_parameter.floating)
                glMultiTexParameterfv(
                    command->u.multi_tex_parameter.texunit, command->u.multi_tex_parameter.target,
                    command->u.multi_tex_parameter.pname, command->u.multi_tex_parameter.fparam);
            else if (command->u.multi_tex_parameter.vector4)
                glMultiTexParameteriv(
                    command->u.multi_tex_parameter.texunit, command->u.multi_tex_parameter.target,
                    command->u.multi_tex_parameter.pname, command->u.multi_tex_parameter.iparam);
            else if (command->u.multi_tex_parameter.floating)
                glMultiTexParameterf(
                    command->u.multi_tex_parameter.texunit, command->u.multi_tex_parameter.target,
                    command->u.multi_tex_parameter.pname, command->u.multi_tex_parameter.fparam[0]);
            else
                glMultiTexParameteri(
                    command->u.multi_tex_parameter.texunit, command->u.multi_tex_parameter.target,
                    command->u.multi_tex_parameter.pname, command->u.multi_tex_parameter.iparam[0]);
        } else if (command->type == MGL_LIST_CMD_TEXTURE_PARAMETER_EXT) {
            if (command->u.texture_parameter_ext.integer_typed &&
                command->u.texture_parameter_ext.unsigned_integer)
                glTextureParameterIuivEXT(command->u.texture_parameter_ext.texture,
                                          command->u.texture_parameter_ext.target,
                                          command->u.texture_parameter_ext.pname,
                                          command->u.texture_parameter_ext.uparam);
            else if (command->u.texture_parameter_ext.integer_typed)
                glTextureParameterIivEXT(command->u.texture_parameter_ext.texture,
                                         command->u.texture_parameter_ext.target,
                                         command->u.texture_parameter_ext.pname,
                                         command->u.texture_parameter_ext.iparam);
            else if (command->u.texture_parameter_ext.vector4 &&
                     command->u.texture_parameter_ext.floating)
                glTextureParameterfvEXT(command->u.texture_parameter_ext.texture,
                                        command->u.texture_parameter_ext.target,
                                        command->u.texture_parameter_ext.pname,
                                        command->u.texture_parameter_ext.fparam);
            else if (command->u.texture_parameter_ext.vector4)
                glTextureParameterivEXT(command->u.texture_parameter_ext.texture,
                                        command->u.texture_parameter_ext.target,
                                        command->u.texture_parameter_ext.pname,
                                        command->u.texture_parameter_ext.iparam);
            else if (command->u.texture_parameter_ext.floating)
                glTextureParameterfEXT(command->u.texture_parameter_ext.texture,
                                       command->u.texture_parameter_ext.target,
                                       command->u.texture_parameter_ext.pname,
                                       command->u.texture_parameter_ext.fparam[0]);
            else
                glTextureParameteriEXT(command->u.texture_parameter_ext.texture,
                                       command->u.texture_parameter_ext.target,
                                       command->u.texture_parameter_ext.pname,
                                       command->u.texture_parameter_ext.iparam[0]);
        } else if (command->type == MGL_LIST_CMD_MULTI_TEX_IMAGE_2D) {
            (void)mgl_texture_apply_image_2d_tight_on_unit(
                ctx, (unsigned)(command->u.multi_tex_image_2d.texunit - GL_TEXTURE0),
                command->u.multi_tex_image_2d.target, command->u.multi_tex_image_2d.level,
                command->u.multi_tex_image_2d.internalformat, command->u.multi_tex_image_2d.width,
                command->u.multi_tex_image_2d.height, command->u.multi_tex_image_2d.border,
                command->u.multi_tex_image_2d.format, command->u.multi_tex_image_2d.data_type,
                command->u.multi_tex_image_2d.data, command->u.multi_tex_image_2d.data_size);
        } else if (command->type == MGL_LIST_CMD_MULTI_TEX_SUB_IMAGE_2D) {
            (void)mgl_texture_apply_sub_image_2d_tight_on_unit(
                ctx, (unsigned)(command->u.multi_tex_sub_image_2d.texunit - GL_TEXTURE0),
                command->u.multi_tex_sub_image_2d.target, command->u.multi_tex_sub_image_2d.level,
                command->u.multi_tex_sub_image_2d.xoffset,
                command->u.multi_tex_sub_image_2d.yoffset, command->u.multi_tex_sub_image_2d.width,
                command->u.multi_tex_sub_image_2d.height, command->u.multi_tex_sub_image_2d.format,
                command->u.multi_tex_sub_image_2d.data_type, command->u.multi_tex_sub_image_2d.data,
                command->u.multi_tex_sub_image_2d.data_size);
        } else if (command->type == MGL_LIST_CMD_MULTI_TEX_IMAGE_3D) {
            (void)mgl_texture_apply_image_3d_tight_on_unit(
                ctx, (unsigned)(command->u.multi_tex_image_3d.texunit - GL_TEXTURE0),
                command->u.multi_tex_image_3d.target, command->u.multi_tex_image_3d.level,
                command->u.multi_tex_image_3d.internalformat, command->u.multi_tex_image_3d.width,
                command->u.multi_tex_image_3d.height, command->u.multi_tex_image_3d.depth,
                command->u.multi_tex_image_3d.border, command->u.multi_tex_image_3d.format,
                command->u.multi_tex_image_3d.data_type, command->u.multi_tex_image_3d.data,
                command->u.multi_tex_image_3d.data_size);
        } else if (command->type == MGL_LIST_CMD_MULTI_TEX_SUB_IMAGE_3D) {
            (void)mgl_texture_apply_sub_image_3d_tight_on_unit(
                ctx, (unsigned)(command->u.multi_tex_sub_image_3d.texunit - GL_TEXTURE0),
                command->u.multi_tex_sub_image_3d.target, command->u.multi_tex_sub_image_3d.level,
                command->u.multi_tex_sub_image_3d.xoffset,
                command->u.multi_tex_sub_image_3d.yoffset,
                command->u.multi_tex_sub_image_3d.zoffset, command->u.multi_tex_sub_image_3d.width,
                command->u.multi_tex_sub_image_3d.height, command->u.multi_tex_sub_image_3d.depth,
                command->u.multi_tex_sub_image_3d.format,
                command->u.multi_tex_sub_image_3d.data_type, command->u.multi_tex_sub_image_3d.data,
                command->u.multi_tex_sub_image_3d.data_size);
        } else if (command->type == MGL_LIST_CMD_COMPRESSED_MULTI_TEX_IMAGE_2D) {
            (void)mgl_texture_apply_compressed_image_2d_tight_on_unit(
                ctx, (unsigned)(command->u.compressed_multi_tex_image_2d.texunit - GL_TEXTURE0),
                command->u.compressed_multi_tex_image_2d.target,
                command->u.compressed_multi_tex_image_2d.level,
                command->u.compressed_multi_tex_image_2d.internalformat,
                command->u.compressed_multi_tex_image_2d.width,
                command->u.compressed_multi_tex_image_2d.height,
                command->u.compressed_multi_tex_image_2d.border,
                command->u.compressed_multi_tex_image_2d.data,
                command->u.compressed_multi_tex_image_2d.data_size);
        } else if (command->type == MGL_LIST_CMD_COMPRESSED_MULTI_TEX_SUB_IMAGE_2D) {
            (void)mgl_texture_apply_compressed_sub_image_2d_tight_on_unit(
                ctx, (unsigned)(command->u.compressed_multi_tex_sub_image_2d.texunit - GL_TEXTURE0),
                command->u.compressed_multi_tex_sub_image_2d.target,
                command->u.compressed_multi_tex_sub_image_2d.level,
                command->u.compressed_multi_tex_sub_image_2d.xoffset,
                command->u.compressed_multi_tex_sub_image_2d.yoffset,
                command->u.compressed_multi_tex_sub_image_2d.width,
                command->u.compressed_multi_tex_sub_image_2d.height,
                command->u.compressed_multi_tex_sub_image_2d.format,
                command->u.compressed_multi_tex_sub_image_2d.data,
                command->u.compressed_multi_tex_sub_image_2d.data_size);
        } else if (command->type == MGL_LIST_CMD_COMPRESSED_MULTI_TEX_IMAGE_3D) {
            (void)mgl_texture_apply_compressed_image_3d_tight_on_unit(
                ctx, (unsigned)(command->u.compressed_multi_tex_image_3d.texunit - GL_TEXTURE0),
                command->u.compressed_multi_tex_image_3d.target,
                command->u.compressed_multi_tex_image_3d.level,
                command->u.compressed_multi_tex_image_3d.internalformat,
                command->u.compressed_multi_tex_image_3d.width,
                command->u.compressed_multi_tex_image_3d.height,
                command->u.compressed_multi_tex_image_3d.depth,
                command->u.compressed_multi_tex_image_3d.border,
                command->u.compressed_multi_tex_image_3d.data,
                command->u.compressed_multi_tex_image_3d.data_size);
        } else if (command->type == MGL_LIST_CMD_COMPRESSED_MULTI_TEX_SUB_IMAGE_3D) {
            (void)mgl_texture_apply_compressed_sub_image_3d_tight_on_unit(
                ctx, (unsigned)(command->u.compressed_multi_tex_sub_image_3d.texunit - GL_TEXTURE0),
                command->u.compressed_multi_tex_sub_image_3d.target,
                command->u.compressed_multi_tex_sub_image_3d.level,
                command->u.compressed_multi_tex_sub_image_3d.xoffset,
                command->u.compressed_multi_tex_sub_image_3d.yoffset,
                command->u.compressed_multi_tex_sub_image_3d.zoffset,
                command->u.compressed_multi_tex_sub_image_3d.width,
                command->u.compressed_multi_tex_sub_image_3d.height,
                command->u.compressed_multi_tex_sub_image_3d.depth,
                command->u.compressed_multi_tex_sub_image_3d.format,
                command->u.compressed_multi_tex_sub_image_3d.data,
                command->u.compressed_multi_tex_sub_image_3d.data_size);
        } else if (command->type == MGL_LIST_CMD_TEX_IMAGE_2D) {
            (void)mgl_texture_apply_image_2d_tight(
                ctx, command->u.tex_image_2d.target, command->u.tex_image_2d.level,
                command->u.tex_image_2d.internalformat, command->u.tex_image_2d.width,
                command->u.tex_image_2d.height, command->u.tex_image_2d.border,
                command->u.tex_image_2d.format, command->u.tex_image_2d.data_type,
                command->u.tex_image_2d.data, command->u.tex_image_2d.data_size);
        } else if (command->type == MGL_LIST_CMD_TEX_SUB_IMAGE_2D) {
            (void)mgl_texture_apply_sub_image_2d_tight(
                ctx, command->u.tex_sub_image_2d.target, command->u.tex_sub_image_2d.level,
                command->u.tex_sub_image_2d.xoffset, command->u.tex_sub_image_2d.yoffset,
                command->u.tex_sub_image_2d.width, command->u.tex_sub_image_2d.height,
                command->u.tex_sub_image_2d.format, command->u.tex_sub_image_2d.data_type,
                command->u.tex_sub_image_2d.data, command->u.tex_sub_image_2d.data_size);
        } else if (command->type == MGL_LIST_CMD_TEX_IMAGE_3D) {
            (void)mgl_texture_apply_image_3d_tight(
                ctx, command->u.tex_image_3d.target, command->u.tex_image_3d.level,
                command->u.tex_image_3d.internalformat, command->u.tex_image_3d.width,
                command->u.tex_image_3d.height, command->u.tex_image_3d.depth,
                command->u.tex_image_3d.border, command->u.tex_image_3d.format,
                command->u.tex_image_3d.data_type, command->u.tex_image_3d.data,
                command->u.tex_image_3d.data_size);
        } else if (command->type == MGL_LIST_CMD_TEX_SUB_IMAGE_3D) {
            (void)mgl_texture_apply_sub_image_3d_tight(
                ctx, command->u.tex_sub_image_3d.target, command->u.tex_sub_image_3d.level,
                command->u.tex_sub_image_3d.xoffset, command->u.tex_sub_image_3d.yoffset,
                command->u.tex_sub_image_3d.zoffset, command->u.tex_sub_image_3d.width,
                command->u.tex_sub_image_3d.height, command->u.tex_sub_image_3d.depth,
                command->u.tex_sub_image_3d.format, command->u.tex_sub_image_3d.data_type,
                command->u.tex_sub_image_3d.data, command->u.tex_sub_image_3d.data_size);
        } else if (command->type == MGL_LIST_CMD_COMPRESSED_TEX_IMAGE_2D) {
            glCompressedTexImage2D(
                command->u.compressed_tex_image_2d.target, command->u.compressed_tex_image_2d.level,
                command->u.compressed_tex_image_2d.internalformat,
                command->u.compressed_tex_image_2d.width, command->u.compressed_tex_image_2d.height,
                command->u.compressed_tex_image_2d.border,
                (GLsizei)command->u.compressed_tex_image_2d.data_size,
                command->u.compressed_tex_image_2d.data);
        } else if (command->type == MGL_LIST_CMD_COMPRESSED_TEX_IMAGE_3D) {
            glCompressedTexImage3D(
                command->u.compressed_tex_image_3d.target, command->u.compressed_tex_image_3d.level,
                command->u.compressed_tex_image_3d.internalformat,
                command->u.compressed_tex_image_3d.width, command->u.compressed_tex_image_3d.height,
                command->u.compressed_tex_image_3d.depth, command->u.compressed_tex_image_3d.border,
                (GLsizei)command->u.compressed_tex_image_3d.data_size,
                command->u.compressed_tex_image_3d.data);
        } else if (command->type == MGL_LIST_CMD_COMPRESSED_TEX_SUB_IMAGE_2D) {
            glCompressedTexSubImage2D(command->u.compressed_tex_sub_image_2d.target,
                                      command->u.compressed_tex_sub_image_2d.level,
                                      command->u.compressed_tex_sub_image_2d.xoffset,
                                      command->u.compressed_tex_sub_image_2d.yoffset,
                                      command->u.compressed_tex_sub_image_2d.width,
                                      command->u.compressed_tex_sub_image_2d.height,
                                      command->u.compressed_tex_sub_image_2d.format,
                                      (GLsizei)command->u.compressed_tex_sub_image_2d.data_size,
                                      command->u.compressed_tex_sub_image_2d.data);
        } else if (command->type == MGL_LIST_CMD_COMPRESSED_TEX_SUB_IMAGE_3D) {
            glCompressedTexSubImage3D(command->u.compressed_tex_sub_image_3d.target,
                                      command->u.compressed_tex_sub_image_3d.level,
                                      command->u.compressed_tex_sub_image_3d.xoffset,
                                      command->u.compressed_tex_sub_image_3d.yoffset,
                                      command->u.compressed_tex_sub_image_3d.zoffset,
                                      command->u.compressed_tex_sub_image_3d.width,
                                      command->u.compressed_tex_sub_image_3d.height,
                                      command->u.compressed_tex_sub_image_3d.depth,
                                      command->u.compressed_tex_sub_image_3d.format,
                                      (GLsizei)command->u.compressed_tex_sub_image_3d.data_size,
                                      command->u.compressed_tex_sub_image_3d.data);
        } else if (command->type == MGL_LIST_CMD_COPY_TEX_IMAGE_2D) {
            if (command->u.copy_tex_image_2d.target == GL_TEXTURE_1D)
                glCopyTexImage1D(
                    command->u.copy_tex_image_2d.target, command->u.copy_tex_image_2d.level,
                    command->u.copy_tex_image_2d.internalformat, command->u.copy_tex_image_2d.x,
                    command->u.copy_tex_image_2d.y, command->u.copy_tex_image_2d.width,
                    command->u.copy_tex_image_2d.border);
            else
                glCopyTexImage2D(
                    command->u.copy_tex_image_2d.target, command->u.copy_tex_image_2d.level,
                    command->u.copy_tex_image_2d.internalformat, command->u.copy_tex_image_2d.x,
                    command->u.copy_tex_image_2d.y, command->u.copy_tex_image_2d.width,
                    command->u.copy_tex_image_2d.height, command->u.copy_tex_image_2d.border);
        } else if (command->type == MGL_LIST_CMD_COPY_TEX_SUB_IMAGE_2D) {
            if (command->u.copy_tex_sub_image_2d.target == GL_TEXTURE_1D)
                glCopyTexSubImage1D(
                    command->u.copy_tex_sub_image_2d.target, command->u.copy_tex_sub_image_2d.level,
                    command->u.copy_tex_sub_image_2d.xoffset, command->u.copy_tex_sub_image_2d.x,
                    command->u.copy_tex_sub_image_2d.y, command->u.copy_tex_sub_image_2d.width);
            else
                glCopyTexSubImage2D(
                    command->u.copy_tex_sub_image_2d.target, command->u.copy_tex_sub_image_2d.level,
                    command->u.copy_tex_sub_image_2d.xoffset,
                    command->u.copy_tex_sub_image_2d.yoffset, command->u.copy_tex_sub_image_2d.x,
                    command->u.copy_tex_sub_image_2d.y, command->u.copy_tex_sub_image_2d.width,
                    command->u.copy_tex_sub_image_2d.height);
        } else if (command->type == MGL_LIST_CMD_COPY_TEX_SUB_IMAGE_3D) {
            glCopyTexSubImage3D(
                command->u.copy_tex_sub_image_3d.target, command->u.copy_tex_sub_image_3d.level,
                command->u.copy_tex_sub_image_3d.xoffset, command->u.copy_tex_sub_image_3d.yoffset,
                command->u.copy_tex_sub_image_3d.zoffset, command->u.copy_tex_sub_image_3d.x,
                command->u.copy_tex_sub_image_3d.y, command->u.copy_tex_sub_image_3d.width,
                command->u.copy_tex_sub_image_3d.height);
        } else if (command->type == MGL_LIST_CMD_COMPRESSED_TEXTURE_IMAGE_2D_EXT) {
            (void)mgl_texture_apply_compressed_image_2d_tight_named_ext(
                ctx, command->u.compressed_texture_image_2d_ext.texture,
                command->u.compressed_texture_image_2d_ext.target,
                command->u.compressed_texture_image_2d_ext.level,
                command->u.compressed_texture_image_2d_ext.internalformat,
                command->u.compressed_texture_image_2d_ext.width,
                command->u.compressed_texture_image_2d_ext.height,
                command->u.compressed_texture_image_2d_ext.border,
                command->u.compressed_texture_image_2d_ext.data,
                command->u.compressed_texture_image_2d_ext.data_size);
        } else if (command->type == MGL_LIST_CMD_COMPRESSED_TEXTURE_SUB_IMAGE_2D_EXT) {
            (void)mgl_texture_apply_compressed_sub_image_2d_tight_named_ext(
                ctx, command->u.compressed_texture_sub_image_2d_ext.texture,
                command->u.compressed_texture_sub_image_2d_ext.target,
                command->u.compressed_texture_sub_image_2d_ext.level,
                command->u.compressed_texture_sub_image_2d_ext.xoffset,
                command->u.compressed_texture_sub_image_2d_ext.yoffset,
                command->u.compressed_texture_sub_image_2d_ext.width,
                command->u.compressed_texture_sub_image_2d_ext.height,
                command->u.compressed_texture_sub_image_2d_ext.format,
                command->u.compressed_texture_sub_image_2d_ext.data,
                command->u.compressed_texture_sub_image_2d_ext.data_size);
        } else if (command->type == MGL_LIST_CMD_COMPRESSED_TEXTURE_IMAGE_3D_EXT) {
            (void)mgl_texture_apply_compressed_image_3d_tight_named_ext(
                ctx, command->u.compressed_texture_image_3d_ext.texture,
                command->u.compressed_texture_image_3d_ext.target,
                command->u.compressed_texture_image_3d_ext.level,
                command->u.compressed_texture_image_3d_ext.internalformat,
                command->u.compressed_texture_image_3d_ext.width,
                command->u.compressed_texture_image_3d_ext.height,
                command->u.compressed_texture_image_3d_ext.depth,
                command->u.compressed_texture_image_3d_ext.border,
                command->u.compressed_texture_image_3d_ext.data,
                command->u.compressed_texture_image_3d_ext.data_size);
        } else if (command->type == MGL_LIST_CMD_COMPRESSED_TEXTURE_SUB_IMAGE_3D_EXT) {
            (void)mgl_texture_apply_compressed_sub_image_3d_tight_named_ext(
                ctx, command->u.compressed_texture_sub_image_3d_ext.texture,
                command->u.compressed_texture_sub_image_3d_ext.target,
                command->u.compressed_texture_sub_image_3d_ext.level,
                command->u.compressed_texture_sub_image_3d_ext.xoffset,
                command->u.compressed_texture_sub_image_3d_ext.yoffset,
                command->u.compressed_texture_sub_image_3d_ext.zoffset,
                command->u.compressed_texture_sub_image_3d_ext.width,
                command->u.compressed_texture_sub_image_3d_ext.height,
                command->u.compressed_texture_sub_image_3d_ext.depth,
                command->u.compressed_texture_sub_image_3d_ext.format,
                command->u.compressed_texture_sub_image_3d_ext.data,
                command->u.compressed_texture_sub_image_3d_ext.data_size);
        } else if (command->type == MGL_LIST_CMD_TEXTURE_IMAGE_2D_EXT) {
            (void)mgl_texture_apply_image_2d_tight_named_ext(
                ctx, command->u.texture_image_2d_ext.texture,
                command->u.texture_image_2d_ext.target, command->u.texture_image_2d_ext.level,
                command->u.texture_image_2d_ext.internalformat,
                command->u.texture_image_2d_ext.width, command->u.texture_image_2d_ext.height,
                command->u.texture_image_2d_ext.border, command->u.texture_image_2d_ext.format,
                command->u.texture_image_2d_ext.data_type, command->u.texture_image_2d_ext.data,
                command->u.texture_image_2d_ext.data_size);
        } else if (command->type == MGL_LIST_CMD_TEXTURE_SUB_IMAGE_2D_EXT) {
            (void)mgl_texture_apply_sub_image_2d_tight_named_ext(
                ctx, command->u.texture_sub_image_2d_ext.texture,
                command->u.texture_sub_image_2d_ext.target,
                command->u.texture_sub_image_2d_ext.level,
                command->u.texture_sub_image_2d_ext.xoffset,
                command->u.texture_sub_image_2d_ext.yoffset,
                command->u.texture_sub_image_2d_ext.width,
                command->u.texture_sub_image_2d_ext.height,
                command->u.texture_sub_image_2d_ext.format,
                command->u.texture_sub_image_2d_ext.data_type,
                command->u.texture_sub_image_2d_ext.data,
                command->u.texture_sub_image_2d_ext.data_size);
        } else if (command->type == MGL_LIST_CMD_TEXTURE_IMAGE_3D_EXT) {
            (void)mgl_texture_apply_image_3d_tight_named_ext(
                ctx, command->u.texture_image_3d_ext.texture,
                command->u.texture_image_3d_ext.target, command->u.texture_image_3d_ext.level,
                command->u.texture_image_3d_ext.internalformat,
                command->u.texture_image_3d_ext.width, command->u.texture_image_3d_ext.height,
                command->u.texture_image_3d_ext.depth, command->u.texture_image_3d_ext.border,
                command->u.texture_image_3d_ext.format, command->u.texture_image_3d_ext.data_type,
                command->u.texture_image_3d_ext.data, command->u.texture_image_3d_ext.data_size);
        } else if (command->type == MGL_LIST_CMD_TEXTURE_SUB_IMAGE_3D_EXT) {
            (void)mgl_texture_apply_sub_image_3d_tight_named_ext(
                ctx, command->u.texture_sub_image_3d_ext.texture,
                command->u.texture_sub_image_3d_ext.target,
                command->u.texture_sub_image_3d_ext.level,
                command->u.texture_sub_image_3d_ext.xoffset,
                command->u.texture_sub_image_3d_ext.yoffset,
                command->u.texture_sub_image_3d_ext.zoffset,
                command->u.texture_sub_image_3d_ext.width,
                command->u.texture_sub_image_3d_ext.height,
                command->u.texture_sub_image_3d_ext.depth,
                command->u.texture_sub_image_3d_ext.format,
                command->u.texture_sub_image_3d_ext.data_type,
                command->u.texture_sub_image_3d_ext.data,
                command->u.texture_sub_image_3d_ext.data_size);
        } else if (command->type == MGL_LIST_CMD_COPY_TEXTURE_IMAGE_2D_EXT) {
            glCopyTextureImage2DEXT(command->u.copy_texture_image_2d_ext.texture,
                                    command->u.copy_texture_image_2d_ext.target,
                                    command->u.copy_texture_image_2d_ext.level,
                                    command->u.copy_texture_image_2d_ext.internalformat,
                                    command->u.copy_texture_image_2d_ext.x,
                                    command->u.copy_texture_image_2d_ext.y,
                                    command->u.copy_texture_image_2d_ext.width,
                                    command->u.copy_texture_image_2d_ext.height,
                                    command->u.copy_texture_image_2d_ext.border);
        } else if (command->type == MGL_LIST_CMD_COPY_TEXTURE_SUB_IMAGE_2D_EXT) {
            glCopyTextureSubImage2DEXT(command->u.copy_texture_sub_image_2d_ext.texture,
                                       command->u.copy_texture_sub_image_2d_ext.target,
                                       command->u.copy_texture_sub_image_2d_ext.level,
                                       command->u.copy_texture_sub_image_2d_ext.xoffset,
                                       command->u.copy_texture_sub_image_2d_ext.yoffset,
                                       command->u.copy_texture_sub_image_2d_ext.x,
                                       command->u.copy_texture_sub_image_2d_ext.y,
                                       command->u.copy_texture_sub_image_2d_ext.width,
                                       command->u.copy_texture_sub_image_2d_ext.height);
        } else if (command->type == MGL_LIST_CMD_COPY_TEXTURE_SUB_IMAGE_3D_EXT) {
            glCopyTextureSubImage3DEXT(command->u.copy_texture_sub_image_3d_ext.texture,
                                       command->u.copy_texture_sub_image_3d_ext.target,
                                       command->u.copy_texture_sub_image_3d_ext.level,
                                       command->u.copy_texture_sub_image_3d_ext.xoffset,
                                       command->u.copy_texture_sub_image_3d_ext.yoffset,
                                       command->u.copy_texture_sub_image_3d_ext.zoffset,
                                       command->u.copy_texture_sub_image_3d_ext.x,
                                       command->u.copy_texture_sub_image_3d_ext.y,
                                       command->u.copy_texture_sub_image_3d_ext.width,
                                       command->u.copy_texture_sub_image_3d_ext.height);
        } else if (command->type == MGL_LIST_CMD_COPY_MULTI_TEX_IMAGE_2D) {
            glCopyMultiTexImage2D(
                command->u.copy_multi_tex_image_2d.texunit,
                command->u.copy_multi_tex_image_2d.target, command->u.copy_multi_tex_image_2d.level,
                command->u.copy_multi_tex_image_2d.internalformat,
                command->u.copy_multi_tex_image_2d.x, command->u.copy_multi_tex_image_2d.y,
                command->u.copy_multi_tex_image_2d.width, command->u.copy_multi_tex_image_2d.height,
                command->u.copy_multi_tex_image_2d.border);
        } else if (command->type == MGL_LIST_CMD_COPY_MULTI_TEX_SUB_IMAGE_2D) {
            glCopyMultiTexSubImage2D(command->u.copy_multi_tex_sub_image_2d.texunit,
                                     command->u.copy_multi_tex_sub_image_2d.target,
                                     command->u.copy_multi_tex_sub_image_2d.level,
                                     command->u.copy_multi_tex_sub_image_2d.xoffset,
                                     command->u.copy_multi_tex_sub_image_2d.yoffset,
                                     command->u.copy_multi_tex_sub_image_2d.x,
                                     command->u.copy_multi_tex_sub_image_2d.y,
                                     command->u.copy_multi_tex_sub_image_2d.width,
                                     command->u.copy_multi_tex_sub_image_2d.height);
        } else if (command->type == MGL_LIST_CMD_COPY_MULTI_TEX_SUB_IMAGE_3D) {
            glCopyMultiTexSubImage3D(command->u.copy_multi_tex_sub_image_3d.texunit,
                                     command->u.copy_multi_tex_sub_image_3d.target,
                                     command->u.copy_multi_tex_sub_image_3d.level,
                                     command->u.copy_multi_tex_sub_image_3d.xoffset,
                                     command->u.copy_multi_tex_sub_image_3d.yoffset,
                                     command->u.copy_multi_tex_sub_image_3d.zoffset,
                                     command->u.copy_multi_tex_sub_image_3d.x,
                                     command->u.copy_multi_tex_sub_image_3d.y,
                                     command->u.copy_multi_tex_sub_image_3d.width,
                                     command->u.copy_multi_tex_sub_image_3d.height);
        } else if (command->type == MGL_LIST_CMD_DRAW_SNAPSHOT) {
            mgl_draw_snapshot_replay(ctx, command->u.draw_snapshot);
        } else if (command->type == MGL_LIST_CMD_INIT_NAMES) {
            glInitNames();
        } else if (command->type == MGL_LIST_CMD_PUSH_NAME) {
            glPushName(command->u.name_value);
        } else if (command->type == MGL_LIST_CMD_POP_NAME) {
            glPopName();
        } else if (command->type == MGL_LIST_CMD_LOAD_NAME) {
            glLoadName(command->u.name_value);
        } else if (command->type == MGL_LIST_CMD_CALL_LIST) {
            replay_list(ctx, command->u.list_name);
        } else if (command->type == MGL_LIST_CMD_CALL_LISTS) {
            GLsizei j;
            for (j = 0; j < command->u.call_lists.count; ++j) {
                uint64_t list_sequence = ctx->errors.sequence;
                replay_list(ctx, ctx->list_base + command->u.call_lists.offsets[j]);
                if (ctx->errors.sequence != list_sequence)
                    break;
            }
        } else if (command->type == MGL_LIST_CMD_LIST_BASE) {
            ctx->list_base = command->u.list_base;
        }
        /* GL errors must not truncate replay; a
         * rejected command inside Begin/End must not swallow the paired End. */
    }
    --ctx->list_replay_depth;
    if (ctx->list_replay_depth == 0u)
        ctx->list_replaying = GL_FALSE;
    release_list(ctx->share_group, list);
}

static int valid_call_lists_type(GLenum type) {
    switch (type) {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
        case GL_INT:
        case GL_UNSIGNED_INT:
        case GL_FLOAT:
        case GL_2_BYTES:
        case GL_3_BYTES:
        case GL_4_BYTES:
            return 1;
        default:
            return 0;
    }
}

static GLuint call_lists_offset(GLenum type, const void *lists, GLsizei index) {
    switch (type) {
        case GL_BYTE:
            return (GLuint)(GLint)((const GLbyte *)lists)[index];
        case GL_UNSIGNED_BYTE:
            return (GLuint)((const GLubyte *)lists)[index];
        case GL_SHORT:
            return (GLuint)(GLint)((const GLshort *)lists)[index];
        case GL_UNSIGNED_SHORT:
            return (GLuint)((const GLushort *)lists)[index];
        case GL_INT:
            return (GLuint)((const GLint *)lists)[index];
        case GL_UNSIGNED_INT:
            return ((const GLuint *)lists)[index];
        case GL_FLOAT:
            return (GLuint)(GLint)((const GLfloat *)lists)[index];
        case GL_2_BYTES:
        case GL_3_BYTES:
        case GL_4_BYTES: {
            const unsigned bytes_per_offset =
                type == GL_2_BYTES ? 2u : (type == GL_3_BYTES ? 3u : 4u);
            const GLubyte *bytes = (const GLubyte *)lists + (size_t)index * bytes_per_offset;
            GLuint offset = 0u;
            unsigned j;
            for (j = 0u; j < bytes_per_offset; ++j)
                offset = (offset << 8u) | (GLuint)bytes[j];
            return offset;
        }
        default:
            return 0u;
    }
}

static void execute_call_lists(struct mgl_context *ctx, GLsizei count, const GLuint *offsets) {
    GLsizei i;
    for (i = 0; i < count; ++i) {
        uint64_t list_sequence = ctx->errors.sequence;
        replay_list(ctx, ctx->list_base + offsets[i]);
        if (ctx->errors.sequence != list_sequence)
            break;
    }
}

static void execute_call_lists_raw(struct mgl_context *ctx, GLsizei count, GLenum type,
                                   const void *lists) {
    GLsizei i;
    for (i = 0; i < count; ++i) {
        uint64_t list_sequence = ctx->errors.sequence;
        replay_list(ctx, ctx->list_base + call_lists_offset(type, lists, i));
        if (ctx->errors.sequence != list_sequence)
            break;
    }
}

GLuint glGenLists(GLsizei range) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_display_list *created = NULL;
    struct mgl_display_list *it;
    GLuint first = 1u;
    uint64_t last;
    GLsizei i;
    if (ctx == NULL)
        return 0u;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return 0u;
    }
    if (range < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return 0u;
    }
    if (range == 0)
        return 0u;

    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (;;) {
        GLuint conflict = 0u;
        last = (uint64_t)first + (uint64_t)(GLuint)range - 1u;
        if (last > UINT32_MAX) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            return 0u;
        }
        for (it = ctx->share_group->display_lists; it != NULL; it = it->next) {
            if (it->name >= first && (uint64_t)it->name <= last && it->name > conflict)
                conflict = it->name;
        }
        if (conflict == 0u)
            break;
        if (conflict == UINT32_MAX) {
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            return 0u;
        }
        first = conflict + 1u;
    }

    for (i = 0; i < range; ++i) {
        struct mgl_display_list *list = calloc(1, sizeof(*list));
        if (list == NULL) {
            while (created != NULL) {
                struct mgl_display_list *next = created->next;
                free(created);
                created = next;
            }
            (void)pthread_mutex_unlock(&ctx->share_group->lock);
            return 0u;
        }
        list->name = first + (GLuint)i;
        list->next = created;
        created = list;
    }
    while (created != NULL) {
        struct mgl_display_list *next = created->next;
        created->next = ctx->share_group->display_lists;
        ctx->share_group->display_lists = created;
        created = next;
    }
    atomic_store_explicit(&ctx->share_group->next_list_name,
                          last < UINT32_MAX ? (unsigned)(last + 1u) : 1u, memory_order_relaxed);
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return first;
}

void glNewList(GLuint list_name, GLenum mode) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_display_list *list;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->compiling_list != NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (list_name == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (mode != GL_COMPILE && mode != GL_COMPILE_AND_EXECUTE) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }

    list = calloc(1, sizeof(*list));
    if (list == NULL) {
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }
    list->name = list_name;
    ctx->compiling_list = list;
    ctx->list_compile_mode = mode;
    ctx->list_compile_failed = GL_FALSE;
}

void glEndList(void) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_display_list *list;
    struct mgl_display_list *replaced = NULL;
    struct mgl_display_list **link;
    GLboolean failed;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (ctx->compiling_list == NULL) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }

    list = ctx->compiling_list;
    failed = ctx->list_compile_failed;
    ctx->compiling_list = NULL;
    ctx->list_compile_mode = 0u;
    ctx->list_compile_failed = GL_FALSE;

    if (failed) {
        destroy_list(list);
        mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        return;
    }

    (void)pthread_mutex_lock(&ctx->share_group->lock);
    link = &ctx->share_group->display_lists;
    while (*link != NULL && (*link)->name != list->name)
        link = &(*link)->next;
    if (*link != NULL) {
        replaced = *link;
        list->next = replaced->next;
        *link = list;
        replaced->next = NULL;
        replaced->retired = GL_TRUE;
        if (replaced->replay_refs != 0u)
            replaced = NULL;
    } else {
        list->next = ctx->share_group->display_lists;
        ctx->share_group->display_lists = list;
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    destroy_list(replaced);
}

void glCallList(GLuint list_name) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_LIST_REPLAY);
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL)
        return;
    if (list_name == 0u) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_CALL_LIST;
    command.u.list_name = list_name;
    suppress = record_command(ctx, &command);
    if (suppress)
        return;
    replay_list(ctx, list_name);
}

void glListBase(GLuint base) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_list_command command;
    int suppress;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    memset(&command, 0, sizeof(command));
    command.type = MGL_LIST_CMD_LIST_BASE;
    command.u.list_base = base;
    suppress = record_command(ctx, &command);
    if (suppress)
        return;
    ctx->list_base = base;
}

void glCallLists(GLsizei n, GLenum type, const void *lists) {
    struct mgl_context *ctx = mgl_context_current();
    MGL_DEEP_SCOPE(ctx, MGL_DEEP_GL_LIST_REPLAY);
    struct mgl_list_command command;
    GLuint *offsets = NULL;
    GLsizei i;
    int owns_offsets = 1;
    int compiling;
    if (ctx == NULL)
        return;
    if (!valid_call_lists_type(type)) {
        mgl_error_set(&ctx->errors, GL_INVALID_ENUM);
        return;
    }
    if (n < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    if (n == 0)
        return;
    if (lists == NULL)
        return;

    compiling = ctx->compiling_list != NULL && !ctx->list_replaying;
    if (compiling && ctx->list_compile_failed) {
        if (ctx->list_compile_mode == GL_COMPILE)
            return;
        execute_call_lists_raw(ctx, n, type, lists);
        return;
    }

    if ((size_t)n > SIZE_MAX / sizeof(*offsets)) {
        if (compiling) {
            (void)compile_storage_failure(ctx);
            if (ctx->list_compile_mode == GL_COMPILE_AND_EXECUTE)
                execute_call_lists_raw(ctx, n, type, lists);
        } else {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        }
        return;
    }
    offsets = malloc((size_t)n * sizeof(*offsets));
    if (offsets == NULL) {
        if (compiling) {
            (void)compile_storage_failure(ctx);
            if (ctx->list_compile_mode == GL_COMPILE_AND_EXECUTE)
                execute_call_lists_raw(ctx, n, type, lists);
        } else {
            mgl_error_set(&ctx->errors, GL_OUT_OF_MEMORY);
        }
        return;
    }
    for (i = 0; i < n; ++i)
        offsets[i] = call_lists_offset(type, lists, i);

    if (compiling) {
        memset(&command, 0, sizeof(command));
        command.type = MGL_LIST_CMD_CALL_LISTS;
        command.u.call_lists.count = n;
        command.u.call_lists.offsets = offsets;
        if (!append_command(ctx, &command)) {
            free(offsets);
            (void)compile_storage_failure(ctx);
            if (ctx->list_compile_mode == GL_COMPILE_AND_EXECUTE)
                execute_call_lists_raw(ctx, n, type, lists);
            return;
        }
        owns_offsets = 0;
        if (ctx->list_compile_mode == GL_COMPILE)
            return;
    }

    execute_call_lists(ctx, n, offsets);
    if (owns_offsets)
        free(offsets);
}

void glDeleteLists(GLuint list_name, GLsizei range) {
    struct mgl_context *ctx = mgl_context_current();
    struct mgl_display_list *destroy_head = NULL;
    GLsizei i;
    if (ctx == NULL)
        return;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return;
    }
    if (range < 0) {
        mgl_error_set(&ctx->errors, GL_INVALID_VALUE);
        return;
    }
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    for (i = 0; i < range; ++i) {
        GLuint name = list_name + (GLuint)i;
        struct mgl_display_list **link = &ctx->share_group->display_lists;
        while (*link != NULL) {
            if ((*link)->name == name) {
                struct mgl_display_list *victim = *link;
                *link = victim->next;
                victim->next = NULL;
                victim->retired = GL_TRUE;
                if (victim->replay_refs == 0u) {
                    victim->next = destroy_head;
                    destroy_head = victim;
                }
                break;
            }
            link = &(*link)->next;
        }
    }
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    while (destroy_head != NULL) {
        struct mgl_display_list *next = destroy_head->next;
        destroy_head->next = NULL;
        destroy_list(destroy_head);
        destroy_head = next;
    }
}

GLboolean glIsList(GLuint list_name) {
    struct mgl_context *ctx = mgl_context_current();
    GLboolean result;
    if (ctx == NULL)
        return GL_FALSE;
    if (ctx->immediate.active) {
        mgl_error_set(&ctx->errors, GL_INVALID_OPERATION);
        return GL_FALSE;
    }
    if (list_name == 0u)
        return GL_FALSE;
    (void)pthread_mutex_lock(&ctx->share_group->lock);
    result = find_list(ctx->share_group, list_name) != NULL ? GL_TRUE : GL_FALSE;
    (void)pthread_mutex_unlock(&ctx->share_group->lock);
    return result;
}

void mgl_display_list_abort_compile(struct mgl_context *ctx) {
    if (ctx == NULL || ctx->compiling_list == NULL)
        return;
    destroy_list(ctx->compiling_list);
    ctx->compiling_list = NULL;
    ctx->list_compile_mode = 0u;
    ctx->list_compile_failed = GL_FALSE;
}

void mgl_display_list_free_all(struct mgl_share_group *group) {
    struct mgl_display_list *list;
    if (group == NULL)
        return;
    list = group->display_lists;
    while (list != NULL) {
        struct mgl_display_list *next = list->next;
        destroy_list(list);
        list = next;
    }
    group->display_lists = NULL;
}
