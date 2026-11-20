# Diagnostics and profiling

Runtime diagnostics write to standard error. Set their environment variables before initializing ManifestGL.

| Variable | Effect |
|---|---|
| `MGL_GLES_TRACE=1` | Wrap all loaded GLES entry points and report the first observed native error with a 24-call history |
| `MGL_GLES_TRACE=all` | Continue reporting native errors with logarithmic rate limiting; include the call history on the first error |
| `MGL_PERF_DIAG=1` | Collect frame counters and CPU timing scopes |

## Interface errors and warnings

Interface validation failures are reported as `event=interface_error_set` with the error code, actual rejecting helper, source file and line, and the prior pending interface error. `pending` is the error already retained before the new rejection; subsequent calls to `glGetError()` consume errors according to the normal mGL error-state rules. The rejecting helper can differ from the application's exported API function.

Warnings such as `event=lowering_skip` report the operation and reason supplied by the corresponding code path.

## Native GLES error tracing

The `MGL_GLES_TRACE` tracing mode installs automatically generated wrappers for all dispatch-table GLES entry points except `glesGetError` and the mGL framebuffer-zero adapter. It routes internal `glesGetError` calls through the queued native errors before polling the GLES driver. Calls to the underlying framebuffer binding function are traced.

On the first error the tracer emits `event=gles_origin`, followed by up to 24 `event=gles_history` records with the native function, argument values, the caller PC and its library-relative offset. `phase=generated_by_call` means the error appeared in the post-call check after pre-existing errors had been drained. `phase=inherited_before_call` means the error was already pending before the named call; the named call is not identified as the cause.

The caller `module` and `offset` can be resolved against an unstripped `libGL` with `llvm-addr2line`. Tracing is intended for investigation and performs two or more additional native error checks per dispatched call. Native calls made while no mGL context is current are not attributed.

The dispatch wrappers are generated from `src/native/dispatch.h` into `src/native/dispatch_trace.generated.inc`.

```sh
python3 tools/generate_gles_trace.py
```

Regenerate after modifying the native GLES dispatch table or the generator. `python3 tools/generate_gles_trace.py --check` verifies that the generated file matches the current declarations; the `gles_trace_generated` CTest performs the same check when Python is available.

## Frame timing

`src/core/render_diag.c` emits `MGL_PERF_DIAG` output after the first presented frame and at subsequent 30-frame intervals. Records include draw paths, buffer staging, state-cache activity, shader/program work, uploads, and CPU time spent submitting and pacing frames.

With `MGL_PERF_DIAG=1`, the instrumentation also emits `MGL_DEEP_DIAG` scope records. `total_ms` includes nested calls; `self_ms` excludes child scope duration. Diagnostic scopes measure CPU time.

Texture-upload scope names include `gl_tex_image_2d`, `tex_unpack`, `tex_color_transform`, `tex_image_apply_2d`, `gles_tex_image_2d`, `tex_upload_begin`, `tex_upload_end`, `gl_generate_mipmap`, and `gles_generate_mipmap`.

## Investigating failures

- Native initialization: inspect provider-loading failures and the GLES version reported at context binding. Provider selection is described in [Architecture](ARCHITECTURE.md#provider-dispatch).
- API errors: capture the failing operation and its `GL_INVALID_*` or native GLES error output.
- Surface failures: capture acquisition, replacement, frame submission, and retirement records from `src/presentation/`.
- Performance: compare timing captures from equivalent scenes, frame counts, provider configurations, and presentation modes.
