#include "app.h"

#include "core/str.h"

#include <chrono>
#include <cstdio>
#include <memory>
#include <print>

#include <Windows.h>
#include <shellapi.h>
#include <sodium.h>

#include "core/animator.h"
#include "core/debug_log.h"
#include "core/profiler.h"
#include "core/ui_automation.h"
#include "core/app_identity.h"
#include "platform/clipboard.h"
#include "ui/controls.h"
#include "ui/text.h"

namespace {
constexpr u32 kDrawListVertexCapacity = 1 << 16;
constexpr u32 kDrawListIndexCapacity = (1 << 16) * 3 / 2;

// The whole process's arena, used for exactly one thing: the draw list's per-frame scratch. Sized
// from that one consumer rather than picked as a round number - it was 64 MB against a real need
// of under two, and malloc commits what it hands back, so the difference was 62 MB of commit
// charge the app never touched.
constexpr u64 kArenaAlignmentSlack = 64;
constexpr u64 kPersistentArenaCapacity = static_cast<u64>(kDrawListVertexCapacity) * sizeof(Vertex2D) +
										 static_cast<u64>(kDrawListIndexCapacity) * sizeof(u32) + kArenaAlignmentSlack;

constexpr ColorF kColorBackground{18.0f / 255.0f, 18.0f / 255.0f, 20.0f / 255.0f, 1.0f};

// A 1px seam reads as clearly as a much bigger, uglier brightness jump would.
constexpr Color kColorChromeSeam{46, 46, 50, 255};
constexpr Color kColorStatusBarText{158, 158, 166, 255};
constexpr float kStatusBarVersionPadding = 14.0f;

// The app mark beside the version. Sized under the bar height so it reads as sitting in the strip
// rather than filling it, and tinted the same dim grey as the text so the two read as one label.
constexpr float kStatusBarMarkSize = 15.0f;
constexpr float kStatusBarMarkGap = 7.0f;

constexpr u32 kMenuIdCopyUsername = 1;
constexpr u32 kMenuIdCopyPassword = 2;

// Everything one game needs, so adding a game means adding one row here. A second list in the
// same order is how a tray ends up drawing one game's icon beside another's name.
struct GameEntry {
	const char *pTitle;
	EAsset Banner;
	EAsset Icon;
	Color Accent;
};

constexpr GameEntry kGames[]{
	{"League of Legends", EAsset::BannerLeagueOfLegends, EAsset::IconLeagueOfLegends, {210, 175, 55, 255}},
	{"Teamfight Tactics", EAsset::BannerTeamfightTactics, EAsset::IconTeamfightTactics, {70, 140, 190, 255}},
	{"Valorant", EAsset::BannerValorant, EAsset::IconValorant, {210, 55, 60, 255}},
	{"2XKO", EAsset::BannerTwoXko, EAsset::IconTwoXko, {45, 205, 210, 255}},
	{"Legends of Runeterra", EAsset::BannerRuneterra, EAsset::IconRuneterra, {140, 90, 200, 255}},
};

constexpr u32 kGameCount = sizeof(kGames) / sizeof(kGames[0]);

// Startup is one straight line of work, so timing it needs nothing more than a running clock and a
// name at each boundary: the gap between two calls is the phase that just finished. Unlike the
// frame profiler this is left in every build - it is a few clock reads, and DebugLog::Write is a
// single predictable branch when the log is off, so the numbers stay measurable where they matter.
void StartupPhase(const char *pName)
{
	using Clock = std::chrono::steady_clock;

	static Clock::time_point processStart = Clock::now();
	static Clock::time_point phaseStart = processStart;
	static const char *pPending = nullptr;

	const Clock::time_point now = Clock::now();

	if (pPending != nullptr) {
		DebugLog::Write("startup", "%-20s %6.1f ms   (%6.1f ms in)", pPending,
						std::chrono::duration<float, std::milli>(now - phaseStart).count(),
						std::chrono::duration<float, std::milli>(now - processStart).count());
	}

	pPending = pName;
	phaseStart = now;
}

static_assert(kGameCount <= kCarouselMaxBanners, "more games than the carousel can hold");
static_assert(kTrayMaxGames >= kCarouselMaxBanners, "the tray would silently drop games the carousel accepts");
} // namespace

