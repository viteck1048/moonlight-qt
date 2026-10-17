# Fork notes — Viktor's moonlight-qt fork

Personal changes on top of **moonlight-stream/moonlight-qt**.
Last synced with upstream @ **b0af84ea** (2026-10-14, 110 commits; previous sync: ca7d61f5 via
`a4bd097e`). Branches: `master` = pure upstream mirror, **`custom-keymap` = the fork**.

This file is the **rebase/merge survival guide**. The README's "Fork Features" section
describes *what* the features do; this file lists *where they hook into upstream* and the
*conflict hazards*, so a future upstream sync doesn't eat a day.

There is also a sibling [`FORK_NOTES.md` in the Sunshine fork] with the same format.

## How to sync with upstream
```
git fetch upstream
git merge upstream/master      # this fork uses MERGE (see a4bd097e), not rebase
# or, to inspect exactly what's ours before resolving:
git diff upstream/master...HEAD --stat
```
Resolve conflicts using the "Integration points" table below — the new files never
conflict; only the ~12 touched upstream files do, each at a small, known spot.

## Custom commits (most recent sync)
- `a1548c8e` Add custom key remapping functionality with auto-detect UI (the big one)
- `553b41fb` Change display cycling to Ctrl+Alt+Shift+O (avoid Sunshine hotkey clash)
- `a6bfe49c` Corrected native↔SDL mappings (Win/Linux, 102-key EU keyboard)
- `63edcfdf` "qqqq" — junk/WIP commit, ignore / squash on next cleanup

- 2026-10-14 sync notes: upstream moved `raiseAllKeys()` → keyboard.cpp and
  `notifyMouseLeave()` → mouse.cpp; the only code conflict was those two sitting next to our
  functions at the end of input.cpp (keep ours, drop theirs). Upstream also vendored
  h264bitstream (no longer a submodule) and moonlight-common-c gained the `nanors` submodule.

## Ukrainian translation (`app/languages/qml_uk.ts`)
Upstream now has its own Ukrainian via Weblate, so this file conflicts on every sync. Don't
resolve hunk by hunk: take upstream's file, run
`/usr/lib64/qt6/bin/lupdate -no-obsolete app/app.pro -ts app/languages/qml_uk.ts` (adds our
strings), fill/review the translations, then `lrelease` the same file (the .qm is committed).
Terminology: pairing = «сполучення», host PC = «хост-ПК», GPU = «відеокарта», input = «введення».

---

## New files (ours — these never conflict, just re-add to the build)
- `app/streaming/input/userkeycombos.{h,cpp}` — core key-combo config + runtime model.
  `UserKeyComboManager` singleton (`initialize()` in main.cpp); types `UserKeyComboHx`,
  `UserKeyKShHx`.
- `app/streaming/input/keyboardmapping.{h,cpp}` — `LikeSdlScancodeMapper` singleton: SDL
  scancode ↔ native scancode + SDL_Keymod ↔ string translators (the big AI-generated tables;
  per README still needs cross-platform testing).
- `app/gui/userkeycombobridge.{h,cpp}` — `UserKeyComboBridge`, the QML <-> config bridge.

If upstream rewrites the input stack, these stay valid as long as `SdlInputHandler` still
sees raw `SDL_KeyboardEvent`s.

---

## Integration points in UPSTREAM files (the conflict-prone edits)

