#include "ui/account_modal.h"

#include "core/profiler.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include <Windows.h>

#include "core/animator.h"
#include "core/settings.h"
#include "gfx/asset_manager.h"
#include "platform/window.h"
#include "ui/controls.h"
#include "ui/draw_list.h"
#include "ui/layout.h"
#include "ui/text.h"

namespace {
constexpr float kOpenEaseRate = 14.0f;

// The panel tracks the window's size, with an absolute ceiling so it does not balloon on a large
// monitor. The 1.6875 aspect is flatter than 1.5, so it takes less vertical space. Both grow
// with the font-size setting.
constexpr float kPanelWidthFraction = 0.78f;
constexpr float kPanelHeightFraction = 0.71f;
constexpr float kPanelMaxWidthBase = 810.0f;
constexpr float kPanelMaxHeightBase = 480.0f;

// Real baked pixels, matching the default font size the panel's base size was tuned at.
constexpr float kReferenceBodyPixelHeight = 24.0f;

constexpr float kPanelMargin = 48.0f;
constexpr float kPanelScaleMin = 0.92f;
constexpr float kPanelBorderThickness = 1.5f;
constexpr float kPanelRadius = 16.0f;

constexpr float kLeftColumnFraction = 0.35f;
constexpr float kSeparatorThickness = 2.0f;

constexpr float kCloseBadgeSize = 40.0f;
constexpr float kCloseBadgeMargin = 12.0f;
constexpr float kCloseBadgeIconSize = 18.0f;

constexpr float kRowPadding = 24.0f;
constexpr float kRowTopPadding = 14.0f;
constexpr float kRowLineGap = 4.0f;
constexpr float kRowBottomPadding = 10.0f;
constexpr float kRowButtonGap = 10.0f;

constexpr float kActionButtonWidth = 108.0f;

// The gap between two footer buttons. Their outer edges use the row padding instead, so the
// primary action's right edge lands on the same column as the rows and fields above it.
constexpr float kActionButtonGap = 16.0f;

// The label sits close to its box; the larger gap comes after, before the next field's label.
constexpr float kEditFieldLabelGap = 8.0f;
constexpr float kEditFieldGroupGap = 26.0f;
constexpr u32 kEditFieldCount = 3;

constexpr float kVisibilityChipIconSize = 16.0f;
constexpr float kVisibilityChipIconInset = 10.0f;
constexpr float kVisibilityChipIconGap = 7.0f;
constexpr float kVisibilityChipRightPadding = 12.0f;

constexpr float kPasswordRevealButtonSize = 24.0f;
constexpr float kScrollbarTrackMargin = 4.0f;

constexpr float kIndicatorOuterRadius = 40.0f;
constexpr float kIndicatorRingThickness = 8.0f;
constexpr float kIndicatorInnerRadius = kIndicatorOuterRadius - kIndicatorRingThickness;
constexpr float kIndicatorGlowMargin = 22.0f;
constexpr float kIndicatorSweepDeg = 112.0f;
constexpr float kIndicatorRotationDegPerSec = 260.0f;
constexpr float kIndicatorFullSweepDeg = 360.0f;
constexpr u32 kMaxLoginMessageLines = 3;

constexpr Color kColorPanelBorder{74, 74, 80, 255};
constexpr Color kColorPanelBg{24, 24, 27, 255};
constexpr Color kColorSeparator{48, 48, 53, 255};
constexpr Color kColorTextBright{232, 232, 236, 255};
constexpr Color kColorTextDim{150, 150, 156, 255};
constexpr Color kColorTextFaint{130, 130, 136, 255};
constexpr Color kColorSuccess{80, 200, 120, 255};
constexpr Color kColorError{220, 90, 80, 255};
constexpr Color kColorWhite{255, 255, 255, 255};
constexpr Color kColorHoverBadge{56, 56, 62, 255};
constexpr Color kColorRowSelected{58, 58, 62, 255};
constexpr Color kColorRowHover{38, 38, 43, 255};
constexpr Color kColorRemoveHoverBg{68, 42, 42, 255};

/// Inset so a row icon does not touch the edge of its circular hover badge.
constexpr float kRowIconInset = 5.0f;

/// Room for the armed pill's label beyond the round badge it grows out of.
constexpr float kRowConfirmExtraWidth = 46.0f;

constexpr Color kColorNeutralButton{46, 46, 52, 255};
constexpr Color kColorNeutralButtonHover{60, 60, 66, 255};
constexpr Color kColorDisabledButton{60, 58, 70, 255};
constexpr Color kColorDeleteButton{60, 45, 45, 255};
constexpr Color kColorScrollThumb{120, 120, 128, 190};
constexpr Color kColorFieldBorder{46, 46, 52, 255};
constexpr Color kColorFieldFill{24, 24, 27, 255};
constexpr Color kColorFieldBorderFocused{225, 225, 230, 255};
constexpr Color kColorFieldFillFocused{42, 42, 46, 255};

float PanelMaxSizeScale(const CFontManager &fonts)
{
	return std::max(1.0f, fonts.GetBody().GetPixelHeight() / kReferenceBodyPixelHeight);
}

float RowHeightFor(const CFontManager &fonts)
{
	return kRowTopPadding + fonts.GetBody().GetLineHeight() + kRowLineGap + fonts.GetSecondary().GetLineHeight() +
		   kRowBottomPadding;
}

// Body, not secondary: the section title is a real title treatment, not a caption.
float HeaderHeightFor(const CFontManager &fonts)
{
	return fonts.GetBody().GetLineHeight() + 20.0f;
}

float FooterHeightFor(const CFontManager &fonts)
{
	return std::max(56.0f, fonts.GetBody().GetLineHeight() + 20.0f);
}

// One size for every primary footer action.
float ActionButtonHeightFor(const CFontManager &fonts)
{
	return std::max(36.0f, fonts.GetBody().GetLineHeight() + 14.0f);
}

// Body, not secondary: the field holds a value the user typed, not a dim label.
float EditFieldInputHeightFor(const CFontManager &fonts)
{
	return std::max(34.0f, fonts.GetBody().GetLineHeight() + 12.0f);
}

float EditFieldBlockHeightFor(const CFontManager &fonts)
{
	return fonts.GetSecondary().GetLineHeight() + kEditFieldLabelGap + EditFieldInputHeightFor(fonts) +
		   kEditFieldGroupGap;
}

// Derived from font metrics, since a flat constant reads undersized at a larger font size.
float RowButtonSizeFor(const CFontManager &fonts)
{
	return std::max(28.0f, fonts.GetSecondary().GetLineHeight() + 8.0f);
}

// A mask of 0 means no explicit choice yet, which Account's accessor treats as "this banner
// only" - so this counts it the same way, as one game.
u32 VisibleGameCountFromMask(u16 mask)
{
	if (mask == 0) return 1;

	u32 count = 0;
	for (u32 b = 0; b < kCarouselMaxBanners; b += 1) {
		if ((mask & (1u << b)) != 0) {
			count += 1;
		}
	}

	return count;
}

// Shared by the chip's sizing and its drawing, so the two cannot disagree.
std::string_view FormatVisibleGameCount(u16 mask, char *pOut, usize outCapacity)
{
	const u32 count = VisibleGameCountFromMask(mask);
	const int written = std::snprintf(pOut, outCapacity, "%u %s", count, count == 1 ? "game" : "games");

	return std::string_view{pOut, written > 0 ? static_cast<u64>(written) : 0};
}

// Layered semi-transparent rounded rects offset downward, so the panel reads as lifted.
// This panel's field palette, applied over the shared chrome primitive. Which colours mean
// "focused" is a panel decision, which is why Controls takes them rather than a bool.
void DrawFieldChrome(CDrawList &drawList, Rect rect, bool highlighted, u8 alpha)
{
	constexpr float kFieldCornerRadius = 8.0f;

	Controls::DrawFieldChrome(drawList, rect, kFieldCornerRadius,
							  highlighted ? kColorFieldBorderFocused : kColorFieldBorder,
							  highlighted ? kColorFieldFillFocused : kColorFieldFill, alpha);
}

void DrawAccentButton(CDrawList &drawList, const CFont &font, Rect rect, std::string_view label, Color accent,
					  bool enabled, bool hovered, u8 alpha)
{
	Controls::DrawAccentButton(drawList, font, rect, label, accent, enabled, hovered, kColorDisabledButton,
							   kColorTextDim, alpha);
}

void DrawNeutralButton(CDrawList &drawList, const CFont &font, Rect rect, std::string_view label, Color liftColor,
					   Color restingFill, Color hoverFill, bool hovered, u8 alpha)
{
	Controls::DrawNeutralButton(drawList, font, rect, label, liftColor, restingFill, hoverFill, kColorTextBright,
								hovered, alpha);
}

// Waiting covers both launching the client and waiting for its window, which the worker does not
// distinguish either. The terminal two are a fallback: the progress view prefers the attempt's
// own message whenever it has one.
std::string_view LoginStageMessage(ELoginStage stage)
{
	switch (stage) {
		case ELoginStage::Idle:
			return "";
		case ELoginStage::WaitingForProcess:
			return "Launching Riot Client...";
		case ELoginStage::Connecting:
			return "Waiting for Riot Client...";
		case ELoginStage::Authenticating:
			return "Logging in...";
		case ELoginStage::Launching:
			return "Launching game...";
		case ELoginStage::Success:
			return "Logged in!";
		case ELoginStage::Error:
			return "Something went wrong.";
		case ELoginStage::Cancelled:
			return "Cancelled.";
	}

	return "";
}
} // namespace