CApp::EStartResult CApp::Init()
{
	// After the crash handler and the log, so a duplicate launch still gets reported.
	if (!m_instanceGuard.IsFirstInstance()) {
		const bool activated = CWindow::ActivateExistingInstance();
		DebugLog::Write("app", "another instance is already running (%s) - exiting",
						activated ? "brought it to the front" : "it never answered");

		return EStartResult::AlreadyRunning;
	}

	// First thing of all: decoding needs no window and no GPU, so it runs alongside the quarter
	// second of driver work that D3D device creation costs and ends up free.
	m_assets.BeginDecode();

	// Read before anything else settings-shaped: the window needs a size, and the overlay guard
	// needs its own switch, both before the real load - which needs a carousel that does not exist
	// yet - can run.
	Settings bootSettings;
	i32 unusedZoomStop = 0;
	i32 unusedSelectedBanner = 0;

	StartupPhase("BootSettings");
	CStorage::LoadSettings(bootSettings, unusedZoomStop, unusedSelectedBanner);

	StartupPhase("ApplyOverlayGuard");
	ApplyOverlayGuard(bootSettings.m_bBlockOverlayInjection);

	// Holds the process's MTA open so the per-attempt COM init and teardown stop rebuilding the
	// whole apartment every time. Joins no apartment itself, so the render thread stays free.
	StartupPhase("KeepProcessMtaAlive");
	CUiAutomation::KeepProcessMtaAlive();

	// Not safe to call concurrently with any other libsodium function, so it happens here, before
	// any worker thread exists.
	if (sodium_init() < 0) {
		std::println("Failed to initialize libsodium.");
		return EStartResult::Failed;
	}

	if (!m_arena.Init(kPersistentArenaCapacity)) {
		std::println("Failed to reserve persistent arena.");
		return EStartResult::Failed;
	}

	StartupPhase("InitGraphics");
	if (!InitGraphics(bootSettings)) return EStartResult::Failed;

	StartupPhase("tray.Create");
	m_tray.Create(kAppNameW);
	m_drawList.Init(m_arena, kDrawListVertexCapacity, kDrawListIndexCapacity);

	StartupPhase("CreateGames");
	CreateGames();

	StartupPhase("MasterKeyInit");
	m_masterKey.Init();

	// One check at startup, no periodic recheck. The network I/O happens on the worker.
	m_updater.Init();
	m_updater.CheckForUpdateAsync(kAppVersion);

	StartupPhase("LoadAll");
	ApplyLoadedSettings(LoadAll());
	StartupPhase("CreateWidgets");
	CreateWidgets();

	m_window.SetRedrawCallback(OnWindowRedraw, this);
	m_window.SetDpiChangedCallback(OnWindowDpiChanged, this);

	m_startTime = std::chrono::steady_clock::now();
	m_previousFrameTime = m_startTime;

	// One real frame, laid out and presented while the window is still hidden, so showing it
	// reveals actual UI instead of an uninitialized backbuffer.
	StartupPhase("FirstFrame");
	AdvanceFrame();
	m_window.Show();

	StartupPhase("Done");

	return EStartResult::Ok;
}

// Runs before the window exists, since the injection policy only governs loads that have not
// happened yet, and an overlay that attaches does so once a window is there to draw on.
void CApp::ApplyOverlayGuard(bool blockInjection)
{
	OverlayGuard::ApplyProcessIdentity();

	if (!blockInjection) {
		DebugLog::Write("app", "overlay injection guard disabled by setting");
		return;
	}

	switch (OverlayGuard::BlockHookInjection()) {
		case OverlayGuard::EInjectionBlockResult::Blocked:
			DebugLog::Write("app", "extension-point DLL injection blocked");
			break;

		case OverlayGuard::EInjectionBlockResult::Refused:
			DebugLog::Write("app", "extension-point block refused, err=%lu", GetLastError());
			break;

		case OverlayGuard::EInjectionBlockResult::Unsupported:
			DebugLog::Write("app", "extension-point block unsupported on this Windows build");
			break;
	}

	if (const wchar_t *pModule = OverlayGuard::DetectInjectedOverlay()) {
		DebugLog::Write("app", "an overlay module was already loaded before the guard ran: %ls", pModule);
	}
}

bool CApp::InitGraphics(const Settings &bootSettings)
{
	StartupPhase("  window.Create");
	if (!m_window.Create(kAppNameW, bootSettings.m_nWindowWidth, bootSettings.m_nWindowHeight)) {
		std::println("Failed to create window.");
		return false;
	}

	const RendererConfig config{m_window.GetHandle(), m_window.GetPhysicalWidth(), m_window.GetPhysicalHeight(),
								static_cast<float>(m_window.GetWidth()), static_cast<float>(m_window.GetHeight())};
	StartupPhase("  renderer.Init");
	if (!m_renderer.Init(config)) {
		std::println("Failed to initialize renderer.");
		return false;
	}

	m_nSwapchainWidth = m_window.GetPhysicalWidth();
	m_nSwapchainHeight = m_window.GetPhysicalHeight();

	StartupPhase("  assets.Upload");
	if (!m_assets.FinishUpload(&m_renderer)) {
		std::println("Failed to load one or more embedded assets.");
		return false;
	}

	StartupPhase("  fonts.Load");
	if (!m_fonts.Load(&m_renderer, m_window.GetDpiScale())) {
		std::println("Failed to load the UI font.");
		return false;
	}

	return true;
}

// AddBanner appends, so this iteration's banner index is i - the same index the tray stores its
// icon under, which is why both happen in one loop.
void CApp::CreateGames()
{
	auto pCarousel = std::make_unique<CCarousel>(m_fonts, m_assets);
	m_pCarousel = pCarousel.get();

	for (u32 i = 0; i < kGameCount; i += 1) {
		const GameEntry &game = kGames[i];
		m_pCarousel->AddBanner(game.pTitle, m_assets.Get(game.Banner), m_assets.Get(game.Icon), game.Accent);

		const EmbeddedImageBytes iconBytes = CAssetManager::GetSourceBytes(game.Icon);
		m_tray.SetGameIcon(static_cast<i32>(i), iconBytes.pBytes, iconBytes.Length);
	}

	m_tray.SetMenuCallback(BuildTrayMenu, m_pCarousel);

	m_stack.Push(std::move(pCarousel));
}