| File | What we add | Hazard on sync |
|---|---|---|
| `app/app.pro` | adds the 3 new `.cpp` to `SOURCES` and 3 new `.h` to `HEADERS` | conflicts only if upstream reorders these lists |
| `app/main.cpp` | includes 3 headers; `LikeSdlScancodeMapper::instance().initialize()` + `UserKeyComboManager::instance().initialize()` after SDL init; `qmlRegisterType<UserKeyComboBridge>(...)` near the other `qmlRegisterType` calls | upstream churns main.cpp init order — re-place these two init calls after SDL is up |
| `app/settings/streamingpreferences.{h,cpp}` | two prefs: `pointerRegionLockActive`, `userCombosEnabled` (Q_PROPERTY + member + signal + load/save) | upstream adds prefs in the same blocks → 3-way but trivial |
| `app/streaming/input/input.h` | **`SdlInputHandler` ctor gains 2 params**: `const QVector<UserKeyComboHx>& userCombos, int initialDisplayCount`. New `KeyCombo` enum values: `KeyComboToggleUserCombos`, `KeyComboSetDsplLen`, `KeyComboNextDsplView`. New methods `setUserComboEnabled/userComboEnabled`, `applyUserKeyCombo`, `matchUserKeyBinding`, `playbackUserKeyCombo`. New members `m_UserKeyCombos`, `m_UserCombosEnabled`, `m_IsProcessingUserCombo`, `dspl_view`, `dspl_len` | **highest-risk file.** Ctor signature change ripples to every `new SdlInputHandler(...)`. The `KeyCombo` enum is where upstream also adds entries → keep ours before `KeyComboMax` |
| `app/streaming/input/input.cpp` (+239) | impl of the above: parse combos, `performSpecialKeyCombo` cases for the new enum values (toggle combos / set display length / next display view / pointer region lock), ctor stores combos + display count | conflicts where upstream edits the special-combo switch and ctor; re-attach our cases |
| `app/streaming/input/keyboard.cpp` (+62) | call `applyUserKeyCombo(event)` in the key-event path; synthetic event playback; **disabled in game/relative mouse-capture mode** | re-attach the `applyUserKeyCombo` hook inside upstream's keyboard handler |
| `app/streaming/session.{h,cpp}` | `Session` ctor gains `ComputerManager* computerManager = nullptr`; member `m_ComputerManager`; `updateClientDisplayCount(int)`; passes `userCombos` + `clientDisplayCount` into `SdlInputHandler` | ctor signature change → see callers below |
| `app/backend/nvcomputer.{h,cpp}` | `int clientDisplayCount = 1;` field + its (de)serialization and `isEqualSerialized()` | upstream adds fields in the same spots; remember the serialize + isEqualSerialized triplet |
| `app/cli/startstream.cpp`, `app/gui/appmodel.cpp`, `app/gui/computermodel.cpp` | updated `new Session(...)` calls to pass `m_ComputerManager` (and `nullptr` prefs where needed) | only conflict if upstream changes the Session ctor too |
| `app/gui/SettingsView.qml` (+632) | the whole remapping UI (key-capture/auto-detect, keymap.xml edit/save) + toggles for the new prefs | biggest QML diff; upstream restructures SettingsView often → expect to re-stitch our `Item/GroupBox` blocks. Ours are self-contained sections, search for `UserKeyComboBridge` / `pointerRegionLockActive` |

### Hotkeys added (live in input.cpp `performSpecialKeyCombo` + the enum)
- `Ctrl+Alt+Shift+U` — toggle custom user combos.
- `Ctrl+Alt+Shift+O` / `...+P` — host display cycling (set length / next view). **O was chosen
  to avoid clashing with Sunshine's hotkeys** (commit 553b41fb) — keep it off Sunshine's set.
- Pointer-region-lock toggle (persisted via `pointerRegionLockActive`).

---

## Build (Fedora VM — see also memory [[moonlight-fork-build]], [[no-local-build]])
Plain qmake, **no RPM** (that's the Sunshine fork). On the Fedora build VM:
```
git submodule update --init --recursive
qmake6 moonlight-qt.pro
make -j$(nproc)            # binary -> app/moonlight
```
Then copy just `app/moonlight` to `~/moonlight-qt/` on Bazzite (that folder holds only the
binary + icon). After editing `app.pro` you MUST re-run `qmake6`, not just make.

**Fedora ffmpeg gotcha:** rpmfusion ships ffmpeg headers under `/usr/include/ffmpeg/`. Do NOT
hardcode that path — pkg-config already handles it (`libavcodec.pc` etc. set
`includedir=/usr/include/ffmpeg`, and upstream's `packagesExist(libavcodec){ PKGCONFIG += ... }`
adds the `-I` globally). If you see missing `libavcodec/...` headers, fix pkg-config, not app.pro.

---

## Re-apply checklist after an upstream sync
1. Re-add new files to `app/app.pro` (SOURCES + HEADERS) if the merge dropped them.
2. Re-place the two `initialize()` calls + `qmlRegisterType<UserKeyComboBridge>` in `main.cpp`.
3. Fix `SdlInputHandler` and `Session` ctor call sites (startstream/appmodel/computermodel).
4. Re-attach `applyUserKeyCombo` in `keyboard.cpp` and the new `performSpecialKeyCombo` cases.
5. Re-stitch the `SettingsView.qml` blocks (search `UserKeyComboBridge`).
6. `qmake6 && make -j$(nproc)`; verify: Settings shows the remap UI, Ctrl+Alt+Shift+O cycles
   host displays, Ctrl+Alt+Shift+U toggles custom combos.

## Release packages (fork-only)

Fork-only files, no upstream counterparts (no merge conflicts expected):
- `.github/workflows/release-fork.yml` — push a tag `v<upstream>-keymap.<n>` (e.g. `v6.2.0-keymap.1`)
  to build Windows (setup .exe + portable zips, x64/arm64), AppImage, `.rpm` (Fedora 44) and `.deb`
  (Ubuntu 24.04 and Debian 13), and attach them to a **draft** GitHub release. Pushing branch `release-ci` runs the
  builds without creating a release.
- `packaging/rpm/moonlight-qt-keymap.spec`, `packaging/deb/build-deb.sh`.

The workflow copies the Windows steps from upstream `build-win-mac.yml`; if upstream changes the Qt
version or the Windows build steps there, mirror the change here.
