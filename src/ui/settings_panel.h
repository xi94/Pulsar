#pragma once

#include "core/settings.h"
#include "gfx/font_manager.h"
#include "ui/color_picker.h"
#include "ui/draggable.h"
#include "ui/scrollable.h"
#include "ui/text_input.h"
#include "ui/tooltip.h"
#include "ui/widget.h"

class CAssetManager;
class CWindow;
class IRenderer;

// The settings dialog: a centred, size-clamped panel listing every setting this app has,
// grouped under section headings.
//
// Every setting here takes effect live rather than being stored for some later sync step -
// this widget edits the live Settings object directly, and the font, animation and roundness
// controls apply their global side effects on the spot.
//
// Rows are laid out by walking one splitting cursor down a scrollable, clipped region, so a
// row can never overlap another or get squeezed however many exist.
//
// Every row with a shipped default also has a reset button just left of its own control - a
// setting nobody remembers the default of is a setting nobody dares change. Three things
// animate around it, all for legibility: the button fades in only while its row is off its
// default, so its presence is the signal; the icon spins a full turn on click; and the value
// travels back rather than snapping, which is what the eased display copies below exist for.
// The settings themselves are still written immediately.
//
// Master Password is the one row that edits nothing here. Resetting one means typing a new
// password twice with a live typo check, which the unlock screen's setup mode already does -
// so this row just closes the panel and asks the owner to activate that.
//
// This widget does not know persistence exists: a consumed pointer or key event doubles as
// "something changed, save now", so no separate flag is needed.

/// Every row that can be restored to its shipped default, and the index into the per-row reset
/// animation arrays. Master Password is absent: "default" for a password would
/// mean no password, which is not a state this app has.
enum class ESettingsResetTarget : u8 {
	Font,
	FontSize,
	SecondaryFontSize,
	Accent,
	CornerRoundness,
	Animations,
	AnimationSpeed,
	Notifications,
	ExcludeFromCapture,
	BlockOverlayInjection,
	CloseToTray,
	Count,
};

class CSettingsPanel : public CWidget {
  public:
	/// window supplies the current size and DPI scale that a font re-bake needs, since the
	/// fixed widget signatures have no room for per-call dimensions. settings is the live
	/// object this panel edits.
	CSettingsPanel(CFontManager *pFonts, Settings *pSettings, const CWindow &window, IRenderer *pRenderer,
				   const CAssetManager &assets);

	/// Touches no field state, so reopening always resumes where it left off.
	void Open();

	/// Clears the transient state that should not survive a reopen - a focused field, an open
	/// picker, a hanging tooltip - which would otherwise read as a stale glitch.
	void Close();

	void Update(float deltaSeconds) override;
	void Draw(CDrawList &drawList) override;

	/// Pointer down and move exist so the colour picker, the scrollbar thumb and the two
	/// sliders can all be dragged.
	bool OnPointerDown(float x, float y) override;
	bool OnPointerMove(float x, float y) override;

	/// Ends any drag first; if none was in progress, handles the release as a real click.
	/// Consumed while the panel is open either way, which the owner reads as "persist now".
	bool OnPointerUp(float x, float y) override;

	/// Scrolls the row list, and consumes regardless so a wheel notch never falls through to
	/// the carousel underneath.
	bool OnScroll(float x, float y, float wheelDelta) override;

	bool OnChar(u32 character) override;

	/// Escape closes the panel; this widget owns its own dismissal. Return applies the typed
	/// font name when that field is focused.
	bool OnKeyDown(u32 keyCode) override;

	bool IsBlocking() const override
	{
		return m_flOpenAmount > 0.01f;
	}

	ECursorKind GetDesiredCursor() const override;

	/// One-shot: the owner activates the unlock screen's setup mode when this reports true.
	bool ConsumeResetPasswordRequested();

  private:
	/// The panel's rects, and the row stack within them. Both are pure functions of the current
	/// state, recomputed wherever needed rather than cached.
	struct PanelLayout;
	struct Rows;

	PanelLayout Layout() const;
	Rows RowsFor(const PanelLayout &layout) const;

	/// Re-bakes both faces and, only if that succeeds, mirrors the typed name into settings -
	/// so a bad in-flight edit is never persisted.
	void ApplyFontSettings();

	/// True if the point is on this control and its row is actually on screen.
	bool IsRowControlHit(const PanelLayout &layout, Rect row, Rect control, float x, float y) const;

	/// Handles a click on either stepper, clamping to its own range and re-baking.
	void HandleStepperClick(Rect stepper, float &value, float minValue, float maxValue, float x, float y);

	/// Shared by the two sliders' press and drag paths.
	void ApplyAnimationSpeedFromPointer(Rect track, float x);
	void ApplyCornerRoundnessFromPointer(Rect track, float x);