// Push order is z-order, bottom to top. The unlock screen sits above every dialog and consumes
// all input while active, so nothing below it needs its own locked-state handling. The app menu
// and update overlay sit above even that, since the title bar opens both whether or not the
// vault is unlocked.
void CApp::CreateWidgets()
{
	// Constructed first: every widget that raises a notification is handed this, so it has to
	// outlive them, and the stack destroys in reverse push order.
	auto pToasts = std::make_unique<CToastHost>(m_fonts, m_window, m_assets, m_settings);
	m_pToasts = pToasts.get();

	auto pModal = std::make_unique<CAccountModal>(m_pCarousel, m_pToasts, m_fonts, m_window, m_settings, m_assets);
	auto pSettingsPanel = std::make_unique<CSettingsPanel>(&m_fonts, &m_settings, m_window, &m_renderer, m_assets);
	auto pContextMenu = std::make_unique<CContextMenu>(m_fonts);
	auto pUnlockScreen = std::make_unique<CUnlockScreen>(m_fonts, m_window, &m_settings, &m_masterKey, m_assets);
	auto pAppMenu = std::make_unique<CAppMenu>(m_fonts, m_assets, m_settings, m_bAppLocked);
	auto pUpdateOverlay = std::make_unique<CUpdateOverlay>(m_fonts, m_window, &m_settings, &m_updater);
	auto pTitleBar = std::make_unique<CTitleBar>(&m_window, m_assets, m_updater, m_fonts);

	m_pModal = pModal.get();
	m_pSettingsPanel = pSettingsPanel.get();
	m_pContextMenu = pContextMenu.get();
	m_pUnlockScreen = pUnlockScreen.get();
	m_pAppMenu = pAppMenu.get();
	m_pUpdateOverlay = pUpdateOverlay.get();
	m_pTitleBar = pTitleBar.get();

	m_stack.Push(std::move(pModal));
	m_stack.Push(std::move(pSettingsPanel));
	m_stack.Push(std::move(pContextMenu));
	m_stack.Push(std::move(pUnlockScreen));
	m_stack.Push(std::move(pAppMenu));
	m_stack.Push(std::move(pUpdateOverlay));

	// Above every dialog, so a confirmation is readable over whatever raised it, but below the
	// title bar, which is the one thing that must never be covered.
	m_stack.Push(std::move(pToasts), true);
	m_stack.Push(std::move(pTitleBar), true);

#ifdef PULSAR_PROFILING
	// Last, so its own draw is the only one not counted in the numbers it reports.
	m_stack.Push(std::make_unique<CProfilerOverlay>(&m_fonts), true);
#endif

	// The master password is mandatory: either none has been set yet, or one exists and has not
	// been unlocked this session.
	if (!m_settings.m_bMasterPasswordEnabled) {
		m_pUnlockScreen->ActivateForSetup();
	} else if (m_bAppLocked) {
		m_pUnlockScreen->ActivateForUnlock();
	}
}

void CApp::ApplyLoadedSettings(EStorageLoadResult loadResult)
{
	m_bAppLocked = !m_settings.m_bMasterPasswordEnabled || loadResult == EStorageLoadResult::Locked;
	m_pCarousel->m_bVisible = !m_bAppLocked;

	if (loadResult != EStorageLoadResult::Ok && loadResult != EStorageLoadResult::Locked) return;

	CAnimator::SetEnabled(m_settings.m_bAnimationsEnabled);
	CAnimator::SetSpeed(m_settings.m_flAnimationSpeed);
	CDrawList::SetCornerRoundnessScale(m_settings.m_flCornerRoundness);
	m_fonts.ApplyBody(&m_renderer, m_settings.m_szFontName, m_settings.m_flFontPixelSize,
					  m_settings.m_flSecondaryFontPixelSize, m_window.GetDpiScale());

	// Latched before the stamp below overwrites it. An empty stored version is a first run, which
	// is not an update and gets nothing.
	m_bJustUpdated = m_settings.m_szLastRunVersion[0] != '\0' &&
					 std::string_view{m_settings.m_szLastRunVersion} != std::string_view{kAppVersion};
	CopyTo(kAppVersion, m_settings.m_szLastRunVersion, sizeof(m_settings.m_szLastRunVersion));
}

void CApp::SaveAccounts()
{
	CStorage::SaveAccounts(&m_pCarousel->GetBanner(0), m_pCarousel->GetBannerCount(),
						   m_settings.m_bMasterPasswordEnabled, m_masterKey);
}

void CApp::SaveSettings()
{
	CStorage::SaveSettings(m_settings, m_pCarousel->GetZoomStop(), m_pCarousel->GetSelectedIndex());
}

void CApp::SaveAll()
{
	SaveAccounts();
	SaveSettings();
}

EStorageLoadResult CApp::LoadAll()
{
	i32 zoomStop = 0;
	i32 selectedBanner = 0;
	const EStorageLoadResult settingsResult = CStorage::LoadSettings(m_settings, zoomStop, selectedBanner);

	m_pCarousel->ApplyZoomStop(zoomStop);
	m_pCarousel->ApplySelectedIndex(selectedBanner);

	const EStorageLoadResult accountsResult = CStorage::LoadAccounts(
		&m_pCarousel->GetBanner(0), m_pCarousel->GetBannerCount(), m_settings.m_bMasterPasswordEnabled, m_masterKey);

	// Settings failing outranks whatever the vault said: the master-password state the accounts
	// result was decided against came from that same unreadable file, so it means nothing here.
	return settingsResult == EStorageLoadResult::Failed ? settingsResult : accountsResult;
}

