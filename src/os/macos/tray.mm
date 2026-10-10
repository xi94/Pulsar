#include "os/tray.h"

#include <utility>

#include "core/debug_log.h"
#include "os/macos/macos.h"

namespace {
constexpr const char* K_LOG_CATEGORY      = "tray";
constexpr const char* K_STATUS_ICON_NAME  = "MenuBarIcon";
constexpr CGFloat     K_STATUS_ICON_SIZE  = 18.0;
constexpr CGFloat     K_MENU_ICON_SIZE    = 16.0;
constexpr NSInteger   K_SHOW_TAG          = 1;
constexpr NSInteger   K_EXIT_TAG          = 2;
constexpr NSInteger   K_FIRST_ACCOUNT_TAG = 1000;

// The menu bar keeps only an icon's shape, so it gets the bare star. The app icon has a tile behind it, and the shape of a tile is a square.
[[nodiscard]] auto status_icon() -> NSImage*
{
	NSImage* icon = [[NSImage imageNamed:@(K_STATUS_ICON_NAME)] copy];
	if (icon == nil) {
		icon = [[NSApplication sharedApplication].applicationIconImage copy];
	}

	icon.size = NSMakeSize(K_STATUS_ICON_SIZE, K_STATUS_ICON_SIZE);
	[icon setTemplate:YES];

	return icon;
}
}

@interface PulsarMenuTarget : NSObject <NSMenuDelegate>
- (instancetype)initWithOpen:(void (^)())t_open choose:(void (^)(NSInteger))t_choose;
- (void)choose:(NSMenuItem*)t_item;
@end

@implementation PulsarMenuTarget {
	void (^m_open)();
	void (^m_choose)(NSInteger);
}

- (instancetype)initWithOpen:(void (^)())t_open choose:(void (^)(NSInteger))t_choose
{
	self = [super init];
	if (self != nil) {
		m_open   = t_open;
		m_choose = t_choose;
	}

	return self;
}

- (void)menuNeedsUpdate:(NSMenu*)t_menu
{
	m_open();
}

- (void)choose:(NSMenuItem*)t_item
{
	m_choose(t_item.tag);
}

@end

namespace os {

struct Tray::Native {
	NSStatusItem*     item   = nil;
	NSMenu*           menu   = nil;
	PulsarMenuTarget* target = nil;
	std::string       tooltip;
	bool              locked = false;
	TrayColors        colors{};

	std::span<const u8> game_icon_sources[K_TRAY_MAX_GAMES]{};
	NSImage*            game_icons[K_TRAY_MAX_GAMES]{};

	std::function<void(TrayMenu*)> fill_menu;
	TrayMenu                       contents{};
	TrayEvent                      pending_event{};