	/// The real click handling, kept out of OnPointerUp so the drag-versus-click check there
	/// stays readable.
	bool HandleClick(float x, float y);
	bool HandleResetButtonClick(const PanelLayout &layout, const Rows &rows, float x, float y);
	bool HandleAccentSwatchClick(const PanelLayout &layout, const Rows &rows, float x, float y);

	/// Writes the default for this target back into settings, applying whatever live side
	/// effect a manual edit would, and kicks that row's spin. The value itself animates back,
	/// because every control draws from the eased copies rather than from settings.
	void ResetTargetToDefault(ESettingsResetTarget target);

	/// Whether this target already holds its shipped default - what decides whether its button
	/// is showing at all.
	bool IsTargetAtDefault(ESettingsResetTarget target) const;

	/// Which row a reset target lives on, which control it sits beside, and where its own button
	/// lands. Every switch over the enum in these is exhaustive with no default label, so adding
	/// a target is a compile error rather than a button that silently does nothing.
	static Rect ResetTargetRowRect(const Rows &rows, ESettingsResetTarget target);
	static Rect ResetTargetControlRect(const Rows &rows, ESettingsResetTarget target, const CFontManager *pFonts);
	static Rect ResetTargetButtonRect(const Rows &rows, ESettingsResetTarget target, const CFontManager *pFonts);

	/// Per-frame hover, fade and spin bookkeeping for the reset buttons, plus the tooltip
	/// request. Split out of Update purely so Update stays readable.
	void UpdateResetButtons(float deltaSeconds);

	void DrawChrome(CDrawList &drawList, const PanelLayout &layout, u8 alpha) const;
	void DrawRowHoverHighlight(CDrawList &drawList, const PanelLayout &layout, const Rows &rows, u8 alpha) const;
	void DrawSectionHeaders(CDrawList &drawList, const PanelLayout &layout, const Rows &rows, u8 alpha) const;
	void DrawAppearanceRows(CDrawList &drawList, const PanelLayout &layout, const Rows &rows, u8 alpha);
	void DrawMotionRows(CDrawList &drawList, const PanelLayout &layout, const Rows &rows, u8 alpha) const;
	void DrawPrivacyRows(CDrawList &drawList, const PanelLayout &layout, const Rows &rows, u8 alpha) const;
	void DrawSecurityRows(CDrawList &drawList, const PanelLayout &layout, const Rows &rows, u8 alpha) const;
	void DrawResetButtons(CDrawList &drawList, const PanelLayout &layout, const Rows &rows, u8 alpha) const;

	CFontManager *m_pFonts = nullptr;
	Settings *m_pSettings = nullptr;
	const CWindow &m_window;
	IRenderer *m_pRenderer = nullptr;
	const CAssetManager &m_assets;

	bool m_bOpen = false;
	float m_flOpenAmount = 0.0f;

	CTextInput m_fontNameInput;

	float m_flAnimationsToggleAmount = 1.0f;
	float m_flExcludeFromCaptureToggleAmount = 1.0f;
	float m_flBlockOverlayInjectionToggleAmount = 1.0f;
	float m_flNotificationsToggleAmount = 1.0f;
	float m_flCloseToTrayToggleAmount = 1.0f;

	/// Eased display copies of every value whose control would otherwise snap. Settings always
	/// holds the real, immediately applied value; these only drive what gets drawn, so
	/// restoring a default reads as the value travelling back rather than blinking there.
	/// Seeded from settings in the constructor, so the first frame is never an animation up
	/// from zero. The accent is three floats rather than a Color because easing on already
	/// quantized channels stalls a step short of its target.
	float m_flFontSizeDisplay = 0.0f;
	float m_flSecondaryFontSizeDisplay = 0.0f;
	float m_flAnimationSpeedDisplay = 0.0f;
	float m_flCornerRoundnessDisplay = 0.0f;
	float m_flAccentDisplayR = 0.0f;
	float m_flAccentDisplayG = 0.0f;
	float m_flAccentDisplayB = 0.0f;

	/// Indexed by ESettingsResetTarget. Appear is the fade; Spin is 1 the moment a button is
	/// clicked and eases back to 0, drawn as a full turn.
	float m_aResetAppearAmount[static_cast<u64>(ESettingsResetTarget::Count)]{};
	float m_aResetSpinAmount[static_cast<u64>(ESettingsResetTarget::Count)]{};

	CTooltip m_tooltip;

	/// Dragging any of its controls applies the result live rather than only on close.
	CColorPicker m_colorPicker;

	CScrollable m_rowsScroll;

	/// One per slider rather than a shared "some slider is dragging" flag, which would let a
	/// drag started on either continue over the other.
	CDraggable m_animationSpeedDrag;
	CDraggable m_cornerRoundnessDrag;

	bool m_bResetPasswordRequestedThisFrame = false;
};
