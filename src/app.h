#pragma once

#include <chrono>

#include "core/master_key.h"
#include "core/memory_arena.h"
#include "core/settings.h"
#include "core/storage.h"
#include "core/updater.h"
#include "gfx/asset_manager.h"
#include "gfx/d3d11/renderer_d3d11.h"
#include "gfx/font_manager.h"
#include "platform/overlay_guard.h"
#include "platform/single_instance.h"
#include "platform/tray.h"
#include "platform/window.h"
#include "ui/account_modal.h"
#include "ui/app_menu.h"
#include "ui/carousel.h"
#include "ui/context_menu.h"
#include "ui/draw_list.h"
#include "ui/profiler_overlay.h"
#include "ui/settings_panel.h"
#include "ui/title_bar.h"
#include "ui/toast.h"
#include "ui/unlock_screen.h"
#include "ui/update_overlay.h"
#include "ui/widget_stack.h"

/// Owns every subsystem and runs the frame loop. main() does only the process-level bootstrap
/// that has to happen before any of this exists - crash handling, the diagnostic log, and the
/// updater's startup recovery.
class CApp {
  public:
	enum class EStartResult : u8 {
		Ok,
		AlreadyRunning, // another instance was handed the foreground; this one should just exit
		Failed,
	};

	CApp() = default;

	CApp(const CApp &) = delete;
	CApp &operator=(const CApp &) = delete;

	/// Brings up every subsystem, loads persisted state, and draws one frame before the window is
	/// ever shown - so the first thing on screen is real UI rather than whatever the swapchain
	/// started with.
	EStartResult Init();

	void Run();

  private:
	/// Win32 delivers both of these synchronously, blocking this thread, so neither can be polled
	/// once per frame the way everything else is. A live border drag runs inside DefWindowProc's
	/// own modal loop, and the DPI change fires mid-move.
	static void OnWindowRedraw(void *pUserData);
	static void OnWindowDpiChanged(void *pUserData);

	/// Called synchronously from inside the tray's menu handler, since that call blocks this
	/// thread for as long as the menu is open and nothing polled per frame could answer in time.
	static void BuildTrayMenu(void *pUserData, TrayMenuModel &outModel);

	void ApplyOverlayGuard(bool blockInjection);
	bool InitGraphics(const Settings &bootSettings);
	void CreateGames();
	void CreateWidgets();
	void ApplyLoadedSettings(EStorageLoadResult loadResult);

	/// The two files are independent, so a caller can touch only the one that actually changed -
	/// a zoom notch never has to re-encrypt the account list. Loading order is not optional:
	/// accounts cannot be decrypted without the master-password parameters settings.json holds.
	void SaveAccounts();
	void SaveSettings();
	void SaveAll();
	EStorageLoadResult LoadAll();

	void PumpInput();
	void Layout();
	void AdvanceFrame();
	void HandleInputEvent(const InputEvent &event);
	bool HandlePointerUp(const InputEvent &event);
	void HandleCarouselActivate();
	bool HandleWheel(const InputEvent &event);
	void HandleTrayEvent();
	void HandleTitleBarButtons();
	void HandleAppMenuAction();
	void HandleContextMenu(const InputEvent &event);

	/// Persisting on any consumed input is a little more liberal than strictly necessary, but a
	/// save is cheap and idempotent, and precisely tracking which sub-interaction touched
	/// persisted state is not worth the complexity.
	void HandlePersistence(const InputEvent &event, bool consumed);

	void NotifyUpdateStageChanges();
	void HandleToastAction();
	void NotifyIfJustUpdated();
	void NotifyIfStorageSealed();
	void UpdateCaptureExclusion();
	bool ConsumeRelaunchRequest();

	void DrawStatusBarVersion();
	void RenderFrame();
	void SubmitDrawList();

	/// Declaration order is destruction order reversed, and that matters here: every texture is
	/// owned by the assets, the fonts or a widget, and each one calls back into the renderer to
	/// release its GPU resource. The renderer is declared before all of them, so it outlives
	/// them; the arena is declared first, so it outlives the draw list allocated from it.
	CSingleInstanceGuard m_instanceGuard;
	CMemoryArena m_arena;
	CWindow m_window;
	CRendererD3D11 m_renderer;
	CAssetManager m_assets;
	CFontManager m_fonts;
	CTray m_tray;
	CDrawList m_drawList;
	CWidgetStack m_stack;

	Settings m_settings;
	CMasterKey m_masterKey;
	CUpdater m_updater;

	/// Non-owning; the stack owns every widget. Moving a unique_ptr into its vector does not
	/// relocate the object, so these stay valid for the stack's whole lifetime.
	CCarousel *m_pCarousel = nullptr;
	CAccountModal *m_pModal = nullptr;
	CAppMenu *m_pAppMenu = nullptr;
	CSettingsPanel *m_pSettingsPanel = nullptr;
	CContextMenu *m_pContextMenu = nullptr;
	CUnlockScreen *m_pUnlockScreen = nullptr;
	CUpdateOverlay *m_pUpdateOverlay = nullptr;
	CTitleBar *m_pTitleBar = nullptr;
	CToastHost *m_pToasts = nullptr;

	/// A live reference into this is handed to the app menu, so the Settings row can grey itself
	/// out while the vault is locked.
	bool m_bAppLocked = true;

	float m_flMouseX = -1.0f;
	float m_flMouseY = -1.0f;

	/// Which account row the open context menu belongs to; -1 when none is in flight.
	i32 m_nContextMenuAccountIndex = -1;

	/// Only applied on a real transition: the OS keeps enforcing whatever affinity was last set.
	bool m_bExcludedFromCapture = false;

	/// What the swapchain was last sized to, so an ordinary repaint does not rebuild it.
	u32 m_nSwapchainWidth = 0;
	u32 m_nSwapchainHeight = 0;

	/// Guards against a frame started from inside another frame - see AdvanceFrame.
	bool m_bInFrame = false;

	/// Edge detection for the one notification the user did not ask for.
	EUpdateStage m_lastNotifiedUpdateStage = EUpdateStage::Idle;
	bool m_bJustUpdated = false;
	bool m_bStorageSealNotified = false;

	std::chrono::steady_clock::time_point m_startTime;
	std::chrono::steady_clock::time_point m_previousFrameTime;
};