// Rows come from the visible-account query rather than raw storage, so an account visible under
// a second game appears under that game here too.
void CApp::BuildTrayMenu(void *pUserData, TrayMenuModel &outModel)
{
	const auto *pCarousel = static_cast<const CCarousel *>(pUserData);
	VisibleAccountRef refs[kCarouselMaxVisibleAccounts];

	for (u32 b = 0; b < pCarousel->GetBannerCount() && outModel.GameCount < kTrayMaxGames; b += 1) {
		const u32 visibleCount = pCarousel->GetVisibleAccounts(b, refs);

		TrayGameItem &game = outModel.Games[outModel.GameCount];
		outModel.GameCount += 1;

		CopyTo(pCarousel->GetBanner(b).Title, game.Title, sizeof(game.Title));
		game.BannerIndex = static_cast<i32>(b);
		game.FirstAccount = outModel.AccountCount;
		game.AccountCount = 0;

		for (u32 q = 0; q < visibleCount && outModel.AccountCount < kTrayMaxAccountItems; q += 1) {
			const Account &account = pCarousel->GetBanner(refs[q].BannerIndex).Accounts[refs[q].AccountIndex];

			// The note names the account the way its owner thinks of it.
			const std::string_view note = account.GetNote();
			const std::string_view label = note.empty() ? account.GetUsername() : note;

			TrayAccountItem &item = outModel.Accounts[outModel.AccountCount];
			CopyTo(label, item.Label, sizeof(item.Label));
			item.BannerIndex = static_cast<i32>(b);
			item.QueryIndex = static_cast<i32>(q);

			outModel.AccountCount += 1;
			game.AccountCount += 1;
		}
	}
}

// Serves both a live resize and an ordinary repaint, so the swapchain is only rebuilt when the
// size genuinely changed - ResizeBuffers is not free, and a plain WM_PAINT arrives at the same
// size it left at.
void CApp::OnWindowRedraw(void *pUserData)
{
	auto *pApp = static_cast<CApp *>(pUserData);
	const u32 physicalWidth = pApp->m_window.GetPhysicalWidth();
	const u32 physicalHeight = pApp->m_window.GetPhysicalHeight();

	// A minimized window reports a zero client size, which the swapchain cannot be resized to.
	if (physicalWidth == 0 || physicalHeight == 0) return;

	if (physicalWidth != pApp->m_nSwapchainWidth || physicalHeight != pApp->m_nSwapchainHeight) {
		pApp->m_renderer.Resize(physicalWidth, physicalHeight, static_cast<float>(pApp->m_window.GetWidth()),
								static_cast<float>(pApp->m_window.GetHeight()));
		pApp->m_nSwapchainWidth = physicalWidth;
		pApp->m_nSwapchainHeight = physicalHeight;
	}

	pApp->AdvanceFrame();
}

// Re-bakes both atlases at the new scale before the move that follows repaints the window, so
// that repaint never samples a stale-resolution atlas.
void CApp::OnWindowDpiChanged(void *pUserData)
{
	auto *pApp = static_cast<CApp *>(pUserData);

	pApp->m_fonts.ApplyBody(&pApp->m_renderer, pApp->m_settings.m_szFontName, pApp->m_settings.m_flFontPixelSize,
							pApp->m_settings.m_flSecondaryFontPixelSize, pApp->m_window.GetDpiScale());
}

void CApp::HandleTrayEvent()
{
	switch (m_tray.TakeEvent()) {
		case ETrayEventType::ExitRequested:
			// A real close request, not a posted WM_CLOSE: with close-to-tray on, that would only
			// hide the window, and the tray's Exit means exit.
			m_window.RequestClose();
			break;

		case ETrayEventType::ShowWindow:
			m_window.Restore();
			break;

		case ETrayEventType::QuickLogin: {
			const i32 bannerIndex = m_tray.GetPendingBannerIndex();
			const i32 queryIndex = m_tray.GetPendingAccountIndex();

			// Re-run rather than trusting the index the menu was built from: the account list can
			// have changed while the menu was open.
			bool valid =
				bannerIndex >= 0 && static_cast<u32>(bannerIndex) < m_pCarousel->GetBannerCount() && queryIndex >= 0;

			if (valid) {
				VisibleAccountRef refs[kCarouselMaxVisibleAccounts];
				valid =
					static_cast<u32>(queryIndex) < m_pCarousel->GetVisibleAccounts(static_cast<u32>(bannerIndex), refs);
			}

			if (valid) {
				// No restore - a tray login stays out of the way. The modal still opens behind the
				// scenes, so restoring later lands on its progress view.
				m_pModal->OpenForQuickLogin(bannerIndex, queryIndex);
			}

			break;
		}

		case ETrayEventType::None:
			break;
	}
}