CAccountModal::CAccountModal(CCarousel *pCarousel, INotificationSink *pNotifications, const CFontManager &fonts,
							 const CWindow &window, const Settings &settings, const CAssetManager &assets)
	: m_pCarousel(pCarousel)
	, m_pNotifications(pNotifications)
	, m_fonts(fonts)
	, m_window(window)
	, m_settings(settings)
	, m_assets(assets)
	, m_gameSelect(fonts)
{
	m_editUsername.Init("");
	m_editNote.Init("");
	m_editPassword.Init("");
	m_login.Init();
}

Rect CAccountModal::PanelRect() const
{
	const float sizeScale = PanelMaxSizeScale(m_fonts);
	const float panelMaxWidth = kPanelMaxWidthBase * sizeScale;
	const float panelMaxHeight = kPanelMaxHeightBase * sizeScale;

	const auto windowW = static_cast<float>(m_window.GetWidth());
	const auto windowH = static_cast<float>(m_window.GetHeight());

	// The smallest of: a fraction of the window, the absolute ceiling, and whatever fits inside
	// the margin.
	float w = std::min({windowW * kPanelWidthFraction, panelMaxWidth, std::max(0.0f, windowW - kPanelMargin * 2.0f)});
	float h = std::min({windowH * kPanelHeightFraction, panelMaxHeight, std::max(0.0f, windowH - kPanelMargin * 2.0f)});

	// Whichever dimension binds wins; the other shrinks to match, so clamping never distorts
	// the panel.
	const float targetAspect = panelMaxWidth / panelMaxHeight;
	if (w / h > targetAspect) {
		w = h * targetAspect;
	} else {
		h = w / targetAspect;
	}

	const float openScale = kPanelScaleMin + (1.0f - kPanelScaleMin) * m_flOpenAmount;
	w *= openScale;
	h *= openScale;

	return Rect{(windowW - w) * 0.5f, (windowH - h) * 0.5f, w, h};
}

CAccountModal::Layout CAccountModal::ComputeLayout() const
{
	Layout layout{};
	layout.Panel = PanelRect();
	layout.Inner = RectInset(layout.Panel, kPanelBorderThickness);

	const float footerHeight = FooterHeightFor(m_fonts);
	layout.Content = Rect{layout.Inner.X, layout.Inner.Y, layout.Inner.W, layout.Inner.H - footerHeight};
	layout.Footer = Rect{layout.Inner.X, layout.Inner.Y + layout.Inner.H - footerHeight, layout.Inner.W, footerHeight};

	layout.Left = Rect{layout.Content.X, layout.Content.Y, layout.Content.W * kLeftColumnFraction, layout.Content.H};

	const float rightX = layout.Left.X + layout.Left.W + kSeparatorThickness;
	layout.Right = Rect{rightX, layout.Content.Y, layout.Content.X + layout.Content.W - rightX, layout.Content.H};

	return layout;
}

bool CAccountModal::HasValidBanner() const
{
	return m_nBannerIndex >= 0 && static_cast<u32>(m_nBannerIndex) < m_pCarousel->GetBannerCount();
}

Rect CAccountModal::AccountsScrollRegionRect(Rect right) const
{
	const float headerHeight = HeaderHeightFor(m_fonts);

	return Rect{right.X, right.Y + headerHeight, right.W, right.H - headerHeight};
}

Rect CAccountModal::AccountsScrollbarTrackRect(Rect scrollRegion) const
{
	return Rect{scrollRegion.X + scrollRegion.W - kScrollbarWidth - kScrollbarTrackMargin, scrollRegion.Y,
				kScrollbarWidth, scrollRegion.H};
}

bool CAccountModal::BuildAccountListView(const Layout &layout, AccountListView &out) const
{
	out = AccountListView{};
	out.ScrollRegion = AccountsScrollRegionRect(layout.Right);
	out.ScrollbarTrack = AccountsScrollbarTrackRect(out.ScrollRegion);

	if (!HasValidBanner()) return false;

	out.Count = m_pCarousel->GetVisibleAccounts(static_cast<u32>(m_nBannerIndex), out.Refs);
	out.RowHeight = RowHeightFor(m_fonts);

	// No gap: the header's boundary line uses the row-divider colour, so it reads as one more
	// separator in the sequence.
	for (u32 i = 0; i < out.Count; i += 1) {
		out.Tops[i] = static_cast<float>(i) * out.RowHeight;
	}

	out.ContentHeight = static_cast<float>(out.Count) * out.RowHeight;

	return true;
}

Rect CAccountModal::RowRectAt(const Layout &layout, const AccountListView &view, u32 index) const
{
	const float headerHeight = HeaderHeightFor(m_fonts);
	const float y = layout.Right.Y + headerHeight + view.Tops[index] - m_accountsScroll.m_flScrollOffset;

	return Rect{layout.Right.X + kRowPadding, y, layout.Right.W - kRowPadding * 2.0f, view.RowHeight};
}

bool CAccountModal::IsRowVisible(const AccountListView &view, Rect row) const
{
	return row.Y + row.H > view.ScrollRegion.Y && row.Y < view.ScrollRegion.Y + view.ScrollRegion.H;
}

// Hit-testing knows nothing about the GPU clip rect, so the point is checked against the scroll
// region: otherwise a click in the header could hit a row scrolled behind it.
i32 CAccountModal::HitTestRow(const Layout &layout, const AccountListView &view, float x, float y) const
{
	if (!RectContainsPoint(view.ScrollRegion, x, y)) return -1;

	for (u32 i = 0; i < view.Count; i += 1) {
		const Rect row = RowRectAt(layout, view, i);

		if (IsRowVisible(view, row) && RectContainsPoint(row, x, y)) return static_cast<i32>(i);
	}

	return -1;
}

const Account &CAccountModal::AccountAt(const AccountListView &view, u32 index) const
{
	const VisibleAccountRef &ref = view.Refs[index];

	return m_pCarousel->GetBanner(ref.BannerIndex).Accounts[ref.AccountIndex];
}

Rect CAccountModal::CloseBadgeRect(Rect left) const
{
	return Rect{left.X + kCloseBadgeMargin, left.Y + kCloseBadgeMargin, kCloseBadgeSize, kCloseBadgeSize};
}

// Login, Save and Back all share this rect, so the primary action always lands in the same
// place whichever mode is showing.
Rect CAccountModal::PrimaryButtonRect(Rect footer) const
{
	const float height = ActionButtonHeightFor(m_fonts);

	return Rect{footer.X + footer.W - kRowPadding - kActionButtonWidth, footer.Y + (footer.H - height) * 0.5f,
				kActionButtonWidth, height};
}

Rect CAccountModal::RowRemoveButtonRect(Rect row) const
{
	const float size = RowButtonSizeFor(m_fonts);

	return Rect{row.X + row.W - size, row.Y + (row.H - size) * 0.5f, size, size};
}

// The armed delete pill: the round remove badge stretched leftward to fit a label. Hit-testing
// uses this while armed, so the whole visible target is clickable rather than just the circle it
// grew from.
Rect CAccountModal::RowConfirmDeleteRect(Rect row) const
{
	const Rect remove = RowRemoveButtonRect(row);

	return Rect{remove.X - kRowConfirmExtraWidth, remove.Y, remove.W + kRowConfirmExtraWidth, remove.H};
}

Rect CAccountModal::RowEditButtonRect(Rect row) const
{
	const Rect remove = RowRemoveButtonRect(row);

	return Rect{remove.X - kRowButtonGap - remove.W, remove.Y, remove.W, remove.H};
}

// A circular icon-only badge, matching the row buttons' convention for a secondary action.
Rect CAccountModal::AddAccountButtonRect(Rect right) const
{
	const float headerHeight = HeaderHeightFor(m_fonts);
	const float size = RowButtonSizeFor(m_fonts);

	return Rect{right.X + right.W - kRowPadding - size, right.Y + (headerHeight - size) * 0.5f, size, size};
}

// The three blocks settle into the upper portion rather than leaving a dead gap before the
// footer.
Rect CAccountModal::EditFieldBlockRect(Rect right, u32 index) const
{
	const float headerHeight = HeaderHeightFor(m_fonts);
	const float blockHeight = EditFieldBlockHeightFor(m_fonts);
	const float available = right.H - headerHeight;
	const float offsetY = std::max(0.0f, (available - blockHeight * static_cast<float>(kEditFieldCount)) * 0.28f);

	return Rect{right.X + kRowPadding, right.Y + headerHeight + offsetY + static_cast<float>(index) * blockHeight,
				right.W - kRowPadding * 2.0f, blockHeight};
}

// Anchored below the label rather than at the block's bottom, which piled all the slack above
// the field instead of splitting it around.
Rect CAccountModal::EditFieldInputRect(Rect block) const
{
	const float labelHeight = m_fonts.GetSecondary().GetLineHeight();

	return Rect{block.X, block.Y + labelHeight + kEditFieldLabelGap, block.W, EditFieldInputHeightFor(m_fonts)};
}

