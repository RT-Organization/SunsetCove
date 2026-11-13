# FrameworkPatches — CoreFramework changes to commit in FrameWorks

Files in `CoreFramework/` mirror their path inside the submodule (`Plugins/CoreFramework/`). They are **already applied** in the
working copy of the submodule (the project was built and tested with them); copy them into the FrameWorks repository
(`coreframework-branch`), commit, then update the submodule pointer in SunsetCove.

Authoritative list: `git -C Plugins/CoreFramework status --porcelain`.

## 2026-11-12 — InputPromptWidget: `SetCollapseWhenNoKey`

| File | Change |
|---|---|
| `Source/CoreFramework/Public/UISystem/Input/InputPromptWidget.h` | new `UFUNCTION(BlueprintCallable) void SetCollapseWhenNoKey(bool bCollapse)` |
| `Source/CoreFramework/Private/UISystem/Input/InputPromptWidget.cpp` | its implementation: sets `bCollapseWhenNoKey`, makes the prompt visible again when the rule is turned off, refreshes |

**Why.** `bCollapseWhenNoKey` was a protected, edit-only property. Prompts created at runtime by a list (SunsetCove's HUD
interaction prompt builds one `UInputPromptWidget` row per available action) had no way to turn it on, so a row whose action has
no key on the current device (for example an action mapped on keyboard only) showed a label without a key. Backward compatible:
no existing behavior changes, nothing else in the framework calls it.

## 2026-11-12 — AudioFrameSubsystem: `GetHandleComponent`

| File | Change |
|---|---|
| `Source/CoreFramework/Public/AudioSystem/Manager/AudioFrameSubsystem.h` | new public C++ function `UAudioComponent* GetHandleComponent(FAudioHandle Handle) const` (inline, returns the existing private `ResolveHandle`) |

**Why.** The handle functions cover volume, pitch and parameters, but not what only the Audio Component can do (an attenuation
set by code, a submix send). SunsetCove's sea gives its looping waves sound a built-in shore attenuation when the cue has none,
which needs the component. Header only, no behavior change, nothing else calls it.

## 2026-11-13 — Audio: the Master slider had no effect (bug fix)

| File | Change |
|---|---|
| `Source/CoreFramework/Private/AudioSystem/Manager/AudioFrameSubsystem.cpp` | `ApplyCategoryVolume`: `SetSoundMixClassOverride(..., bApplyToChildren = true)` (was `false`), comment rewritten |
| `Docs/AudioSystem/Audio_Architecture_Overview.md` | volume routing steps 3-4 describe the fix |
| `Docs/AudioSystem/Audio_Engine_Setup.md` | "Why a hierarchy" line corrected |

**Why.** The audio device first resets every Sound Class to its asset properties and propagates them down the class tree
(`FAudioDevice::ParseSoundClasses`), and only afterwards applies the sound mix adjusters (`ApplyClassAdjusters`). An adjuster
without "apply to children" changes only its own class, so the Master adjuster changed `SC_Master` alone — and no sound plays in
`SC_Master` itself (every sound is in SFX / Music / Ambient / UI). With `true`, each category adjuster applies to its class and
its children: Master multiplies into every category exactly once (each category has one adjuster on its own class, so nothing is
counted twice). Verified against the UE 5.8 source; a project whose categories are not children of Master is unaffected.