void CApp::HandleTitleBarButtons()
{
	if (m_pTitleBar->ConsumeMenuClicked()) {
		if (m_pAppMenu->IsBlocking()) {
			m_pAppMenu->Close();
		} else {
			m_pAppMenu->Open();
		}
	}

	if (m_pTitleBar->ConsumeUpdateClicked()) {
		if (m_pUpdateOverlay->IsBlocking()) {
			m_pUpdateOverlay->Close();
		} else {
			m_pUpdateOverlay->Open();
		}
	}
}

void CApp::HandleAppMenuAction()
{
	switch (m_pAppMenu->ConsumeAction()) {
		case EAppMenuAction::OpenSettings:
			m_pSettingsPanel->Open();
			break;

		case EAppMenuAction::CheckForUpdates: {
			// Reopening the menu must not restart an in-flight download or discard an available
			// update the user has not acted on yet.
			const EUpdateStage stage = m_updater.GetStage();
			const bool idle =
				stage == EUpdateStage::Idle || stage == EUpdateStage::UpToDate || stage == EUpdateStage::CheckFailed;

			if (idle) {
				m_updater.CheckForUpdateAsync(kAppVersion);
			}

			m_pUpdateOverlay->Open();
			break;
		}

		case EAppMenuAction::OpenDataFolder: {
			// "open" rather than "explore" reuses an existing Explorer window. Resolving the
			// directory also creates it, so there is always somewhere to point at.
			char dataDirectory[MAX_PATH];
			if (CStorage::GetDataDirectory(dataDirectory, sizeof(dataDirectory))) {
				ShellExecuteA(nullptr, "open", dataDirectory, nullptr, nullptr, SW_SHOWNORMAL);
			}

			break;
		}

		case EAppMenuAction::None:
			break;
	}

	// Setup rather than unlock: a reset means "create a new password", and this only fires while
	// already unlocked.
	if (m_pSettingsPanel->ConsumeResetPasswordRequested()) {
		m_pUnlockScreen->ActivateForSetup();
		m_bAppLocked = true;
		m_pCarousel->m_bVisible = false;
	}
}

// The modal cannot open a context menu itself, so it latches which row was hit and this serves
// the menu. Copy only - editing and deleting have dedicated buttons on the row.
void CApp::HandleContextMenu(const InputEvent &event)
{
	const PendingHit rightClickedRow = m_pModal->ConsumePendingRightClickRow();

	if (rightClickedRow.Kind == EPendingHitKind::Index) {
		m_nContextMenuAccountIndex = rightClickedRow.Index;

		const ContextMenuItem items[]{
			{"Copy Username", kMenuIdCopyUsername},
			{"Copy Password", kMenuIdCopyPassword},
		};

		m_pContextMenu->Open(event.X, event.Y, items, 2, static_cast<float>(m_window.GetWidth()),
							 static_cast<float>(m_window.GetHeight()));
	}

	const u32 selected = m_pContextMenu->ConsumeSelection();
	if (selected == kContextMenuNoSelection || m_nContextMenuAccountIndex < 0) return;

	// Resolved through the modal rather than indexed into a banner here: a row's position in the
	// visible list is not its position in storage, and that mapping lives in exactly one place.
	const char *pUsername = nullptr;
	const char *pPassword = nullptr;
	if (!m_pModal->GetAccountCopyFields(static_cast<u32>(m_nContextMenuAccountIndex), pUsername, pPassword)) return;

	SetClipboardText(m_window.GetHandle(), selected == kMenuIdCopyUsername ? pUsername : pPassword);
}

bool CApp::HandleWheel(const InputEvent &event)
{
	// Ctrl+scroll cycles the carousel's view mode, but only while nothing above it is blocking.
	const bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
	const bool blocked = m_pModal->IsBlocking() || m_pAppMenu->IsBlocking() || m_pSettingsPanel->IsBlocking() ||
						 m_pContextMenu->IsBlocking() || m_pUnlockScreen->IsBlocking();

	if (!ctrlDown || blocked) return m_stack.DispatchScroll(event.X, event.Y, event.WheelDelta);

	m_pCarousel->AdjustZoomStop(event.WheelDelta);

	// The zoom stop lives in the settings file, so narrowing to that one means a scroll notch -
	// which can fire many times in a row - never re-encrypts the account list.
	SaveSettings();

	return true;
}

bool CApp::HandlePointerUp(const InputEvent &event)
{
	// "An already-centred card opens the modal" needs the selection as it stood before this
	// click, since clicking a different card just re-centres it - dispatch may change it.
	const i32 previouslySelectedBanner = m_pCarousel->GetSelectedIndex();
	const bool consumed = m_stack.DispatchPointerUp(event.X, event.Y);

	const PendingHit clickedBanner = m_pCarousel->ConsumePendingClick();
	const bool openImmediately = m_pCarousel->GetViewMode() != ECarouselViewMode::Carousel;

	if (clickedBanner.Kind == EPendingHitKind::Index &&
		(openImmediately || clickedBanner.Index == previouslySelectedBanner)) {
		m_pModal->Open(clickedBanner.Index);
	}

	return consumed;
}

// Enter on the carousel always opens the focused game, unlike a click, which only opens a card
// that was already centred - the focus ring has already said which one Enter would act on.
void CApp::HandleCarouselActivate()
{
	const PendingHit activated = m_pCarousel->ConsumePendingActivate();

	if (activated.Kind == EPendingHitKind::Index) {
		m_pModal->Open(activated.Index);
	}
}