Rect CAccountModal::EditFieldInputRectAt(Rect right, u32 index) const
{
	return EditFieldInputRect(EditFieldBlockRect(right, index));
}

// Top-right of the form's header, the same slot and height as the account list's add button, so
// switching modes does not move the header's furniture. Sized to fit its content.
Rect CAccountModal::VisibilityChipRect(Rect right) const
{
	const float headerHeight = HeaderHeightFor(m_fonts);
	const float height = RowButtonSizeFor(m_fonts);

	char countBuffer[16];
	const std::string_view countText = FormatVisibleGameCount(m_gameSelect.GetMask(), countBuffer, sizeof(countBuffer));
	const float countTextW = TextWidth(m_fonts.GetSecondary(), countText);

	const float width = kVisibilityChipIconInset + kVisibilityChipIconSize + kVisibilityChipIconGap + countTextW +
						kVisibilityChipRightPadding;

	return Rect{right.X + right.W - kRowPadding - width, right.Y + (headerHeight - height) * 0.5f, width, height};
}

Rect CAccountModal::EditCancelButtonRect(Rect save) const
{
	return Rect{save.X - kActionButtonGap - kActionButtonWidth, save.Y, kActionButtonWidth, save.H};
}

// Bottom-left of the footer, only shown when editing an existing row.
Rect CAccountModal::EditDeleteButtonRect(Rect footer) const
{
	const float height = ActionButtonHeightFor(m_fonts);

	return Rect{footer.X + kRowPadding, footer.Y + (footer.H - height) * 0.5f, kActionButtonWidth, height};
}

Rect CAccountModal::PasswordRevealButtonRect(Rect passwordField) const
{
	return Rect{passwordField.X + passwordField.W - kPasswordRevealButtonSize - 6.0f,
				passwordField.Y + (passwordField.H - kPasswordRevealButtonSize) * 0.5f, kPasswordRevealButtonSize,
				kPasswordRevealButtonSize};
}

bool CAccountModal::ResolveVisibleAccount(u32 bannerIndex, u32 queryIndex, VisibleAccountRef &out) const
{
	VisibleAccountRef refs[kCarouselMaxVisibleAccounts];
	const u32 count = m_pCarousel->GetVisibleAccounts(bannerIndex, refs);

	if (queryIndex >= count) return false;

	out = refs[queryIndex];

	return true;
}

void CAccountModal::StartLoginFor(u32 bannerIndex, u32 queryIndex)
{
	VisibleAccountRef ref{};
	if (!ResolveVisibleAccount(bannerIndex, queryIndex, ref)) return;

	const Account &account = m_pCarousel->GetBanner(ref.BannerIndex).Accounts[ref.AccountIndex];
	m_login.Start(account.GetUsername(), account.m_szPassword, m_pCarousel->GetBanner(bannerIndex).Title);
}

void CAccountModal::RequestLogin(i32 bannerIndex, i32 queryIndex)
{
	m_nSelectedAccountIndex = queryIndex;
	m_mode = EAccountModalMode::LoginProgress;
	m_flLoginElapsedSeconds = 0.0f;

	if (m_login.IsActive() && !CLoginAttempt::IsTerminalStage(m_login.GetStage())) {
		m_login.Cancel();
		m_nPendingLoginBannerIndex = bannerIndex;
		m_nPendingLoginQueryIndex = queryIndex;

		return;
	}

	m_nPendingLoginBannerIndex = -1;
	m_nPendingLoginQueryIndex = -1;
	StartLoginFor(static_cast<u32>(bannerIndex), static_cast<u32>(queryIndex));
}

void CAccountModal::Open(i32 bannerIndex)
{
	m_bIsOpen = true;
	m_nBannerIndex = bannerIndex;
	m_mode = EAccountModalMode::AccountList;

	// A different banner has a different account count, so the old scroll position means
	// nothing here.
	m_accountsScroll = CScrollable{};
	m_nSelectedAccountIndex = -1;
}

// Both latches are cleared on the way out, so reopening never lands on a button already armed
// from a previous session with the panel.
void CAccountModal::Close()
{
	m_bIsOpen = false;
	m_rowDeleteConfirm.Disarm();
	m_editDeleteConfirm.Disarm();
}

void CAccountModal::OpenForQuickLogin(i32 bannerIndex, i32 accountIndex)
{
	Open(bannerIndex);
	RequestLogin(bannerIndex, accountIndex);
}

void CAccountModal::StartAddAccount()
{
	m_editDeleteConfirm.Disarm();
	m_mode = EAccountModalMode::EditAccount;
	m_nEditAccountIndex = -1;

	m_editUsername.SetValue("");
	m_editNote.SetValue("");
	m_editPassword.SetValue("");
	m_editUsername.m_bFocused = true;
	m_editNote.m_bFocused = false;
	m_editPassword.m_bFocused = false;
	m_bEditPasswordRevealed = false;

	// Explicitly this banner's bit rather than a raw 0, which would leave the chip claiming one
	// game while the popup showed nothing checked. It is also the row the popup protects, so a
	// new account cannot end up visible nowhere.
	const u16 initialMask = m_nBannerIndex >= 0 ? static_cast<u16>(1u << m_nBannerIndex) : 0;

	// Open then immediately close: this seeds the popup's working mask without showing it, which
	// its persist-across-reopen contract allows.
	m_gameSelect.Open(initialMask, &m_pCarousel->GetBanner(0), m_pCarousel->GetBannerCount(), Rect{}, Rect{});
	m_gameSelect.Close();
}

void CAccountModal::StartEditAccount(u32 queryIndex)
{
	m_rowDeleteConfirm.Disarm();
	m_editDeleteConfirm.Disarm();
	m_mode = EAccountModalMode::EditAccount;
	m_nEditAccountIndex = static_cast<i32>(queryIndex);

	u16 mask = 0;
	VisibleAccountRef ref{};

	if (m_nBannerIndex >= 0 && ResolveVisibleAccount(static_cast<u32>(m_nBannerIndex), queryIndex, ref)) {
		const Account &account = m_pCarousel->GetBanner(ref.BannerIndex).Accounts[ref.AccountIndex];
		m_editUsername.SetValue(account.GetUsername());
		m_editNote.SetValue(account.GetNote());
		m_editPassword.SetValue(account.m_szPassword);
		mask = account.GetEffectiveVisibleMask(ref.BannerIndex);
	}

	m_editUsername.m_bFocused = true;
	m_editNote.m_bFocused = false;
	m_editPassword.m_bFocused = false;
	m_bEditPasswordRevealed = false;

	// Seeded the same way as adding - see StartAddAccount.
	m_gameSelect.Open(mask, &m_pCarousel->GetBanner(0), m_pCarousel->GetBannerCount(), Rect{}, Rect{});
	m_gameSelect.Close();
}

bool CAccountModal::CanSaveEditedAccount() const
{
	return !m_editUsername.GetValue().empty() && !m_editPassword.GetValue().empty();
}

bool CAccountModal::GetAccountCopyFields(u32 queryIndex, const char *&outUsername, const char *&outPassword) const
{
	VisibleAccountRef ref{};
	if (m_nBannerIndex < 0 || !ResolveVisibleAccount(static_cast<u32>(m_nBannerIndex), queryIndex, ref)) {
		return false;
	}

	const Account &account = m_pCarousel->GetBanner(ref.BannerIndex).Accounts[ref.AccountIndex];
	outUsername = account.m_szUsername;
	outPassword = account.m_szPassword;

	return true;
}

void CAccountModal::RemoveAccountRow(u32 queryIndex)
{
	VisibleAccountRef ref{};
	if (m_nBannerIndex < 0 || !ResolveVisibleAccount(static_cast<u32>(m_nBannerIndex), queryIndex, ref)) return;

	m_pCarousel->RemoveAccount(ref.BannerIndex, ref.AccountIndex);

	const auto removed = static_cast<i32>(queryIndex);
	if (m_nSelectedAccountIndex == removed) {
		m_nSelectedAccountIndex = -1;
	} else if (m_nSelectedAccountIndex > removed) {
		m_nSelectedAccountIndex -= 1;
	}
}

// True while either delete button is waiting on its second click.
bool CAccountModal::IsDeleteArmed() const
{
	return m_rowDeleteConfirm.IsArmedAny() || m_editDeleteConfirm.IsArmedAny();
}

// The prompt and the button share one deadline, so the bar running out and the button disarming
// are the same event rather than two clocks that happen to be set to the same number.
void CAccountModal::NotifyDeleteArmed() const
{
	if (m_pNotifications != nullptr) {
		m_pNotifications->NotifyDeadline("Click again to delete this account.", CConfirmLatch::kWindowSeconds);
	}
}

// A no-op when the owner supplied no sink, so no call site has to guard itself.
void CAccountModal::Notify(std::string_view message) const
{
	if (m_pNotifications != nullptr) {
		m_pNotifications->Notify(Notification{.Message = message});
	}
}

PendingHit CAccountModal::ConsumePendingRightClickRow()
{
	const PendingHit pending = m_pendingRightClickRow;
	m_pendingRightClickRow = PendingHit{};

	return pending;
}

