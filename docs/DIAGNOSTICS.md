# Diagnostics and profiling

Runtime diagnostics write to standard error. Set their environment variables before initializing ManifestGL; the diagnostic enable flags are cached during use.

| Variable | Effect |
|---|---|
| `MGL_BACKEND_ERROR_CHECK=1` | Poll GLES errors at instrumented native operation sites, queueing them for OpenGL error reporting |
| `MGL_PERF_DIAG=1` | Collect frame counters and CPU timing scopes |

## Frame timing

`src/core/render_diag.c` emits `MGL_PERF_DIAG` output after the first presented frame and at subsequent 30-frame intervals. Records include draw paths, buffer staging, state-cache activity, shader/program work, uploads, and CPU time spent submitting and pacing frames.

With `MGL_PERF_DIAG=1`, the instrumentation also emits `MGL_DEEP_DIAG` scope records. `total_ms` includes nested calls; `self_ms` excludes child scope duration. Diagnostic scopes measure CPU time.

Texture-upload scope names include `gl_tex_image_2d`, `tex_unpack`, `tex_color_transform`, `tex_image_apply_2d`, `gles_tex_image_2d`, `tex_upload_begin`, `tex_upload_end`, `gl_generate_mipmap`, and `gles_generate_mipmap`.

## Investigating failures

- Native initialization: inspect provider-loading failures and the GLES version reported at context binding. Provider selection is described in [Architecture](ARCHITECTURE.md#provider-dispatch).
- API errors: capture the failing operation and its `GL_INVALID_*` or native GLES error output.
- Surface failures: capture acquisition, replacement, frame submission, and retirement records from `src/presentation/`.
- Performance: compare timing captures from equivalent scenes, frame counts, provider configurations, and presentation modes.