void CApp::HandleInputEvent(const InputEvent &event)
{
	if (event.Type == EInputEventType::MouseMove) {
		m_flMouseX = event.X;
		m_flMouseY = event.Y;
	}

	m_stack.SetRealMousePosition(m_flMouseX, m_flMouseY);

	bool consumed = false;
	switch (event.Type) {
		case EInputEventType::MouseDown:
			consumed = m_stack.DispatchPointerDown(event.X, event.Y);
			break;

		case EInputEventType::MouseMove:
			consumed = m_stack.DispatchPointerMove(event.X, event.Y);
			break;

		case EInputEventType::MouseUp:
			consumed = HandlePointerUp(event);
			break;

		case EInputEventType::RightMouseUp:
			consumed = m_stack.DispatchRightPointerUp(event.X, event.Y);
			break;

		case EInputEventType::MouseWheel:
			consumed = HandleWheel(event);
			break;

		case EInputEventType::KeyDown:
			consumed = m_stack.DispatchKeyDown(event.KeyCode);
			HandleCarouselActivate();
			break;

		case EInputEventType::CharTyped:
			consumed = m_stack.DispatchChar(event.KeyCode);
			break;
	}

	// Cheap to poll on every event: each returns a "nothing happened" answer almost always.
	HandleTitleBarButtons();
	HandleAppMenuAction();
	HandleContextMenu(event);
	HandlePersistence(event, consumed);
}

// The unlock screen's two success signals need different follow-ups: an unlock only needs a
// reload, since the in-memory passwords are blank from the earlier locked load, while a setup
// needs a save, since every password is re-encrypted under the fresh key.
void CApp::HandlePersistence(const InputEvent &event, bool consumed)
{
	const bool isActionEvent = event.Type == EInputEventType::MouseUp || event.Type == EInputEventType::KeyDown ||
							   event.Type == EInputEventType::RightMouseUp;
	if (!isActionEvent) return;

	const bool unlockSucceeded = m_pUnlockScreen->ConsumeUnlockSucceeded();
	const bool setupSucceeded = m_pUnlockScreen->ConsumeSetupSucceeded();

	if (unlockSucceeded) {
		CStorage::LoadAccounts(&m_pCarousel->GetBanner(0), m_pCarousel->GetBannerCount(),
							   m_settings.m_bMasterPasswordEnabled, m_masterKey);
	} else if (setupSucceeded) {
		SaveAll();
	} else if (consumed) {
		SaveAll();
	} else {
		return;
	}

	if (unlockSucceeded || setupSucceeded) {
		m_bAppLocked = false;
		m_pCarousel->m_bVisible = true;
		m_pUnlockScreen->Deactivate();
	}
}

// The update flow is the one thing that happens without the user asking, so it is the one thing
// that has to announce itself. Everything else in this app is a direct response to a click and
// needs no corner-of-the-screen confirmation.
//
// Edge-triggered off the stage rather than polled on its value, or an available update would
// re-announce itself every frame until acted on.
void CApp::NotifyUpdateStageChanges()
{
	const EUpdateStage stage = m_updater.GetStage();
	if (stage == m_lastNotifiedUpdateStage) return;

	m_lastNotifiedUpdateStage = stage;

	if (stage == EUpdateStage::Available || stage == EUpdateStage::ManualUpgradeRequired) {
		// The one notification here that asks for something rather than reporting it, so it is the
		// one that spins - a turning icon is worth spending on an update nobody has acted on yet.
		char szMessage[96];
		std::snprintf(szMessage, sizeof(szMessage), "Version %s available", m_updater.GetManifest().szVersion);
		m_pToasts->Notify(
			{.Message = szMessage, .Icon = EAsset::IconUpdate, .SpinIcon = true, .Action = EToastAction::OpenUpdates});
	} else if (stage == EUpdateStage::Error) {
		const char *szMessage = "The update could not be installed.";
		m_pToasts->Notify({.Message = szMessage, .Icon = EAsset::IconUpdate, .Action = EToastAction::OpenUpdates});
	}
}

// The host only latches what a click asked for; opening the overlay is this class's job, since it
// is the only thing that owns one.
void CApp::HandleToastAction()
{
	if (m_pToasts->ConsumeAction() == EToastAction::OpenUpdates) m_pUpdateOverlay->Open();
}

// Reported from the frame loop rather than from Init, so the toast animates in over a window that
// is already on screen instead of being half over by the time it is shown.
void CApp::NotifyIfJustUpdated()
{
	if (!m_bJustUpdated) return;

	m_bJustUpdated = false;

	char message[96];
	std::snprintf(message, sizeof(message), "Updated to %s", kAppVersion);
	m_pToasts->Notify({.Message = message, .Icon = EAsset::IconUpdate});
}

// Saving is off for the rest of the session when this fires, which is a state the user has to be
// told about - every change they make from here is silently temporary, and the reason the app is
// refusing is that the files on disk are still worth more than anything it could write over them.
void CApp::NotifyIfStorageSealed()
{
	if (m_bStorageSealNotified) return;
	if (CStorage::IsSettingsWritable() && CStorage::IsAccountsWritable()) return;

	m_bStorageSealNotified = true;
	m_pToasts->Notify({.Message = "Saved data could not be read - changes will not be kept"});
}