// Both latches run every frame regardless of mode: an armed delete has to expire on its own even
// while the form it belongs to is not the one on screen.
void CAccountModal::Update(float deltaSeconds)
{
	m_flOpenAmount = CAnimator::EaseToward(m_flOpenAmount, m_bIsOpen ? 1.0f : 0.0f, kOpenEaseRate, deltaSeconds);

	const bool wasArmed = IsDeleteArmed();
	m_rowDeleteConfirm.Update(deltaSeconds);
	m_editDeleteConfirm.Update(deltaSeconds);

	// The prompt is cleared the moment nothing is armed any more, however that happened - a
	// commit, a click elsewhere, or the window simply running out.
	if (wasArmed && !IsDeleteArmed()) {
		if (m_pNotifications != nullptr) {
			m_pNotifications->DismissDeadline();
		}
	}

	if (!m_bIsOpen && m_flOpenAmount < 0.002f) {
		m_flOpenAmount = 0.0f;
		m_nBannerIndex = -1; // fully closed; the next open sets it fresh
	}

	switch (m_mode) {
		case EAccountModalMode::AccountList:
			m_accountsScroll.Update(deltaSeconds);
			break;

		case EAccountModalMode::LoginProgress:
			m_flLoginElapsedSeconds += deltaSeconds;
			break;

		case EAccountModalMode::EditAccount:
			m_editUsername.Update(deltaSeconds);
			m_editNote.Update(deltaSeconds);
			m_editPassword.Update(deltaSeconds);
			break;
	}

	// The popup is a plain member rather than a stack entry, so nothing gates its mouse or
	// advances its animation unless this does it. The position is already gated by the owner.
	m_gameSelect.SetMouseGated(false, m_flMouseX, m_flMouseY);
	m_gameSelect.Update(deltaSeconds);

	// Regardless of mode: the join is instantaneous once the worker reaches a terminal stage.
	m_login.Update();

	// IsActive goes false once Update retires the worker, or its watchdog abandons a wedged one,
	// so a queued request always starts eventually.
	if (HasPendingLogin() && !m_login.IsActive()) {
		const i32 bannerIndex = m_nPendingLoginBannerIndex;
		const i32 queryIndex = m_nPendingLoginQueryIndex;

		m_nPendingLoginBannerIndex = -1;
		m_nPendingLoginQueryIndex = -1;
		m_flLoginElapsedSeconds = 0.0f;

		StartLoginFor(static_cast<u32>(bannerIndex), static_cast<u32>(queryIndex));
	}
}

bool CAccountModal::OnPointerDown(float x, float y)
{
	if (!IsBlocking() || m_mode != EAccountModalMode::AccountList) return IsBlocking();

	const Layout layout = ComputeLayout();

	AccountListView view;
	if (!BuildAccountListView(layout, view)) return true;

	m_accountsScroll.OnPointerDown(x, y, view.ScrollbarTrack, view.ContentHeight, view.ScrollRegion.H);

	return true;
}

bool CAccountModal::OnPointerMove(float x, float y)
{
	if (!m_accountsScroll.IsDragging()) return IsBlocking();

	const Layout layout = ComputeLayout();

	AccountListView view;
	if (!BuildAccountListView(layout, view)) return IsBlocking();

	m_accountsScroll.OnPointerMove(y, view.ScrollbarTrack, view.ContentHeight, view.ScrollRegion.H);

	return true;
}

// Arrow keys walk the rows, Enter logs the selected one in, and Delete arms the same two-step
// confirm the row's own button uses. Edit is left to the mouse: it opens a form, and a form is not
// something to land in by mistake while arrowing through a list.
//
// False for anything else, so a key this list has no use for still reaches the rest of OnKeyDown.
bool CAccountModal::HandleAccountListKey(u32 keyCode)
{
	AccountListView view;
	if (!BuildAccountListView(ComputeLayout(), view) || view.Count == 0) return false;

	const auto lastIndex = static_cast<i32>(view.Count) - 1;

	switch (keyCode) {
		case VK_UP:
		case VK_DOWN: {
			// From -1, either direction starts at the top rather than wrapping to the bottom: the
			// first arrow press after opening should land somewhere predictable.
			const i32 delta = keyCode == VK_DOWN ? 1 : -1;
			m_nSelectedAccountIndex =
				m_nSelectedAccountIndex < 0 ? 0 : std::clamp(m_nSelectedAccountIndex + delta, 0, lastIndex);

			ScrollSelectedRowIntoView(view);
			return true;
		}

		case VK_RETURN:
			if (m_nSelectedAccountIndex >= 0) {
				RequestLogin(m_nBannerIndex, m_nSelectedAccountIndex);
			}

			return true;

		case VK_DELETE:
			if (m_nSelectedAccountIndex >= 0) {
				if (m_rowDeleteConfirm.ClickArmedOrCommit(m_nSelectedAccountIndex)) {
					RemoveAccountRow(static_cast<u32>(m_nSelectedAccountIndex));
					Notify("Account deleted.");
				} else {
					NotifyDeleteArmed();
				}
			}

			return true;

		default:
			return false;
	}
}

// Nudges the list just far enough that the selected row is fully visible, measured against where
// the scroll is heading rather than where it is - holding an arrow key would otherwise correct
// against a still-moving offset and overshoot on every press.
void CAccountModal::ScrollSelectedRowIntoView(const AccountListView &view)
{
	if (m_nSelectedAccountIndex < 0) return;

	const Rect row = RowRectAt(ComputeLayout(), view, static_cast<u32>(m_nSelectedAccountIndex));
	const float settleShift = m_accountsScroll.m_flScrollOffset - m_accountsScroll.m_flTargetScrollOffset;
	const float top = row.Y + settleShift;

	const float above = view.ScrollRegion.Y - top;
	const float below = (top + row.H) - (view.ScrollRegion.Y + view.ScrollRegion.H);

	if (above > 0.0f) {
		m_accountsScroll.ScrollBy(-above, view.ContentHeight, view.ScrollRegion.H);
	} else if (below > 0.0f) {
		m_accountsScroll.ScrollBy(below, view.ContentHeight, view.ScrollRegion.H);
	}
}

bool CAccountModal::HandleAccountListClick(const Layout &layout, float x, float y)
{
	if (RectContainsPoint(AddAccountButtonRect(layout.Right), x, y)) {
		StartAddAccount();
		return true;
	}

	AccountListView view;
	if (!BuildAccountListView(layout, view)) return true;

	if (RectContainsPoint(view.ScrollRegion, x, y)) {
		for (u32 i = 0; i < view.Count; i += 1) {
			const Rect row = RowRectAt(layout, view, i);
			if (!IsRowVisible(view, row)) continue;

			// An armed row answers over its whole pill, and answers first: the pill overlaps the
			// edit button, and while a delete is pending that button is not what is on screen.
			const bool armed = m_rowDeleteConfirm.IsArmed(static_cast<i32>(i));
			const Rect deleteRect = armed ? RowConfirmDeleteRect(row) : RowRemoveButtonRect(row);

			if (RectContainsPoint(deleteRect, x, y)) {
				if (m_rowDeleteConfirm.ClickArmedOrCommit(static_cast<i32>(i))) {
					RemoveAccountRow(i);
					Notify("Account deleted.");
				} else {
					NotifyDeleteArmed();
				}

				return true;
			}

			if (!armed && RectContainsPoint(RowEditButtonRect(row), x, y)) {
				StartEditAccount(i);
				return true;
			}
		}

		// Clicking the gaps between rows, or the empty space below the last one, deselects.
		m_nSelectedAccountIndex = HitTestRow(layout, view, x, y);
	}

	if (m_nSelectedAccountIndex >= 0 && RectContainsPoint(PrimaryButtonRect(layout.Footer), x, y)) {
		RequestLogin(m_nBannerIndex, m_nSelectedAccountIndex);
	}

	return true;
}

// Cancel and Back share the primary button's rect and both return to the account list. Cancel is
// a real interrupt the worker notices within about one poll interval, and a no-op once the
// attempt has finished.
bool CAccountModal::HandleLoginProgressClick(const Layout &layout, float x, float y)
{
	if (RectContainsPoint(PrimaryButtonRect(layout.Footer), x, y)) {
		m_nPendingLoginBannerIndex = -1;
		m_nPendingLoginQueryIndex = -1;
		m_login.Cancel();
		m_mode = EAccountModalMode::AccountList;
	}

	return true;
}

void CAccountModal::SaveEditedAccount()
{
	if (!HasValidBanner()) {
		m_mode = EAccountModalMode::AccountList;
		return;
	}

	const auto bannerIndex = static_cast<u32>(m_nBannerIndex);
	const bool adding = m_nEditAccountIndex < 0;

	if (adding) {
		m_pCarousel->AddAccount(bannerIndex, m_editUsername.GetValue(), m_editNote.GetValue(),
								m_editPassword.GetValue());

		Banner &banner = m_pCarousel->GetBanner(bannerIndex);
		const u32 savedIndex = banner.AccountCount - 1;

		// Account::Init always resets the mask, so the form's working mask is written back
		// afterwards rather than threaded through it.
		banner.Accounts[savedIndex].m_uVisibleBannerMask = m_gameSelect.GetMask();

		// A new account is the last of this banner's, which is where the query places it too.
		m_nSelectedAccountIndex = static_cast<i32>(savedIndex);
	} else {
		VisibleAccountRef ref{};
		if (ResolveVisibleAccount(bannerIndex, static_cast<u32>(m_nEditAccountIndex), ref)) {
			m_pCarousel->UpdateAccount(ref.BannerIndex, ref.AccountIndex, m_editUsername.GetValue(),
									   m_editNote.GetValue(), m_editPassword.GetValue());
			m_pCarousel->GetBanner(ref.BannerIndex).Accounts[ref.AccountIndex].m_uVisibleBannerMask =
				m_gameSelect.GetMask();
		}
	}

	m_mode = EAccountModalMode::AccountList;
}

