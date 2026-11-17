# Graphics compatibility

ManifestGL exposes an experimental desktop OpenGL 4.6 interface over an OpenGL ES 3.2 provider. The reported version and extensions do not establish complete desktop OpenGL conformance. This page records translation trade-offs, provider-dependent features, and known unsupported behavior. For implementation details, see [Architecture](ARCHITECTURE.md); for test coverage, see [Testing](TESTING.md).

## Floating-point texture storage

When the provider lacks `GL_OES_texture_float_linear`, mGL intentionally backs `GL_R32F`, `GL_RG32F`, `GL_RGBA32F`, and `GL_RGB32F_ARB` textures with 16-bit floating-point storage to retain native linear filtering. Precision and representable range are reduced, while texture queries retain the requested desktop internal format. With that extension, 32-bit floating-point storage is used. This is an intentional filtering-first policy, including for immutable storage.

## Texture data and views

CPU-side upload shadows retain client-format values and may not reflect the quantization of the texture's native internal format. For example, `GL_FLOAT` values uploaded to `GL_RGBA8` can be returned without the eight-bit quantization applied by native storage.

`glTextureView` requires a native `GL_EXT_texture_view` or `GL_OES_texture_view` extension, the corresponding entry point, and native immutable storage for the original texture. Views are unavailable when any of these requirements is missing.

A cube-map-array view of a cube-map source is not implemented. Views of cube-map arrays backed by compatible array storage require the native texture-view extension.

Signedness-changing views of software-decompressed RGTC and BC6H textures are unavailable. They require native compressed storage and texture-view support. CPU-side readback of uncompressed texture views does not fully support bitwise reinterpretation between compatible internal formats.

## Pixel operations

Depth-only and stencil-only `glDrawPixels` operations are not implemented.

Depth and stencil `glReadPixels` operations require the corresponding native `GL_NV_read_depth`, `GL_NV_read_stencil`, or `GL_NV_read_depth_stencil` capability. Without the matching extension, those transfers are unavailable.

The legacy imaging `glGetn*` operations for color tables, convolution, histograms, minmax, and separable filters are unimplemented.

## Image binding and texture barriers

`glBindImageTexture` does not support mutable textures. For `GL_RGBA32F` textures backed by `GL_RGBA16F` under the filtering-first policy, image binding uses the native 16-bit format while the public binding query reports the requested desktop format.

`glTextureBarrier` requires a native `GL_NV_texture_barrier`, `GL_EXT_texture_barrier`, or `GL_ARB_texture_barrier` extension and its entry point. Without a matching native capability, the barrier is unavailable.

## Query limitations

`GL_SAMPLES_PASSED` uses a native `GL_ANY_SAMPLES_PASSED_CONSERVATIVE` boolean query. mGL reports `4096` for a nonzero result; this is a compatibility approximation, not a measured sample count.

Only vertex stream 0 is guaranteed by GLES 3.2. `GL_MAX_VERTEX_STREAMS` reports 1 without native `GL_ARB_transform_feedback3`; multiple streams require that extension and its native indexed-query operations. Desktop OpenGL 4.x ordinarily requires more streams.

Multiple indexed viewports require native `GL_OES_viewport_array`. Without it, only the single native viewport is available, and fractional viewport origins cannot be preserved.

`GL_TIME_ELAPSED` and `GL_TIMESTAMP` require `GL_EXT_disjoint_timer_query` and the relevant native query entry points. Without them, timer queries are unavailable and `GL_QUERY_COUNTER_BITS` reports zero for those targets.

## Shader features

Double-precision shader uniforms (`glUniform*d`, `glProgramUniform*d`, and their matrix variants) and double-precision vertex attribute APIs (`glVertexAttribL*`) are not implemented. Shader subroutine queries and `glUniformSubroutinesuiv` are also unavailable.

`glGetProgramResourceiv` supports `GL_LOCATION_INDEX` for program outputs using native `GL_EXT_blend_func_extended` reflection when available, or the tracked fragment-output index fallback otherwise. The fallback depends on native output-name reflection and the linked output bindings. GLES 3.2 does not expose desktop `GL_LOCATION_COMPONENT` reflection, so component-location metadata is not generally available.

## Legacy accumulation

No accumulation buffer is exposed. Accumulation component sizes are zero, and `glAccum` operations are unavailable.

## Extension-dependent state and storage

`glClipControl` requires native `GL_EXT_clip_control` and its entry point. Persistent buffer storage and mapping require `GL_EXT_buffer_storage` and the native mapping operations. These features are unavailable without the required native capabilities.

`GL_NV_fence` is advertised only when native GLES sync creation, waiting, and deletion are available. Its fence names are local to each context.

`GL_MULTISAMPLE` enable state is tracked for desktop compatibility, but toggling it cannot disable multisample rasterization on the OpenGL ES provider.