void CApp::PumpInput()
{
	PULSAR_PROFILE_SCOPE("Input");

	m_window.PumpMessages();

	m_window.SetCloseToTray(m_settings.m_bCloseToTray && m_tray.IsIconVisible());
	m_tray.SetAccentColor(m_settings.m_clrAccent);
	m_pCarousel->SetAccentColor(m_settings.m_clrAccent);

	HandleTrayEvent();

	for (u32 i = 0; i < m_window.GetInputEventCount(); i += 1) {
		HandleInputEvent(m_window.GetInputEvents()[i]);
	}
}

// The carousel is the one widget whose layout derives from externally-set bounds; every other
// widget queries the window directly. Runs inside AdvanceFrame rather than alongside input, so a
// frame drawn from a live resize drag lays out at the size it is about to be drawn at.
void CApp::Layout()
{
	// Kept live so whatever save happens next persists the current size. Guarded against zero,
	// which a minimized window reports and which would otherwise overwrite the real saved size.
	if (m_window.GetWidth() > 0 && m_window.GetHeight() > 0) {
		m_settings.m_nWindowWidth = m_window.GetWidth();
		m_settings.m_nWindowHeight = m_window.GetHeight();
	}

	const float width = static_cast<float>(m_window.GetWidth());
	const float height = static_cast<float>(m_window.GetHeight());
	m_pCarousel->m_vecBounds = Rect{0.0f, kTitleBarHeight, width, height - kTitleBarHeight - kStatusBarHeight};
}

// One frame: lay out, advance every animation, draw. Everything here is safe to run from inside
// a resize drag's modal loop, which is the whole reason it is separated from the rest of Run's
// body - a frame that only redrew would leave the layout and every animation frozen at whatever
// they held when the drag started.
//
// The re-entrancy guard is not theoretical: presenting can pump messages, and a WM_PAINT
// arriving there would otherwise start a second frame inside this one.
void CApp::AdvanceFrame()
{
	if (m_bInFrame) return;

	m_bInFrame = true;
	PULSAR_PROFILE_FRAME_BEGIN();

	const auto now = std::chrono::steady_clock::now();
	const float deltaSeconds = std::chrono::duration<float>(now - m_previousFrameTime).count();
	m_previousFrameTime = now;

	{
		PULSAR_PROFILE_SCOPE("Layout");
		Layout();
	}

	{
		PULSAR_PROFILE_SCOPE("Widgets.Update");
		m_stack.SetRealMousePosition(m_flMouseX, m_flMouseY);
		m_stack.Update(deltaSeconds);
	}

	// Skipped over the resize border: forcing the app cursor there every frame would fight the
	// OS's resize arrows, which its own hit-test already sets correctly.
	if (!m_window.IsMouseOverResizeBorder()) {
		m_window.SetCursorKind(m_stack.GetDesiredCursor());
	}

	// Drives the banner glow's shimmer, read back when the carousel's geometry is submitted.
	m_renderer.SetEffectTime(std::chrono::duration<float>(now - m_startTime).count());

	if (!m_window.IsMinimized()) {
		RenderFrame();
	}

	PULSAR_PROFILE_FRAME_END();
	m_bInFrame = false;
}

// Keeps usernames, notes and revealed passwords out of screenshots and screen shares while the
// account modal is open. Scoped to that one view, so normal capture returns when it closes.
void CApp::UpdateCaptureExclusion()
{
	const bool shouldExclude = m_settings.m_bExcludeAccountListFromCapture && m_pModal->IsBlocking();
	if (shouldExclude == m_bExcludedFromCapture) return;

	SetWindowDisplayAffinity(m_window.GetHandle(), shouldExclude ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
	m_bExcludedFromCapture = shouldExclude;
}

// A verified update has been swapped into this exe's path. Exiting through the normal close
// path rather than ExitProcess keeps the renderer's GPU release order intact.
bool CApp::ConsumeRelaunchRequest()
{
	if (!m_updater.ConsumeReadyToRelaunch()) return false;

	SaveAll();

	// Released before spawning the replacement, or the new build would see this still-running
	// process's mutex and exit as a duplicate instead of updating.
	m_instanceGuard.Release();

	wchar_t exePath[MAX_PATH];
	if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) > 0) {
		STARTUPINFOW startupInfo{sizeof(startupInfo)};
		PROCESS_INFORMATION processInfo{};

		if (CreateProcessW(exePath, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startupInfo,
						   &processInfo)) {
			CloseHandle(processInfo.hProcess);
			CloseHandle(processInfo.hThread);
		}
	}

	// RequestClose rather than a posted WM_CLOSE, for the same reason the tray's Exit uses it:
	// close-to-tray would otherwise turn this into a hide, leaving two copies running.
	m_window.RequestClose();

	return true;
}