void CAccountModal::DeleteEditedAccount()
{
	if (HasValidBanner()) {
		VisibleAccountRef ref{};
		if (ResolveVisibleAccount(static_cast<u32>(m_nBannerIndex), static_cast<u32>(m_nEditAccountIndex), ref)) {
			m_pCarousel->RemoveAccount(ref.BannerIndex, ref.AccountIndex);
		}

		m_nSelectedAccountIndex = -1;
	}

	m_mode = EAccountModalMode::AccountList;
}

bool CAccountModal::HandleEditAccountClick(const Layout &layout, float x, float y)
{
	// The popup takes priority over everything else in the form while open: a click either
	// toggles one of its rows or, missing it entirely, closes it.
	if (m_gameSelect.IsBlocking()) {
		if (!m_gameSelect.OnPointerDown(x, y)) {
			m_gameSelect.Close();
		}

		return true;
	}

	const Rect chip = VisibilityChipRect(layout.Right);
	if (RectContainsPoint(chip, x, y)) {
		// Clamped to the form column, not the panel: the panel's height includes the footer, so
		// the popup could spill past the separator above the Delete and Save buttons.
		m_gameSelect.Open(m_gameSelect.GetMask(), &m_pCarousel->GetBanner(0), m_pCarousel->GetBannerCount(), chip,
						  layout.Right);
		return true;
	}

	const Rect noteField = EditFieldInputRectAt(layout.Right, 0);
	const Rect usernameField = EditFieldInputRectAt(layout.Right, 1);
	const Rect passwordField = EditFieldInputRectAt(layout.Right, 2);

	if (RectContainsPoint(PasswordRevealButtonRect(passwordField), x, y)) {
		m_bEditPasswordRevealed = !m_bEditPasswordRevealed;
		return true;
	}

	m_editNote.m_bFocused = RectContainsPoint(noteField, x, y);
	m_editUsername.m_bFocused = RectContainsPoint(usernameField, x, y);
	m_editPassword.m_bFocused = RectContainsPoint(passwordField, x, y);

	// Click-to-position within a field is not implemented; re-setting the value puts the cursor
	// at the end, which is a reasonable default.
	CTextInput *const fields[]{&m_editNote, &m_editUsername, &m_editPassword};
	for (CTextInput *pField : fields) {
		if (pField->m_bFocused) {
			pField->SetValue(pField->GetValue());
		}
	}

	const Rect save = PrimaryButtonRect(layout.Footer);

	if (RectContainsPoint(EditCancelButtonRect(save), x, y)) {
		m_mode = EAccountModalMode::AccountList;
	} else if (CanSaveEditedAccount() && RectContainsPoint(save, x, y)) {
		SaveEditedAccount();
	} else if (m_nEditAccountIndex >= 0 && RectContainsPoint(EditDeleteButtonRect(layout.Footer), x, y)) {
		if (m_editDeleteConfirm.ClickArmedOrCommit(0)) {
			DeleteEditedAccount();
			Notify("Account deleted.");
		} else {
			NotifyDeleteArmed();
		}
	}

	return true;
}

bool CAccountModal::OnPointerUp(float x, float y)
{
	if (m_accountsScroll.IsDragging()) {
		m_accountsScroll.OnPointerUp();
		return true;
	}

	if (!IsBlocking()) return false;

	const Layout layout = ComputeLayout();

	// A login in flight cannot be dismissed by clicking away; Cancel is the only way out.
	const bool loggingIn = m_mode == EAccountModalMode::LoginProgress;

	if (!loggingIn &&
		(RectContainsPoint(CloseBadgeRect(layout.Left), x, y) || !RectContainsPoint(layout.Panel, x, y))) {
		Close();
		return true;
	}

	switch (m_mode) {
		case EAccountModalMode::AccountList:
			return !HasValidBanner() || HandleAccountListClick(layout, x, y);

		case EAccountModalMode::LoginProgress:
			return HandleLoginProgressClick(layout, x, y);

		case EAccountModalMode::EditAccount:
			return HandleEditAccountClick(layout, x, y);
	}

	return true;
}

ECursorKind CAccountModal::AccountListCursor(const Layout &layout) const
{
	if (RectContainsPoint(AddAccountButtonRect(layout.Right), m_flMouseX, m_flMouseY)) return ECursorKind::Hand;

	AccountListView view;
	if (!BuildAccountListView(layout, view)) return ECursorKind::Arrow;

	if (RectContainsPoint(view.ScrollRegion, m_flMouseX, m_flMouseY)) {
		if (HitTestRow(layout, view, m_flMouseX, m_flMouseY) >= 0) return ECursorKind::Hand;

		if (CScrollable::IsVisible(view.ContentHeight, view.ScrollRegion.H) &&
			RectContainsPoint(view.ScrollbarTrack, m_flMouseX, m_flMouseY)) {
			return ECursorKind::Hand;
		}
	}

	if (m_nSelectedAccountIndex >= 0 && !m_login.IsActive() &&
		RectContainsPoint(PrimaryButtonRect(layout.Footer), m_flMouseX, m_flMouseY)) {
		return ECursorKind::Hand;
	}

	return ECursorKind::Arrow;
}

ECursorKind CAccountModal::EditAccountCursor(const Layout &layout) const
{
	// Consulted directly and ahead of the form underneath, exactly as the click path does.
	if (m_gameSelect.IsBlocking()) return m_gameSelect.GetDesiredCursor();

	if (RectContainsPoint(VisibilityChipRect(layout.Right), m_flMouseX, m_flMouseY)) return ECursorKind::Hand;

	const Rect noteField = EditFieldInputRectAt(layout.Right, 0);
	const Rect usernameField = EditFieldInputRectAt(layout.Right, 1);
	const Rect passwordField = EditFieldInputRectAt(layout.Right, 2);

	// The reveal button sits inside the password field, so it has to answer first.
	if (RectContainsPoint(PasswordRevealButtonRect(passwordField), m_flMouseX, m_flMouseY)) {
		return ECursorKind::Hand;
	}

	if (RectContainsPoint(noteField, m_flMouseX, m_flMouseY) ||
		RectContainsPoint(usernameField, m_flMouseX, m_flMouseY) ||
		RectContainsPoint(passwordField, m_flMouseX, m_flMouseY)) {
		return ECursorKind::IBeam;
	}

	const Rect save = PrimaryButtonRect(layout.Footer);

	if (RectContainsPoint(EditCancelButtonRect(save), m_flMouseX, m_flMouseY)) return ECursorKind::Hand;

	if (CanSaveEditedAccount() && RectContainsPoint(save, m_flMouseX, m_flMouseY)) return ECursorKind::Hand;

	if (m_nEditAccountIndex >= 0 && RectContainsPoint(EditDeleteButtonRect(layout.Footer), m_flMouseX, m_flMouseY)) {
		return ECursorKind::Hand;
	}

	return ECursorKind::Arrow;
}

ECursorKind CAccountModal::GetDesiredCursor() const
{
	if (!IsBlocking()) return ECursorKind::Arrow;

	if (m_accountsScroll.IsDragging()) return ECursorKind::Drag;

	const Layout layout = ComputeLayout();

	// The close badge does nothing while a login is in flight, so it should not look clickable.
	if (m_mode != EAccountModalMode::LoginProgress &&
		RectContainsPoint(CloseBadgeRect(layout.Left), m_flMouseX, m_flMouseY)) {
		return ECursorKind::Hand;
	}

	switch (m_mode) {
		case EAccountModalMode::AccountList:
			return HasValidBanner() ? AccountListCursor(layout) : ECursorKind::Arrow;

		case EAccountModalMode::LoginProgress:
			return RectContainsPoint(PrimaryButtonRect(layout.Footer), m_flMouseX, m_flMouseY) ? ECursorKind::Hand
																							   : ECursorKind::Arrow;

		case EAccountModalMode::EditAccount:
			return EditAccountCursor(layout);
	}

	return ECursorKind::Arrow;
}

bool CAccountModal::OnRightPointerUp(float x, float y)
{
	if (!IsBlocking()) return false;

	if (m_mode != EAccountModalMode::AccountList || !HasValidBanner()) return true;

	const Layout layout = ComputeLayout();

	AccountListView view;
	if (!BuildAccountListView(layout, view)) return true;

	m_pendingRightClickRow = PendingHitFromHitTest(HitTestRow(layout, view, x, y));

	return true;
}

bool CAccountModal::OnScroll(float x, float y, float wheelDelta)
{
	if (!IsBlocking()) return false;

	if (m_mode != EAccountModalMode::AccountList) return true;

	const Layout layout = ComputeLayout();

	AccountListView view;
	if (BuildAccountListView(layout, view)) {
		m_accountsScroll.OnScroll(wheelDelta, view.ContentHeight, view.ScrollRegion.H);
	}

	return true;
}

