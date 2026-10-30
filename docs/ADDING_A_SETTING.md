# Adding a setting

Every setting is one `SettingSpec` in the registry (`header/imgui/CommandRegistry.h`). The registry generates
`set_*` / `get_*`, `list_settings`, `describe`, value coercion with range/enum checks and the "Settings reference" table
of `Glider.exe --agent-guide`. Everything else is manual. In particular `SettingSpec::cliFlag` is display text only:
the flag needs its own parser branch. Decide the scope first ([SETTINGS_SCOPES.md](SETTINGS_SCOPES.md)).

| The value is... | Scope | State | Register in |
|---|---|---|---|
| how one view looks (helper, overlay, ground) | Viewport | `ViewportSettingsState` (`ViewportSettingsPanel.h`) | `CommandsSettingsViewport.cpp` |
| how the scene looks, or an effect on/off | Environment | `SceneEnvironmentState` (`SceneEnvironment.h`); effects `SceneEffectSettings` (`SettingsScopes.h`) | `CommandsSettingsEnvironment.cpp`; effects `CommandsSettings.cpp` |
| costs time/memory, or is a saved preference | Renderer | `RenderQualitySettings` (`RenderQuality.h`) | `CommandsSettingsQuality.cpp` |
| which glTF extensions apply | Renderer (app) | `GltfExtensionSettings` | `CommandsSettings.cpp` (`extensionSwitch`) |

## A. Viewport view setting (traced: `wireframe_blend`)

1. Field with default in `ViewportSettingsState` (`ViewportSettingsPanel.h:73`).
2. Register in `registerAgentViewportSettings` (`CommandsSettingsViewport.cpp:339`). The default comes from a default-constructed
   struct. Helpers `boolSetting<VS>`, `floatSetting<VS>`, `indexEnumSetting<VS>` (`CommandsInternal.h`); otherwise `get`/`set` lambdas:
   ```cpp
   addSetting(boolSetting<VS>("force_double_sided", V, &VS::forceDoubleSided, "Disable back-face culling.", "--force-double-sided"));
   ```
3. CLI: one branch in `Application::configure` (`CommandLine.cpp:398`):
   ```cpp
   } else if (arg == "--wireframe-blend" && i + 1 < argc) {
       if (!applyCliSetting(SettingScope::Viewport, "wireframe_blend", toLowerAscii(argv[++i]), "--wireframe-blend")) return 1;
   ```
4. UI: widget in `ViewportSettingsPanel.cpp:182`, then `revisions.mark(SceneChange::ViewportSettings);`. The registry path marks it
   and resets that viewport's TAA history itself (`setSetting`, `CommandsHandlers.cpp`). If the renderer reads a value resolved from
   several scopes, mirror it in `RenderQuality::resolveViewport`; a plain view field is read directly.
5. `get_state`: not needed, `get_viewport_setting` returns it. Add to `CommandsState.cpp` / `DiagnosticsFrame.cpp` only if agents need it in the snapshot.
6. Test: `validate_agent_control.py` (the `wanted` key list, a set/get round trip) and the area validator (`validate_wireframe_overlay.py`).
   A per-viewport override of a scene effect is `overrideSetting` plus `validate_settings_scopes.py`.

## B. Scene / environment setting (traced: `sun_disk_scale`)

1. Field in `SceneEnvironmentState` (here `sunDisk.scale`, `SunDisk.h:29`), or in `SceneEffectSettings` for an effect (then also
   `resolveViewport` if the renderer reads it per viewport).
2. Register in `registerAgentEnvironmentSettings` (`CommandsSettingsEnvironment.cpp:524`; effects use `effectBool`/`effectFloat`).
   Give `hasRange`/`minimum`/`maximum`: out-of-range values are then refused with `bad_value`.
   ```cpp
   addSetting(floatSetting<ES>("ambient_fill", E, &ES::ambientFill, "Auto ambient fill fraction k (0..1).",
                               "--ambient-fill <k>", std::pair{0.0, 1.0}));
   ```