void CApp::DrawStatusBarVersion()
{
	const CFont &secondary = m_fonts.GetSecondary();
	const float statusBarY = static_cast<float>(m_window.GetHeight()) - kStatusBarHeight;

	char versionBuffer[48];
	const int written = std::snprintf(versionBuffer, sizeof(versionBuffer), "%s v%s%s", kAppName, kAppVersion,
									  kIsDebugBuild ? " [dev]" : "");
	const std::string_view versionText{versionBuffer, written > 0 ? static_cast<u64>(written) : 0};

	// The same baseline centring, nudge included, that the carousel's status-bar content uses.
	constexpr float kBaselineVisualNudge = 2.0f;
	const float baselineY = statusBarY + kStatusBarHeight * 0.5f +
							(secondary.GetAscent() + secondary.GetDescent()) * 0.5f - kBaselineVisualNudge;

	// Centred on the bar rather than on the text's baseline: the mark is a square, and lining its
	// centre up with the text's would sit it low by half a descender.
	const Rect mark{kStatusBarVersionPadding, statusBarY + (kStatusBarHeight - kStatusBarMarkSize) * 0.5f,
					kStatusBarMarkSize, kStatusBarMarkSize};
	Controls::DrawIcon(m_drawList, mark, m_assets.Get(EAsset::IconApp), kColorStatusBarText);

	DrawText(m_drawList, secondary, mark.X + mark.W + kStatusBarMarkGap, baselineY, versionText, kColorStatusBarText);
}

// One renderer call per command, in the order the draw list closed them.
void CApp::SubmitDrawList()
{
	for (u32 i = 0; i < m_drawList.GetCommandCount(); i += 1) {
		const DrawCommand &command = m_drawList.GetCommands()[i];
		const u32 *pIndices = m_drawList.GetIndices() + command.IndexOffset;
		const Vertex2D *pVertices = m_drawList.GetVertices();
		const u32 vertexCount = m_drawList.GetVertexCount();

		m_renderer.SetClipRect(command.HasClip ? ClipRect{true, command.ClipRect} : ClipRect{false, Rect{}});

		switch (command.Kind) {
			case EDrawCommandKind::Solid:
				m_renderer.Draw2D(pVertices, vertexCount, pIndices, command.IndexCount);
				break;

			case EDrawCommandKind::Textured:
				m_renderer.Draw2DTextured(command.pTexture != nullptr ? command.pTexture->GetHandle() : nullptr,
										  pVertices, vertexCount, pIndices, command.IndexCount);
				break;

			case EDrawCommandKind::BannerGlow:
				m_renderer.Draw2DBannerGlow(pVertices, vertexCount, pIndices, command.IndexCount,
											command.Glow.QuadWidth, command.Glow.QuadHeight, command.Glow.CornerRadius,
											command.Glow.RingWidth);
				break;

			case EDrawCommandKind::ColorPickerSv:
				m_renderer.Draw2DColorPickerSv(pVertices, vertexCount, pIndices, command.IndexCount);
				break;

			case EDrawCommandKind::CircularProgress:
				m_renderer.Draw2DCircularProgress(
					pVertices, vertexCount, pIndices, command.IndexCount, command.Progress.QuadWidth,
					command.Progress.QuadHeight, command.Progress.OuterRadius, command.Progress.InnerRadius,
					command.Progress.StartAngle, command.Progress.SweepAngle, command.Progress.GlowStrength);
				break;
		}
	}
}

void CApp::RenderFrame()
{
	PULSAR_PROFILE_SCOPE("Render");

	const float width = static_cast<float>(m_window.GetWidth());
	const float height = static_cast<float>(m_window.GetHeight());

	m_drawList.Clear();

	// The chrome stays visible even on the master-password screen: only the carousel's
	// content waits on the unlock state.
	m_drawList.AddRectFilled(0.0f, kTitleBarHeight, width, 1.0f, kColorChromeSeam);
	m_drawList.AddRectFilled(0.0f, height - kStatusBarHeight - 1.0f, width, 1.0f, kColorChromeSeam);
	m_drawList.AddRectFilled(0.0f, height - kStatusBarHeight, width, kStatusBarHeight, kTitleBarColor);

	DrawStatusBarVersion();

	if (m_pCarousel->m_bVisible) {
		m_pCarousel->DrawStatusBarContent(m_drawList);
	}

	{
		PULSAR_PROFILE_SCOPE("Render.BuildGeometry");
		m_stack.Draw(m_drawList);
		m_drawList.Finish();
	}

	m_renderer.BeginFrame();
	m_renderer.Clear(kColorBackground);

	{
		PULSAR_PROFILE_SCOPE("Render.Submit");
		SubmitDrawList();
	}

	{
		// Blocks on vsync, so this is nearly always the largest number in the report and nearly
		// always means nothing. Read the others against the frame total instead.
		PULSAR_PROFILE_SCOPE("Render.Present");
		m_renderer.EndFrame();
	}
}

void CApp::Run()
{
	while (!m_window.ShouldClose()) {
		PumpInput();
		NotifyIfJustUpdated();
		NotifyIfStorageSealed();
		AdvanceFrame();

		// Joins a finished worker once it is done; safe to call every frame regardless.
		m_updater.Update();
		NotifyUpdateStageChanges();
		HandleToastAction();
		ConsumeRelaunchRequest();
		UpdateCaptureExclusion();

		// Hidden or minimized: nothing was presented, so nothing paced this iteration.
		// Everything above still runs, since a tray quick-login has to work while hidden.
		if (m_window.IsHidden() || m_window.IsMinimized()) {
			Sleep(16);
		}

		// Last in the frame, so it only ticks once one has been presented. The watchdog reads this
		// to tell a frozen app apart from a stuck login worker.
		DebugLog::MarkUiThreadAlive();
	}

	SaveAll();
}