bool CAccountModal::OnKeyDown(u32 keyCode)
{
	if (!IsBlocking()) return false;

	if (keyCode == VK_ESCAPE) {
		if (m_mode == EAccountModalMode::EditAccount) {
			// Backs out to the list rather than closing the modal outright.
			m_mode = EAccountModalMode::AccountList;
		} else if (m_mode == EAccountModalMode::AccountList) {
			if (m_nSelectedAccountIndex >= 0) {
				m_nSelectedAccountIndex = -1;
			} else {
				Close();
			}
		}

		return true;
	}

	if (m_mode == EAccountModalMode::AccountList && HandleAccountListKey(keyCode)) return true;

	if (m_mode != EAccountModalMode::EditAccount) return true;

	// Tab walks the fields in the order they are drawn, wrapping around.
	if (keyCode == VK_TAB) {
		if (m_editNote.m_bFocused) {
			m_editNote.m_bFocused = false;
			m_editUsername.m_bFocused = true;
		} else if (m_editUsername.m_bFocused) {
			m_editUsername.m_bFocused = false;
			m_editPassword.m_bFocused = true;
		} else if (m_editPassword.m_bFocused) {
			m_editPassword.m_bFocused = false;
			m_editNote.m_bFocused = true;
		}

		return true;
	}

	// Each is a no-op on an unfocused field, so calling all three routes to whichever is
	// focused without an if-chain.
	m_editUsername.OnKey(keyCode);
	m_editNote.OnKey(keyCode);
	m_editPassword.OnKey(keyCode);

	return true;
}

bool CAccountModal::OnChar(u32 character)
{
	if (!IsBlocking()) return false;

	if (m_mode != EAccountModalMode::EditAccount) return true;

	m_editUsername.OnChar(character);
	m_editNote.OnChar(character);
	m_editPassword.OnChar(character);

	return true;
}

void CAccountModal::DrawSectionTitle(CDrawList &drawList, Rect right, std::string_view title, u8 alpha) const
{
	const CFont &body = m_fonts.GetBody();
	const float headerHeight = HeaderHeightFor(m_fonts);
	const float baselineY = right.Y + headerHeight * 0.5f + (body.GetAscent() + body.GetDescent()) * 0.5f;

	DrawText(drawList, body, right.X + kRowPadding, baselineY, title, ColorScaleAlpha(kColorTextBright, alpha));

	// Closes the header off from whatever is below it.
	drawList.AddRectFilled(right.X + kRowPadding, right.Y + headerHeight, right.W - kRowPadding * 2.0f, 1.0f,
						   ColorScaleAlpha(kColorSeparator, alpha));
}

void CAccountModal::DrawAddAccountButton(CDrawList &drawList, Rect right, u8 alpha) const
{
	constexpr float kIconSize = 24.0f;

	const Rect button = AddAccountButtonRect(right);
	const bool hovered = RectContainsPoint(button, m_flMouseX, m_flMouseY);

	if (hovered) {
		Controls::DrawCircularHover(drawList, button, m_settings.m_clrAccent, kColorHoverBadge, alpha);
	}

	const Rect icon{button.X + (button.W - kIconSize) * 0.5f, button.Y + (button.H - kIconSize) * 0.5f, kIconSize,
					kIconSize};
	Controls::DrawIcon(drawList, icon, m_assets.Get(EAsset::IconAdd),
					   ColorScaleAlpha(hovered ? kColorTextBright : kColorTextDim, alpha));
}

void CAccountModal::DrawAccountRow(CDrawList &drawList, Rect right, Rect row, const Account &account, bool isSelected,
								   float deleteArmedAmount, u8 alpha) const
{
	// Inset rather than bled past the row's top, so the first row's highlight is not clipped.
	const Rect hoverRect{row.X - 8.0f, row.Y + 3.0f, row.W + 16.0f, row.H - 6.0f};
	const bool hovered = !isSelected && RectContainsPoint(hoverRect, m_flMouseX, m_flMouseY);

	if (isSelected) {
		// The left indicator bar is the only accent-coloured part of a selected row.
		drawList.AddRectRoundedFilled(hoverRect.X, hoverRect.Y, hoverRect.W, hoverRect.H,
									  CDrawList::UniformRadii(10.0f), ColorScaleAlpha(kColorRowSelected, alpha));
		drawList.AddRectRoundedFilled(right.X + 8.0f, hoverRect.Y, 3.0f, hoverRect.H, CDrawList::UniformRadii(1.5f),
									  ColorScaleAlpha(m_settings.m_clrAccent, alpha));
	} else if (hovered) {
		drawList.AddRectRoundedFilled(hoverRect.X, hoverRect.Y, hoverRect.W, hoverRect.H,
									  CDrawList::UniformRadii(10.0f), ColorScaleAlpha(kColorRowHover, alpha));
	}

	const CFont &body = m_fonts.GetBody();
	const CFont &secondary = m_fonts.GetSecondary();
	const std::string_view note = account.GetNote();

	// A row with a note centres the username-and-note pair as a block; one without centres the
	// username alone, level with the buttons on the right. The two therefore sit the username's
	// baseline in different places, which is the better trade: a lone username pinned to the
	// block position sits visibly high of the icons beside it.
	const float blockHeight = body.GetLineHeight() + kRowLineGap + secondary.GetLineHeight();
	const float blockY = row.Y + (row.H - blockHeight) * 0.5f;
	const float centeredBaselineY = row.Y + row.H * 0.5f + (body.GetAscent() + body.GetDescent()) * 0.5f;
	const float usernameBaselineY = note.empty() ? centeredBaselineY : blockY + body.GetAscent();

	DrawText(drawList, body, row.X, usernameBaselineY, account.GetUsername(), ColorScaleAlpha(kColorTextBright, alpha));

	if (!note.empty()) {
		const float noteBaselineY = blockY + body.GetLineHeight() + kRowLineGap + secondary.GetAscent();
		DrawText(drawList, secondary, row.X, noteBaselineY, note, ColorScaleAlpha(kColorTextDim, alpha));
	}

	drawList.AddRectFilled(row.X, row.Y + row.H - 1.0f, row.W, 1.0f, ColorScaleAlpha(kColorSeparator, alpha));

	const Rect editRect = RowEditButtonRect(row);
	const Rect removeRect = RowRemoveButtonRect(row);
	const bool hoverEdit = RectContainsPoint(editRect, m_flMouseX, m_flMouseY);
	const bool hoverRemove = RectContainsPoint(removeRect, m_flMouseX, m_flMouseY);

	// Armed, the round X badge grows into a labelled red pill saying what the next click does. A
	// colour change alone was not enough - it reads as a hover state, which is exactly the thing it
	// must not be confused with.
	if (deleteArmedAmount > 0.01f) {
		const Rect pill = RowConfirmDeleteRect(row);
		const auto armedAlpha = static_cast<u8>(static_cast<float>(alpha) * deleteArmedAmount);
		const auto fadingAlpha = static_cast<u8>(static_cast<float>(alpha) * (1.0f - deleteArmedAmount));

		drawList.AddRectRoundedFilled(pill.X, pill.Y, pill.W, pill.H, CDrawList::UniformRadii(pill.H * 0.5f),
									  ColorScaleAlpha(kColorError, armedAlpha));
		DrawCenteredText(drawList, m_fonts.GetSecondary(), pill.X, pill.Y, pill.W, pill.H, "Delete?",
						 ColorScaleAlpha(ColorForegroundOn(kColorError), armedAlpha));

		// The edit button has no meaning while a delete is pending, so it fades out rather than
		// sitting there inviting a click that would only cancel the confirm.
		Controls::DrawIcon(drawList, RectInset(editRect, kRowIconInset), m_assets.Get(EAsset::IconEdit),
						   ColorScaleAlpha(kColorTextDim, fadingAlpha));
		return;
	}

	if (hoverEdit) {
		Controls::DrawCircularHover(drawList, editRect, m_settings.m_clrAccent, kColorHoverBadge, alpha);
	}

	if (hoverRemove) {
		Controls::DrawCircularHover(drawList, removeRect, kColorError, kColorRemoveHoverBg, alpha);
	}

	Controls::DrawIcon(drawList, RectInset(editRect, kRowIconInset), m_assets.Get(EAsset::IconEdit),
					   ColorScaleAlpha(hoverEdit ? kColorTextBright : kColorTextDim, alpha));

	// Remove has no embedded icon yet, so its glyph stays hand-drawn.
	Controls::DrawXGlyph(drawList, removeRect, ColorScaleAlpha(hoverRemove ? kColorError : kColorTextDim, alpha));
}

void CAccountModal::DrawAccountList(CDrawList &drawList, const Layout &layout, u8 alpha) const
{
	DrawSectionTitle(drawList, layout.Right, "Accounts", alpha);
	DrawAddAccountButton(drawList, layout.Right, alpha);

	AccountListView view;
	if (!BuildAccountListView(layout, view)) return;

	// Real GPU-side clipping, so a row scrolled halfway behind the header genuinely cannot
	// paint outside the region.
	drawList.PushClipRect(view.ScrollRegion);

	for (u32 i = 0; i < view.Count; i += 1) {
		const Rect row = RowRectAt(layout, view, i);
		if (!IsRowVisible(view, row)) continue;

		const bool isSelected = static_cast<i32>(i) == m_nSelectedAccountIndex;
		DrawAccountRow(drawList, layout.Right, row, AccountAt(view, i), isSelected,
					   m_rowDeleteConfirm.ArmedAmount(static_cast<i32>(i)), alpha);
	}

	drawList.PopClipRect();

	m_accountsScroll.DrawEdgeFade(drawList, view.ScrollRegion, view.ContentHeight, view.ScrollRegion.H,
								  ColorScaleAlpha(kColorPanelBg, alpha));
	m_accountsScroll.Draw(drawList, view.ScrollbarTrack, view.ContentHeight, view.ScrollRegion.H,
						  ColorScaleAlpha(kColorScrollThumb, alpha), m_flMouseX, m_flMouseY);
}

