#pragma once

#include <string_view>

#include "core/types.h"
#include "gfx/asset_manager.h"
#include "gfx/font_manager.h"
#include "ui/widget.h"

class CWindow;
struct Settings;

/// What clicking a notification should do. The host only records the choice - carrying it out
/// belongs to the owner, which is the only thing that knows what an update overlay is.
enum class EToastAction : u8 {
	/// A click just dismisses, which is what every purely informational notification wants.
	None,

	OpenUpdates,
};

/// One notification. Assembled by the caller so the common case stays a single braced literal and
/// the uncommon fields do not turn every call site into a row of defaults.
struct Notification {
	std::string_view Message;

	/// EAsset::Count for none, which leaves the card as text alone.
	EAsset Icon = EAsset::Count;

	/// Turns the icon slowly for as long as the card is up. Opt-in per notification rather than
	/// something every icon does: motion in the corner of the eye is worth spending on a
	/// notification the user has not acted on yet, and wasted on one merely reporting a result.
	bool SpinIcon = false;

	EToastAction Action = EToastAction::None;
};

/// Somewhere to report a completed action to. The only implementation is CToastHost, and the
/// interface exists so a widget's dependency reads as "somewhere to report to" rather than a
/// concrete panel it otherwise has no business knowing about.
class INotificationSink {
  public:
	virtual ~INotificationSink() = default;

	/// The message is copied, so a caller may pass a view of a temporary.
	virtual void Notify(const Notification &notification) = 0;

	/// A notification tied to a deadline the caller is itself enforcing - an armed confirm, say.
	/// Its bar is the deadline, so the two cannot drift: it does not pause on hover and it is not
	/// dismissed by a click, because neither of those pauses or cancels what it counts down to.
	virtual void NotifyDeadline(std::string_view message, float seconds) = 0;

	/// Clears a deadline notification, and only that - an ordinary one showing in its place is
	/// left alone, since it was not this caller that raised it.
	virtual void DismissDeadline() = 0;
};

/// One small notification in the bottom-left corner.
///
/// Deliberately one at a time: these confirm an action the user just took, and a stack of them is
/// a log, which is not what a corner of the screen is for. A new notification replaces whatever is
/// showing rather than queueing behind it, since the newest is always the relevant one.
///
/// The card is as wide as its text needs and no wider. Never blocking, and consumes a press only
/// when it lands on the card - both halves of the press, or the widget underneath would receive a
/// mouse-down whose matching up never arrives and would sit there thinking a drag was in progress.
class CToastHost : public CWidget, public INotificationSink {
  public:
	CToastHost(const CFontManager &fonts, const CWindow &window, const CAssetManager &assets, const Settings &settings);

	void Notify(const Notification &notification) override;
	void NotifyDeadline(std::string_view message, float seconds) override;
	void DismissDeadline() override;

	/// The action of a notification the user just clicked, cleared on read. None almost always.
	EToastAction ConsumeAction();

	void Update(float deltaSeconds) override;
	void Draw(CDrawList &drawList) override;

	bool OnPointerDown(float x, float y) override;
	bool OnPointerUp(float x, float y) override;

	ECursorKind GetDesiredCursor() const override;

  private:
	static constexpr u32 kMaxMessageLength = 96;
	static constexpr u32 kMaxLines = 2;

	void Show(const Notification &notification, float seconds, bool deadlineDriven);

	/// The icon plus the gap after it, or zero when there is no icon.
	float IconColumnWidth() const;

	/// Sized to the message: its natural single-line width, clamped so a short one is not a stub
	/// and a long one wraps instead of running off the screen.
	float CardWidth() const;
	float CardHeight() const;

	/// Where the card sits once it has finished sliding in, which is what hit-testing uses - a card
	/// mid-slide is not something anyone is trying to click.
	Rect CardRect() const;

	/// The same rect shifted left by however much of the entry slide is still outstanding.
	Rect AnimatedCardRect() const;

	void DrawTimeoutBar(CDrawList &drawList, Rect card, u8 alpha) const;

	bool IsInteractive() const;
	bool IsPointerOverCard(float x, float y) const;

	const CFontManager &m_fonts;
	const CWindow &m_window;
	const CAssetManager &m_assets;
	const Settings &m_settings;

	char m_szMessage[kMaxMessageLength]{};
	EAsset m_icon = EAsset::Count;
	bool m_bSpinIcon = false;
	EToastAction m_action = EToastAction::None;
	EToastAction m_pendingAction = EToastAction::None;

	/// Counts down to zero, then the card leaves. Also the timeout bar's own width.
	float m_flRemainingSeconds = 0.0f;
	float m_flTotalSeconds = 0.0f;

	/// Eased 0..1 on the way in and back to 0 on the way out, driving both the fade and the slide.
	float m_flPresenceAmount = 0.0f;

	/// Drives the timeout bar's travelling highlight. Never reset, so the sweep does not jump when
	/// one notification replaces another.
	float m_flElapsedSeconds = 0.0f;
	bool m_bShowing = false;

	/// Set for a deadline notification: no hover hold, and no dismiss on click.
	bool m_bDeadlineDriven = false;
};