	auto rebuild_menu() -> void;
	auto choose(NSInteger t_tag) -> void;
	auto update_button() const -> void;
	[[nodiscard]] auto game_icon(i32 t_game) -> NSImage*;
	auto add_item(NSMenu* t_menu, NSString* t_title, NSInteger t_tag) const -> NSMenuItem*;
};

Tray::Tray()
	: m_native(std::make_unique<Native>())
{
}

Tray::~Tray()
{
	if (m_native->item != nil) {
		[NSStatusBar.systemStatusBar removeStatusItem:m_native->item];
	}
}

auto Tray::create(std::string_view t_tooltip) -> bool
{
	macos::prepare_application();

	Native* native = m_native.get();
	native->item   = [NSStatusBar.systemStatusBar statusItemWithLength:NSSquareStatusItemLength];
	if (native->item == nil) {
		debug_log::write(K_LOG_CATEGORY, "the menu bar refused a status item");
		return false;
	}

	const auto open = ^{
		native->rebuild_menu();
	};

	const auto choose = ^(NSInteger t_tag) {
		native->choose(t_tag);
	};

	native->tooltip               = std::string{t_tooltip};
	native->target                = [[PulsarMenuTarget alloc] initWithOpen:open choose:choose];
	native->menu                  = [[NSMenu alloc] init];
	native->menu.autoenablesItems = NO;
	native->menu.delegate         = native->target;
	native->item.menu             = native->menu;
	native->item.button.image     = status_icon();
	native->update_button();

	return true;
}

auto Tray::on_menu_open(std::function<void(TrayMenu*)> t_fill_menu) -> void
{
	m_native->fill_menu = std::move(t_fill_menu);
}

auto Tray::set_game_icon(u32 t_game, std::span<const u8> t_png) -> void
{
	if (t_game >= K_TRAY_MAX_GAMES) return;

	m_native->game_icon_sources[t_game] = t_png;
	m_native->game_icons[t_game]        = nil;
}

// The menu bar's menu is the system's own and shows the app icon, not the logo.
auto Tray::set_logo(std::span<const u8>) -> void {}

// The small login window by the tray is Windows only for now. On a Mac the app's own login progress shows instead.
auto Tray::show_login(const TrayLogin&) -> void {}

auto Tray::hide_login() -> void {}

auto Tray::set_colors(const TrayColors& t_colors) -> void
{
	m_native->colors = t_colors;
}

auto Tray::set_locked(bool t_locked) -> void
{
	if (t_locked == m_native->locked) return;

	m_native->locked = t_locked;
	m_native->update_button();
}

auto Tray::is_icon_visible() const -> bool
{
	return m_native->item != nil && m_native->item.isVisible;
}

auto Tray::take_event() -> TrayEvent
{
	return std::exchange(m_native->pending_event, TrayEvent{});
}

auto Tray::Native::rebuild_menu() -> void
{
	[menu removeAllItems];

	contents        = TrayMenu{};
	contents.locked = locked;
	if (fill_menu && !locked) {
		fill_menu(&contents);
	}

	for (const TrayGame& game : std::span{contents.games, contents.game_count}) {
		NSMenu* accounts          = [[NSMenu alloc] init];
		accounts.autoenablesItems = NO;
		NSMenuItem* game_item     = add_item(menu, macos::to_ns_string(game.title), 0);
		game_item.image           = game_icon(game.game);
		game_item.submenu         = accounts;

		for (u32 i = 0; i < game.account_count; i += 1) {
			const u32 account = game.first_account + i;
			if (account >= contents.account_count) break;

			add_item(accounts, macos::to_ns_string(contents.accounts[account].label), K_FIRST_ACCOUNT_TAG + account);
		}

		if (game.account_count == 0) {
			add_item(accounts, @"No accounts", 0).enabled = NO;
		}
	}

	if (contents.locked) {
		add_item(menu, @"Vault locked", 0).enabled = NO;
	} else if (contents.game_count == 0) {
		add_item(menu, @"No games", 0).enabled = NO;
	}

	[menu addItem:NSMenuItem.separatorItem];
	add_item(menu, @"Show Pulsar", K_SHOW_TAG);
	add_item(menu, @"Quit Pulsar", K_EXIT_TAG);

	const bool dark = luminance(colors.background) < luminance(colors.text);
	menu.appearance = [NSAppearance appearanceNamed:dark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
}

auto Tray::Native::choose(NSInteger t_tag) -> void
{
	if (t_tag == K_SHOW_TAG) {
		pending_event = TrayEvent{.type = TrayEventType::SHOW_WINDOW};
	} else if (t_tag == K_EXIT_TAG) {
		pending_event = TrayEvent{.type = TrayEventType::EXIT};
	} else if (t_tag >= K_FIRST_ACCOUNT_TAG && static_cast<u32>(t_tag - K_FIRST_ACCOUNT_TAG) < contents.account_count && !locked) {
		const TrayAccount& account = contents.accounts[t_tag - K_FIRST_ACCOUNT_TAG];
		pending_event              = TrayEvent{.type = TrayEventType::QUICK_LOGIN, .game = account.game, .row = account.row};
	}

	macos::wake_event_loop();
}

auto Tray::Native::update_button() const -> void
{
	item.button.appearsDisabled = locked;
	item.button.toolTip         = macos::to_ns_string(locked ? tooltip + " (locked)" : tooltip);
}

auto Tray::Native::game_icon(i32 t_game) -> NSImage*
{
	if (t_game < 0 || static_cast<u32>(t_game) >= K_TRAY_MAX_GAMES) return nil;

	const std::span<const u8> source = game_icon_sources[t_game];
	if (game_icons[t_game] == nil && !source.empty()) {
		NSData* png             = [NSData dataWithBytes:source.data() length:source.size()];
		game_icons[t_game]      = [[NSImage alloc] initWithData:png];
		game_icons[t_game].size = NSMakeSize(K_MENU_ICON_SIZE, K_MENU_ICON_SIZE);
	}

	return game_icons[t_game];
}

auto Tray::Native::add_item(NSMenu* t_menu, NSString* t_title, NSInteger t_tag) const -> NSMenuItem*
{
	NSMenuItem* entry = [t_menu addItemWithTitle:t_title action:t_tag != 0 ? @selector(choose:) : nil keyEquivalent:@""];
	entry.target      = target;
	entry.tag         = t_tag;

	return entry;
}

}
