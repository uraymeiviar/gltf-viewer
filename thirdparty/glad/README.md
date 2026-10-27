# glad (OpenGL loader)

Generated with glad 0.1.36 (C/C++ generator, `gl` spec, core profile, OpenGL 4.6, all extensions so
the existing `GLAD_GL_*` checks keep working). The spec is downloaded by the generator from the Khronos
OpenGL-Registry at generation time, so regeneration needs network access.

```cmd
pip install glad==0.1.36
python -m glad --profile core --api gl=4.6 --generator c --spec gl --out-path <tmp>
```

Copy `<tmp>/include/glad/glad.h`, `<tmp>/include/KHR/khrplatform.h` and `<tmp>/src/glad.c` over the files in
this folder. `--extensions` is omitted on purpose (omitted = every extension). Generated 2026-10-28; the
previous version was generated for `gl=4.1`.

Glider requests a 4.6 core context and falls back to 4.5, 4.3 and 4.1 (see `Application::run`), so the
loader exposes 4.6 entry points that are null on older contexts: check `ux3d::imgui::glCaps()` before
using compute shaders, SSBOs or image load/store.