void CAccountModal::DrawLoginProgress(CDrawList &drawList, Rect right, u8 alpha) const
{
	const ELoginStage stage = m_login.GetStage();

	// A queued restart is still in flight to the user, so the attempt it cancelled must not
	// flash its terminal ring on the way through.
	const bool pending = HasPendingLogin();
	const bool terminal = !pending && CLoginAttempt::IsTerminalStage(stage);

	const float cx = right.X + right.W * 0.5f;
	const float cy = right.Y + right.H * 0.5f - 24.0f;

	Color stageColor = m_settings.m_clrAccent;
	if (terminal && stage == ELoginStage::Success) {
		stageColor = kColorSuccess;
	} else if (terminal && stage == ELoginStage::Error) {
		stageColor = kColorError;
	}

	// A breathing bloom while in flight; a terminal stage holds it steady so the ring reads as
	// settled the instant it lands.
	const float glowPulse = terminal ? 1.0f : 0.55f + 0.45f * (0.5f + 0.5f * std::sin(m_flLoginElapsedSeconds * 2.6f));
	const float glowStrength = 0.85f * glowPulse * (static_cast<float>(alpha) / 255.0f);

	// A terminal stage draws a full solid ring; an in-flight one draws a rotating comet tail.
	// Either way this is one draw call, with the shader doing the rest per pixel.
	const float sweepAngleDeg = terminal ? kIndicatorFullSweepDeg : kIndicatorSweepDeg;
	const float spin = std::fmod(m_flLoginElapsedSeconds * kIndicatorRotationDegPerSec, 360.0f);
	const float startAngleDeg = terminal ? 0.0f : spin - 90.0f - kIndicatorSweepDeg;

	drawList.AddCircularProgress(cx, cy, kIndicatorOuterRadius, kIndicatorInnerRadius, kIndicatorGlowMargin,
								 startAngleDeg, sweepAngleDeg, glowStrength, ColorScaleAlpha(stageColor, alpha));

	if (terminal) {
		const Color glyphColor = ColorScaleAlpha(kColorTextBright, alpha);

		if (stage == ELoginStage::Success) {
			drawList.AddLine(cx - 13.0f, cy, cx - 3.0f, cy + 11.0f, 4.0f, glyphColor);
			drawList.AddLine(cx - 3.0f, cy + 11.0f, cx + 15.0f, cy - 11.0f, 4.0f, glyphColor);
		} else {
			drawList.AddLine(cx - 11.0f, cy - 11.0f, cx + 11.0f, cy + 11.0f, 4.0f, glyphColor);
			drawList.AddLine(cx - 11.0f, cy + 11.0f, cx + 11.0f, cy - 11.0f, 4.0f, glyphColor);
		}
	}

	// The attempt's message - Riot's error text, or whatever else the worker set - replaces the
	// generic stage label as soon as there is one, and is empty until then.
	std::string_view message = pending ? "Switching account..." : LoginStageMessage(stage);
	if (terminal && !m_login.GetTerminalMessage().empty()) {
		message = m_login.GetTerminalMessage();
	}

	// Wrapped: a real backend error is wider than this column at any normal font size.
	const CFont &body = m_fonts.GetBody();
	const float maxMessageWidth = std::max(right.W - kRowPadding * 2.0f, 40.0f);

	std::string_view messageLines[kMaxLoginMessageLines];
	const u32 lineCount = WrapText(body, message, maxMessageWidth, messageLines, kMaxLoginMessageLines);

	float lineY = cy + kIndicatorOuterRadius + 40.0f;
	for (u32 i = 0; i < lineCount; i += 1) {
		DrawText(drawList, body, cx - TextWidth(body, messageLines[i]) * 0.5f, lineY, messageLines[i],
				 ColorScaleAlpha(kColorTextBright, alpha));
		lineY += body.GetLineHeight();
	}
}

void CAccountModal::DrawEditField(CDrawList &drawList, Rect block, const char *pLabel, const CTextInput &input,
								  bool masked, u8 alpha) const
{
	const CFont &secondary = m_fonts.GetSecondary();
	DrawText(drawList, secondary, block.X, block.Y + secondary.GetAscent(), pLabel,
			 ColorScaleAlpha(kColorTextFaint, alpha));

	// A neutral focus ring rather than an accent one: a typed value's box only needs to show
	// which field is active.
	const Rect field = EditFieldInputRect(block);
	DrawFieldChrome(drawList, field, input.m_bFocused, alpha);

	// Body, not secondary - this is the value the user is reading and typing.
	input.Draw(drawList, m_fonts.GetBody(), field.X, field.Y, field.W, field.H,
			   ColorScaleAlpha(kColorTextBright, alpha), ColorScaleAlpha(kColorTextBright, alpha), masked);
}

// A pill carrying an icon and the visible-game count, in the header opposite the title. Always
// bordered and filled, like an unfocused field: a chip only visible on hover is easy to miss.
void CAccountModal::DrawVisibilityChip(CDrawList &drawList, Rect right, u8 alpha) const
{
	const Rect chip = VisibilityChipRect(right);
	const bool hovered = RectContainsPoint(chip, m_flMouseX, m_flMouseY);
	const Color contentColor = ColorScaleAlpha(hovered ? kColorTextBright : kColorTextDim, alpha);

	DrawFieldChrome(drawList, chip, hovered, alpha);

	const Rect icon{chip.X + kVisibilityChipIconInset, chip.Y + (chip.H - kVisibilityChipIconSize) * 0.5f,
					kVisibilityChipIconSize, kVisibilityChipIconSize};
	Controls::DrawIcon(drawList, icon, m_assets.Get(EAsset::IconListArrow), contentColor);

	char countBuffer[16];
	const std::string_view countText = FormatVisibleGameCount(m_gameSelect.GetMask(), countBuffer, sizeof(countBuffer));

	const CFont &secondary = m_fonts.GetSecondary();
	const float baselineY = chip.Y + chip.H * 0.5f + (secondary.GetAscent() + secondary.GetDescent()) * 0.5f;
	DrawText(drawList, secondary, icon.X + icon.W + kVisibilityChipIconGap, baselineY, countText, contentColor);
}

void CAccountModal::DrawEditAccount(CDrawList &drawList, Rect right, u8 alpha)
{
	DrawSectionTitle(drawList, right, m_nEditAccountIndex < 0 ? "Add Account" : "Edit Account", alpha);

	// Note first: it is what identifies an account to the person reading the list. The block
	// index is the only thing that orders these, and every hit-test reads the same indices.
	DrawEditField(drawList, EditFieldBlockRect(right, 0), "Note", m_editNote, false, alpha);
	DrawEditField(drawList, EditFieldBlockRect(right, 1), "Username", m_editUsername, false, alpha);
	DrawEditField(drawList, EditFieldBlockRect(right, 2), "Password", m_editPassword, !m_bEditPasswordRevealed, alpha);

	const Rect reveal = PasswordRevealButtonRect(EditFieldInputRectAt(right, 2));
	const bool hoverReveal = RectContainsPoint(reveal, m_flMouseX, m_flMouseY);
	Controls::DrawEyeGlyph(drawList, m_assets, reveal, m_bEditPasswordRevealed,
						   ColorScaleAlpha(hoverReveal ? kColorTextBright : kColorTextDim, alpha));

	DrawVisibilityChip(drawList, right, alpha);

	// After the fields, so its popup layers over them.
	m_gameSelect.Draw(drawList);
}