3. CLI: branch with `SettingScope::Environment` (`CommandLine.cpp:675`). `applyCliSetting` records it in `cliEnvironmentSettings`, which
   is replayed onto every scene after the model load (`CommandLine.cpp` ~1201); nothing more to do.
4. UI: `SceneInspector.cpp` (`renderSunDisk`, `:818`), then `revisions.mark(SceneChange::RenderSettings);`. The registry path marks
   `RenderSettings` and resets TAA on all viewports. Add `UiProbe::item("scene.sun_disk.scale", 0)` for scripted UI checks.
5. `get_state`: hand-written blocks in `CommandsState.cpp` (`sun.disk`, `:292`); add the field if it explains a rendered result.
   Not persisted: the environment resets on a model open (effects survive).
6. Test: the area validator (`validate_sun_disk.py`: `session.env(...)`, refused values, CLI equals pipe). A new area gets a
   `validate_<area>.py` listed in `run_regression.py`.

## C. Quality / saved setting (traced: `shadow_resolution`)

1. Field in `RenderQualitySettings` (`RenderQuality.h:58`); in the preset defaults if presets carry it (`defaultProfile`,
   `RenderQuality.cpp:61`); in `effective()` if Auto adapts it (`:666`).
2. Register in `registerAgentRendererSettings` (`CommandsSettingsQuality.cpp`, `shadowSpec`): `persisted = true`, `defaultValue` by hand
   (Renderer scope has no target to default-construct), `get`/`set` through `RenderQuality::settings()`, and the matching `RenderQuality::pin*()`
   so Auto quality does not override an explicit value. `persisted` makes a pipe set require `set_persist(true)`, then it calls `RenderQuality::save()`.
3. CLI: not in `CommandLine.cpp`. `RenderQuality::parseArgument` (`RenderQuality.cpp:913`) parses `--shadow-*`, `--ibl-*`, `--grid-mode`...
   before `configure`. `applyCliSetting` is only for flags that go through the registry.
4. UI: File > Settings > Quality, `RenderQuality::drawSettings` (`:1201`, `sizeCombo("Shadow Map Size", ...)`), and the Presets tab
   (`:568`) if per preset. Mark `SceneChange::RenderSettings` and call `save()` after a change.
5. Persistence, by hand: read in `readRenderQuality` (`:315`) and `readQualityProfiles` (`:259`); write in `RenderQuality::save` (`:880`
   profile keys, `:900` `[RenderQuality]`) **only when different from the default**; validate with `setChecked`/`valid*`. `--isolated`
   is handled by `save()`/`load()`; use `SettingsFile` only, never a new file.
6. `get_state`: `qualityStateJson` (`CommandsState.cpp:138`).
7. Test: `validate_settings_scopes.py` (`check_persistence`: `--isolated` writes nothing, defaults leave no key) and the area validator
   (`validate_shadows.py`). A new `settings.ini` section also needs `validate_settings_storage.py`.

An **app setting** (glTF extensions) is the same registry entry with `spec.onPersist = [name] { GES::instance().commit(name); }`;
its class owns the `SettingsFile` section (`GltfExtensionSettings::load/save`) and CLI/pipe edits stay session-only unless `set_persist(true)`.

## Always

- The help text is the agent's documentation: unit, range, default. The CLI accepts exactly what the registry accepts.
- Document the flag in [CLI_REFERENCE.md](CLI_REFERENCE.md); explain non-obvious semantics in `resources/agent/GLIDER_AGENT_GUIDE.md`
  (embedded: rebuild). Do not copy the generated key table.
- Run `python scripts/check_settings_docs.py` (needs a built `Glider.exe`, opens no window): every registered key and every flag parsed in
  the CLI sources must appear in the docs; a registry `cli` flag must have a parser.
- A renamed key keeps working as a `forward(...)` entry in `registerAgentCompatibilitySettings`.
- Test proportionally ([BUILD_AND_TEST.md](BUILD_AND_TEST.md)): your feature check plus the closest 1-3 validators named above.