// While Save is disabled the helper line says why, rather than describing the form in general.
void CAccountModal::DrawEditFooter(CDrawList &drawList, Rect footer, u8 alpha)
{
	const CFont &body = m_fonts.GetBody();
	const CFont &secondary = m_fonts.GetSecondary();
	const bool editingExisting = m_nEditAccountIndex >= 0;

	std::string_view helperText = editingExisting ? "Edit the account's details" : "Fill in the new account's details";

	if (!CanSaveEditedAccount()) {
		const bool noUsername = m_editUsername.GetValue().empty();
		const bool noPassword = m_editPassword.GetValue().empty();
		helperText = noUsername && noPassword ? "Username and password are required"
											  : (noUsername ? "Username is required" : "Password is required");
	}

	// Starts past the Delete button when it is showing, so the two never overlap.
	const float helperX =
		editingExisting ? EditDeleteButtonRect(footer).X + kActionButtonWidth + kRowPadding : footer.X + kRowPadding;
	const float baselineY = footer.Y + footer.H * 0.5f + (secondary.GetAscent() + secondary.GetDescent()) * 0.5f;
	DrawText(drawList, secondary, helperX, baselineY, helperText, ColorScaleAlpha(kColorTextFaint, alpha));

	const Rect save = PrimaryButtonRect(footer);
	const bool saveEnabled = CanSaveEditedAccount();
	const bool hoverSave = saveEnabled && RectContainsPoint(save, m_flMouseX, m_flMouseY);
	DrawAccentButton(drawList, body, save, "Save", m_settings.m_clrAccent, saveEnabled, hoverSave, alpha);

	const Rect cancel = EditCancelButtonRect(save);
	DrawNeutralButton(drawList, body, cancel, "Cancel", kColorTextBright, kColorNeutralButton, kColorNeutralButtonHover,
					  RectContainsPoint(cancel, m_flMouseX, m_flMouseY), alpha);

	if (editingExisting) {
		// Armed, the button stops being a neutral one with red text and becomes a solid red one
		// asking a question. Nothing else on screen changes, which is the point of a confirm that
		// lives in the button rather than in a dialog.
		const Rect del = EditDeleteButtonRect(footer);
		const float armed = m_editDeleteConfirm.ArmedAmount(0);
		const Color fill = ColorLerp(kColorDeleteButton, kColorError, armed);
		const Color hoverFill = ColorLerp(ColorLighten(kColorError, 20), ColorLighten(kColorError, 40), armed);
		const Color label = ColorLerp(kColorError, ColorForegroundOn(kColorError), armed);

		DrawNeutralButton(drawList, body, del, armed > 0.5f ? "Delete?" : "Delete", label, fill, hoverFill,
						  RectContainsPoint(del, m_flMouseX, m_flMouseY), alpha);

		// A ring around the armed button, so it reads as a live prompt rather than a button that
		// merely got redder. Drawn over the fill, inset by its own thickness so it sits inside.
		if (armed > 0.01f) {
			constexpr float kArmedRingThickness = 1.5f;

			drawList.AddRectRoundedBordered(
				del.X, del.Y, del.W, del.H, CDrawList::UniformRadii(8.0f), Color{0, 0, 0, 0},
				ColorScaleAlpha(ColorLighten(kColorError, 60), static_cast<u8>(alpha * armed)), kArmedRingThickness);
		}
	}
}

void CAccountModal::DrawLoginProgressFooter(CDrawList &drawList, Rect footer, u8 alpha)
{
	const CFont &body = m_fonts.GetBody();
	const CFont &secondary = m_fonts.GetSecondary();
	const bool terminal = !HasPendingLogin() && CLoginAttempt::IsTerminalStage(m_login.GetStage());

	const float baselineY = footer.Y + footer.H * 0.5f + (secondary.GetAscent() + secondary.GetDescent()) * 0.5f;
	DrawText(drawList, secondary, footer.X + kRowPadding, baselineY, terminal ? "" : "Logging in...",
			 ColorScaleAlpha(kColorTextFaint, alpha));

	// The same rect the Login button occupies in the list, and the same action: back to the list.
	const Rect button = PrimaryButtonRect(footer);
	DrawNeutralButton(drawList, body, button, terminal ? "Back" : "Cancel", kColorTextBright, kColorNeutralButton,
					  kColorNeutralButtonHover, RectContainsPoint(button, m_flMouseX, m_flMouseY), alpha);
}

void CAccountModal::DrawAccountListFooter(CDrawList &drawList, Rect footer, u8 alpha)
{
	const CFont &body = m_fonts.GetBody();
	const CFont &secondary = m_fonts.GetSecondary();

	const float baselineY = footer.Y + footer.H * 0.5f + (secondary.GetAscent() + secondary.GetDescent()) * 0.5f;
	DrawText(drawList, secondary, footer.X + kRowPadding, baselineY, "Select an account to log in",
			 ColorScaleAlpha(kColorTextFaint, alpha));

	// Not gated on an attempt being active: pressing Login during one replaces it.
	const Rect button = PrimaryButtonRect(footer);
	const bool enabled = m_nSelectedAccountIndex >= 0;
	const bool hovered = enabled && RectContainsPoint(button, m_flMouseX, m_flMouseY);
	DrawAccentButton(drawList, body, button, "Login", m_settings.m_clrAccent, enabled, hovered, alpha);
}

void CAccountModal::DrawFooter(CDrawList &drawList, Rect footer, u8 alpha)
{
	drawList.AddRectFilled(footer.X, footer.Y, footer.W, 1.0f, ColorScaleAlpha(kColorSeparator, alpha));

	switch (m_mode) {
		case EAccountModalMode::EditAccount:
			DrawEditFooter(drawList, footer, alpha);
			break;

		case EAccountModalMode::LoginProgress:
			DrawLoginProgressFooter(drawList, footer, alpha);
			break;

		case EAccountModalMode::AccountList:
			DrawAccountListFooter(drawList, footer, alpha);
			break;
	}
}

void CAccountModal::DrawPanelChrome(CDrawList &drawList, const Layout &layout, const Banner &banner, u8 alpha) const
{
	Controls::DrawPanelShadow(drawList, layout.Panel, kPanelRadius, m_flOpenAmount);

	drawList.AddRectRoundedFilled(layout.Panel.X, layout.Panel.Y, layout.Panel.W, layout.Panel.H,
								  CDrawList::UniformRadii(kPanelRadius), ColorScaleAlpha(kColorPanelBorder, alpha));
	drawList.AddRectRoundedFilled(layout.Inner.X, layout.Inner.Y, layout.Inner.W, layout.Inner.H,
								  CDrawList::UniformRadii(kPanelRadius - kPanelBorderThickness),
								  ColorScaleAlpha(kColorPanelBg, alpha));

	// A faint top-edge highlight, which reads better than a drop shadow against a dark backdrop.
	// Inset by the effective corner radius so it stops exactly where the curve starts.
	const float highlightInset = CDrawList::ScaledRadius(kPanelRadius);
	drawList.AddRectFilled(layout.Inner.X + highlightInset, layout.Inner.Y, layout.Inner.W - highlightInset * 2.0f,
						   1.0f, ColorScaleAlpha(Color{255, 255, 255, 22}, alpha));

	// Rounded only on its top-left, matching the panel's corner there. Through the scaled radii,
	// so an art corner cannot stay notched while the panel squares off.
	const Rect left = layout.Left;
	if (banner.pTexture != nullptr) {
		const CornerRadii imageRadii = CDrawList::Radii(kPanelRadius - kPanelBorderThickness, 0.0f, 0.0f, 0.0f);
		const UvRect uv = CDrawList::ComputeCoverUv(left.W, left.H, banner.TextureAspect, 1.0f);

		drawList.AddRectRoundedTexturedUv(left.X, left.Y, left.W, left.H, imageRadii, uv.U0, uv.V0, uv.U1, uv.V1,
										  banner.pTexture, ColorScaleAlpha(kColorWhite, alpha));
	} else {
		drawList.AddRectFilled(left.X, left.Y, left.W, left.H, ColorScaleAlpha(banner.Accent, alpha));
	}

	drawList.AddRectFilled(left.X + left.W, left.Y, kSeparatorThickness, left.H,
						   ColorScaleAlpha(Color{90, 90, 96, 255}, alpha));

	// Close: a back arrow in a circular dark badge floating over the art's top-left corner.
	const Rect badge = CloseBadgeRect(left);
	const bool hovered = RectContainsPoint(badge, m_flMouseX, m_flMouseY);
	const auto badgeAlpha = static_cast<u8>(hovered ? 210 : 170);

	drawList.AddRectRoundedFilled(badge.X, badge.Y, badge.W, badge.H, CDrawList::UniformRadii(badge.W * 0.5f),
								  ColorScaleAlpha(Color{20, 20, 22, badgeAlpha}, alpha));

	const Rect badgeIcon{badge.X + (badge.W - kCloseBadgeIconSize) * 0.5f,
						 badge.Y + (badge.H - kCloseBadgeIconSize) * 0.5f, kCloseBadgeIconSize, kCloseBadgeIconSize};
	Controls::DrawIcon(drawList, badgeIcon, m_assets.Get(EAsset::IconArrowBack), ColorScaleAlpha(kColorWhite, alpha));
}

void CAccountModal::Draw(CDrawList &drawList)
{
	PULSAR_PROFILE_SCOPE("AccountModal.Draw");

	if (m_flOpenAmount <= 0.001f || !HasValidBanner()) return;

	const auto alpha = static_cast<u8>(255.0f * m_flOpenAmount);
	const auto windowW = static_cast<float>(m_window.GetWidth());
	const auto windowH = static_cast<float>(m_window.GetHeight());

	drawList.AddRectFilled(0.0f, 0.0f, windowW, windowH, Color{0, 0, 0, static_cast<u8>(160.0f * m_flOpenAmount)});

	const Layout layout = ComputeLayout();
	DrawPanelChrome(drawList, layout, m_pCarousel->GetBanner(static_cast<u32>(m_nBannerIndex)), alpha);

	switch (m_mode) {
		case EAccountModalMode::AccountList:
			DrawAccountList(drawList, layout, alpha);
			break;

		case EAccountModalMode::LoginProgress:
			DrawLoginProgress(drawList, layout.Right, alpha);
			break;

		case EAccountModalMode::EditAccount:
			DrawEditAccount(drawList, layout.Right, alpha);
			break;
	}

	DrawFooter(drawList, layout.Footer, alpha);
}
