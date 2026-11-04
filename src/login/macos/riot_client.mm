#include "login/riot_client.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <concepts>
#include <optional>
#include <ranges>
#include <thread>
#include <utility>
#include <vector>

#include <libproc.h>
#include <signal.h>

#include "core/debug_log.h"
#include "core/file.h"
#include "os/macos/macos.h"

namespace {
constexpr const char* K_LOG_CATEGORY = "riot";

constexpr const char*      K_INSTALLS_JSON_PATH        = "/Users/Shared/Riot Games/RiotClientInstalls.json";
constexpr const char*      K_DEFAULT_INSTALL_FOLDER    = "/Users/Shared/Riot Games";
constexpr const char*      K_CLIENT_BUNDLE_NAME        = "Riot Client.app";
constexpr const char*      K_CLIENT_BUNDLE_IDENTIFIER  = "com.riotgames.RiotGames.RiotClient";
constexpr std::string_view K_SERVICES_IN_BUNDLE        = "/Contents/MacOS/RiotClientServices";
constexpr std::string_view K_INTERFACE_PATH_SUFFIX     = "/Riot Client.app/Contents/MacOS/Riot Client";
constexpr std::string_view K_CLIENT_BUNDLE_MARKER      = "/Riot Client.app/";
constexpr std::string_view K_LEAGUE_CLIENT_PREFIX      = "LeagueClient";
constexpr std::string_view K_GAME_PROCESS_NAME         = "LeagueofLegends";
constexpr std::string_view K_GAME_LAUNCH_PRODUCT       = "league_of_legends";
constexpr u32              K_CHOSEN_PATH_SEARCH_LEVELS = 3;

constexpr const char* K_AUTOMATION_FAILED_MESSAGE  = "Couldn't connect to the Riot Client - try again.";
constexpr const char* K_PERMISSION_MISSING_MESSAGE = "Pulsar needs the Accessibility permission to sign in. If it's already on in System Settings, remove Pulsar from the list and add it again.";
constexpr const char* K_ACCESSIBILITY_SETTINGS_URL = "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility";
constexpr const char* K_USERNAME_FIELD_NAME        = "USERNAME";
constexpr const char* K_PASSWORD_FIELD_NAME        = "PASSWORD";
constexpr const char* K_SIGN_IN_BUTTON_NAME        = "Sign in";
constexpr const char* K_PLAY_BUTTON_NAME           = "Play";
constexpr const char* K_LOGIN_ERROR_NAME           = "Login error";
constexpr const char* K_INVALID_CREDENTIALS_REASON = "Your login credentials don't match an account in our system.";
constexpr const char* K_TROUBLE_SIGNING_IN_REASON  = "Sorry, we're having trouble signing you in right now. Please try again later.";

constexpr auto  K_WINDOW_ELEMENT_LIFETIME = std::chrono::milliseconds(2000);
constexpr auto  K_POLL_INTERVAL           = std::chrono::milliseconds(100);
constexpr auto  K_LAUNCH_TIMEOUT          = std::chrono::seconds(15);
constexpr float K_MESSAGING_TIMEOUT       = 0.75f;
constexpr u32   K_FOCUS_SETTLE_MS         = 500;
constexpr usize K_MAX_SEARCH_NODES        = 6000;

struct AxNode {
	NSString* role;
	NSString* subrole;
	NSString* title;
	NSString* description;
	NSString* value;
	NSString* placeholder;
	NSArray*  children;
};

[[nodiscard]] auto is_cancelled(const std::atomic<bool>* t_cancel) -> bool
{
	return t_cancel->load(std::memory_order_relaxed);
}

[[nodiscard]] auto deadline_after(u32 t_timeout_ms) -> std::chrono::steady_clock::time_point
{
	return std::chrono::steady_clock::now() + std::chrono::milliseconds(t_timeout_ms);
}

[[nodiscard]] auto is_past(std::chrono::steady_clock::time_point t_deadline) -> bool
{
	return std::chrono::steady_clock::now() >= t_deadline;
}

[[nodiscard]] auto is_file(const std::string& t_path) -> bool
{
	BOOL is_folder = NO;

	return !t_path.empty() && [NSFileManager.defaultManager fileExistsAtPath:os::macos::to_ns_string(t_path) isDirectory:&is_folder] && !is_folder;
}

[[nodiscard]] auto file_name_of(std::string_view t_path) -> std::string_view
{
	const usize slash = t_path.rfind('/');

	return slash == std::string_view::npos ? t_path : t_path.substr(slash + 1);
}

auto for_each_process(std::invocable<pid_t, std::string_view> auto t_visitor) -> void
{
	std::vector<pid_t> pids(static_cast<usize>(std::max(proc_listallpids(nullptr, 0), 0)) + 64);
	const int          count = std::max(proc_listallpids(pids.data(), static_cast<int>(pids.size() * sizeof(pid_t))), 0);

	char path[PROC_PIDPATHINFO_MAXSIZE];
	for (const int i : std::views::iota(0, count)) {
		if (proc_pidpath(pids[static_cast<usize>(i)], path, sizeof(path)) > 0) {
			t_visitor(pids[static_cast<usize>(i)], std::string_view{path});
		}
	}
}

[[nodiscard]] auto is_client_process(std::string_view t_path) -> bool
{
	return t_path.find(K_CLIENT_BUNDLE_MARKER) != std::string_view::npos || file_name_of(t_path).starts_with(K_LEAGUE_CLIENT_PREFIX);
}

[[nodiscard]] auto interface_process() -> pid_t
{
	pid_t found = 0;
	for_each_process([&found](pid_t t_pid, std::string_view t_path) {
		if (t_path.ends_with(K_INTERFACE_PATH_SUFFIX)) {
			found = t_pid;
		}
	});

	return found;
}

[[nodiscard]] auto has_visible_window(pid_t t_pid) -> bool
{
	NSArray<NSDictionary*>* windows = CFBridgingRelease(CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID));

	for (NSDictionary* window in windows) {
		const bool owned   = [window[(__bridge NSString*)kCGWindowOwnerPID] intValue] == t_pid;
		const bool regular = [window[(__bridge NSString*)kCGWindowLayer] intValue] == 0;
		if (owned && regular) return true;
	}

	return false;
}

auto installs_json_paths(std::vector<std::string>* t_out) -> void
{
	std::vector<u8> bytes;
	if (!read_whole_file(K_INSTALLS_JSON_PATH, &bytes)) return;

	const std::string_view json{reinterpret_cast<const char*>(bytes.data()), bytes.size()};

	for (std::string& path : RiotClient::paths_in_installs_file(json)) {
		t_out->push_back(std::move(path));
	}
}

[[nodiscard]] auto client_path_candidates() -> std::vector<std::string>
{
	std::vector<std::string> candidates;
	installs_json_paths(&candidates);

	if (NSURL* bundle = [NSWorkspace.sharedWorkspace URLForApplicationWithBundleIdentifier:@(K_CLIENT_BUNDLE_IDENTIFIER)]; bundle != nil) {
		candidates.push_back(os::macos::to_utf8(bundle.path) + std::string{K_SERVICES_IN_BUNDLE});
	}

	for_each_process([&candidates](pid_t, std::string_view t_path) {
		if (t_path.ends_with(K_SERVICES_IN_BUNDLE)) {
			candidates.emplace_back(t_path);
		}
	});

	candidates.push_back(std::string{K_DEFAULT_INSTALL_FOLDER} + "/" + K_CLIENT_BUNDLE_NAME + std::string{K_SERVICES_IN_BUNDLE});

	return candidates;
}

[[nodiscard]] auto bundle_of(const std::string& t_executable) -> NSURL*
{
	if (!t_executable.ends_with(K_SERVICES_IN_BUNDLE)) return nil;

	return [NSURL fileURLWithPath:os::macos::to_ns_string(std::string_view{t_executable}.substr(0, t_executable.size() - K_SERVICES_IN_BUNDLE.size()))];
}

[[nodiscard]] auto string_or_nil(id t_value) -> NSString*
{
	return [t_value isKindOfClass:NSString.class] ? t_value : nil;
}

[[nodiscard]] auto read_node(id t_element) -> AxNode
{
	static NSArray* const ATTRIBUTES = @[
		(__bridge NSString*)kAXRoleAttribute,
		(__bridge NSString*)kAXSubroleAttribute,
		(__bridge NSString*)kAXTitleAttribute,
		(__bridge NSString*)kAXDescriptionAttribute,
		(__bridge NSString*)kAXValueAttribute,
		(__bridge NSString*)kAXPlaceholderValueAttribute,
		(__bridge NSString*)kAXChildrenAttribute,
	];

	CFArrayRef values = nullptr;
	if (AXUIElementCopyMultipleAttributeValues((__bridge AXUIElementRef)t_element, (__bridge CFArrayRef)ATTRIBUTES, 0, &values) != kAXErrorSuccess) return {};

	NSArray* read = CFBridgingRelease(values);
	if (read.count != ATTRIBUTES.count) return {};

	return AxNode{
		.role        = string_or_nil(read[0]),
		.subrole     = string_or_nil(read[1]),
		.title       = string_or_nil(read[2]),
		.description = string_or_nil(read[3]),
		.value       = string_or_nil(read[4]),
		.placeholder = string_or_nil(read[5]),
		.children    = [read[6] isKindOfClass:NSArray.class] ? read[6] : nil,
	};
}

[[nodiscard]] auto find_element(id t_root, const std::atomic<bool>* t_cancel, std::predicate<const AxNode&> auto t_matches) -> id
{
	if (t_root == nil) return nil;

	NSMutableArray* pending = [NSMutableArray arrayWithObject:t_root];

	for (usize visited = 0; pending.count > 0 && visited < K_MAX_SEARCH_NODES; visited += 1) {
		if (is_cancelled(t_cancel)) return nil;

		id element = pending.lastObject;
		[pending removeLastObject];

		const AxNode node = read_node(element);
		if (t_matches(node)) return element;

		for (id child in node.children.reverseObjectEnumerator) {
			[pending addObject:child];
		}
	}

	return nil;
}

[[nodiscard]] auto is_named(const AxNode& t_node, const char* t_name) -> bool
{
	NSString* name = @(t_name);

	for (NSString* label : {t_node.title, t_node.description, t_node.placeholder}) {
		if (label != nil && [label caseInsensitiveCompare:name] == NSOrderedSame) return true;
	}

	return false;
}

[[nodiscard]] auto shows_text(const AxNode& t_node, const char* t_text) -> bool
{
	NSString* text = @(t_text);

	return [t_node.value isEqualToString:text] || [t_node.title isEqualToString:text] || [t_node.description isEqualToString:text];
}

[[nodiscard]] auto is_text_field(const AxNode& t_node, bool t_secure) -> bool
{
	const bool secure = [t_node.subrole isEqualToString:(__bridge NSString*)kAXSecureTextFieldSubrole];

	return [t_node.role isEqualToString:(__bridge NSString*)kAXTextFieldRole] && secure == t_secure;
}

[[nodiscard]] auto is_button_named(const AxNode& t_node, const char* t_name) -> bool
{
	return [t_node.role isEqualToString:(__bridge NSString*)kAXButtonRole] && is_named(t_node, t_name);
}

[[nodiscard]] auto has_keyboard_focus(id t_element) -> bool
{
	CFTypeRef focused = nullptr;
	if (AXUIElementCopyAttributeValue((__bridge AXUIElementRef)t_element, kAXFocusedAttribute, &focused) != kAXErrorSuccess) return false;

	return [CFBridgingRelease(focused) boolValue];
}

[[nodiscard]] auto is_enabled(id t_element) -> bool
{
	CFTypeRef enabled = nullptr;
	if (AXUIElementCopyAttributeValue((__bridge AXUIElementRef)t_element, kAXEnabledAttribute, &enabled) != kAXErrorSuccess) return true;

	return [CFBridgingRelease(enabled) boolValue];
}

auto focus(id t_element) -> void
{
	AXUIElementSetAttributeValue((__bridge AXUIElementRef)t_element, kAXFocusedAttribute, kCFBooleanTrue);
}

auto wait_for_keyboard_focus(id t_element) -> void
{
	const auto deadline = deadline_after(K_FOCUS_SETTLE_MS);

	while (!has_keyboard_focus(t_element) && !is_past(deadline)) {
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
	}
}

auto post_key(pid_t t_pid, CGKeyCode t_key, const UniChar* t_text, UniCharCount t_length) -> void
{
	for (const bool down : {true, false}) {
		const CGEventRef event = CGEventCreateKeyboardEvent(nullptr, t_key, down);
		if (t_length > 0) {
			CGEventKeyboardSetUnicodeString(event, t_length, t_text);
		}

		CGEventPostToPid(t_pid, event);
		CFRelease(event);
	}
}

auto type_text(pid_t t_pid, NSString* t_text) -> void
{
	for (NSUInteger i = 0; i < t_text.length;) {
		UniChar            units[2]{[t_text characterAtIndex:i], 0};
		const UniCharCount length = CFStringIsSurrogateHighCharacter(units[0]) && i + 1 < t_text.length ? 2 : 1;
		if (length == 2) {
			units[1] = [t_text characterAtIndex:i + 1];
		}

		post_key(t_pid, 0, units, length);
		i += length;
	}
}

auto fill_field(pid_t t_pid, id t_field, NSString* t_value) -> void
{
	if (AXUIElementSetAttributeValue((__bridge AXUIElementRef)t_field, kAXValueAttribute, (__bridge CFStringRef)t_value) == kAXErrorSuccess) return;

	focus(t_field);
	wait_for_keyboard_focus(t_field);
	type_text(t_pid, t_value);
}

[[nodiscard]] auto is_tooltip(const AxNode& t_node) -> bool
{
	return [t_node.role isEqualToString:(__bridge NSString*)kAXHelpTagRole];
}

[[nodiscard]] auto login_error_text(id t_window, const std::atomic<bool>* t_cancel) -> std::string
{
	for (const char* reason : {K_INVALID_CREDENTIALS_REASON, K_TROUBLE_SIGNING_IN_REASON}) {
		if (find_element(t_window, t_cancel, [reason](const AxNode& t_node) { return shows_text(t_node, reason); }) != nil) return reason;
	}

	const id tooltip = find_element(t_window, t_cancel, is_tooltip);
	if (tooltip == nil) return {};

	const AxNode node = read_node(tooltip);

	for (NSString* text : {node.description, node.title, node.value}) {
		if (text.length > 0 && ![text isEqualToString:@(K_LOGIN_ERROR_NAME)]) return os::macos::to_utf8(text);
	}

	return {};
}

[[nodiscard]] auto find_login_fields(id t_window, const std::atomic<bool>* t_cancel, id* t_username, id* t_password, bool* t_by_name) -> bool
{
	*t_username = find_element(t_window, t_cancel, [](const AxNode& t_node) { return is_text_field(t_node, false) && is_named(t_node, K_USERNAME_FIELD_NAME); });
	*t_password = find_element(t_window, t_cancel, [](const AxNode& t_node) { return is_text_field(t_node, true) && is_named(t_node, K_PASSWORD_FIELD_NAME); });
	*t_by_name  = true;
	if (*t_username != nil && *t_password != nil) return true;

	// A client in another language names the fields differently, so fall back to the password box and the plain box beside it.
	*t_password = find_element(t_window, t_cancel, [](const AxNode& t_node) { return is_text_field(t_node, true); });
	*t_username = *t_password != nil ? find_element(t_window, t_cancel, [](const AxNode& t_node) { return is_text_field(t_node, false); }) : nil;
	*t_by_name  = false;

	return *t_username != nil && *t_password != nil;
}

[[nodiscard]] auto shown_login_error(id t_window, const std::atomic<bool>* t_cancel, bool t_fields_found_by_name) -> std::optional<std::string>
{
	if (find_element(t_window, t_cancel, [](const AxNode& t_node) { return shows_text(t_node, K_LOGIN_ERROR_NAME); }) != nil) {
		return login_error_text(t_window, t_cancel);
	}

	if (t_fields_found_by_name || find_element(t_window, t_cancel, is_tooltip) == nil) return std::nullopt;

	return login_error_text(t_window, t_cancel);
}

[[nodiscard]] auto find_play_button(id t_window, const std::atomic<bool>* t_cancel) -> id
{
	return find_element(t_window, t_cancel, [](const AxNode& t_node) { return is_button_named(t_node, K_PLAY_BUTTON_NAME); });
}
}

struct RiotClient::Native {
	pid_t                                 interface_pid = 0;
	id                                    application   = nil;
	id                                    cached_window = nil;
	std::chrono::steady_clock::time_point cached_window_expiry;

	[[nodiscard]] auto current_window() -> id;
	auto wait_for_interface_window(const std::atomic<bool>* t_cancel) -> pid_t;
};

auto RiotClient::Native::current_window() -> id
{
	const auto now = std::chrono::steady_clock::now();
	if (cached_window != nil && now < cached_window_expiry) return cached_window;

	CFTypeRef window = nullptr;
	if (AXUIElementCopyAttributeValue((__bridge AXUIElementRef)application, kAXFocusedWindowAttribute, &window) != kAXErrorSuccess || window == nullptr) {
		CFArrayRef windows = nullptr;
		if (AXUIElementCopyAttributeValues((__bridge AXUIElementRef)application, kAXWindowsAttribute, 0, 1, &windows) == kAXErrorSuccess) {
			NSArray* list = CFBridgingRelease(windows);
			window        = list.count > 0 ? CFBridgingRetain(list.firstObject) : nullptr;
		}
	}

	cached_window        = CFBridgingRelease(window);
	cached_window_expiry = now + K_WINDOW_ELEMENT_LIFETIME;

	return cached_window;
}

auto RiotClient::Native::wait_for_interface_window(const std::atomic<bool>* t_cancel) -> pid_t
{
	const auto started              = std::chrono::steady_clock::now();
	u32        polls                = 0;
	u32        next_progress_report = 20;

	debug_log::write(K_LOG_CATEGORY, "waiting for the client window (until cancelled)");

	for (;;) {
		const pid_t pid = interface_process();
		if (pid != 0 && has_visible_window(pid)) {
			interface_pid = pid;
			debug_log::write(K_LOG_CATEGORY, "client window is up (pid %d)", pid);
			return pid;
		}

		polls += 1;
		if (polls >= next_progress_report) {
			next_progress_report = polls + 50;
			const auto waited    = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started);
			debug_log::write(K_LOG_CATEGORY, "still waiting for the client window after %lldms (%s)", static_cast<long long>(waited.count()), pid == 0 ? "no interface process yet" : "no window yet");
		}

		if (is_cancelled(t_cancel)) {
			debug_log::write(K_LOG_CATEGORY, "stopped waiting for the client window (cancelled)");
			return 0;
		}

		std::this_thread::sleep_for(K_POLL_INTERVAL);
	}
}

RiotClient::RiotClient(const std::atomic<bool>* t_cancel)
	: m_cancel(t_cancel)
	, m_native(std::make_unique<Native>())
{
}

RiotClient::~RiotClient() = default;

auto RiotClient::prepare_automation() -> void {}

auto RiotClient::running_game() -> std::optional<std::string_view>
{
	bool in_progress = false;
	for_each_process([&in_progress](pid_t, std::string_view t_path) { in_progress = in_progress || file_name_of(t_path) == K_GAME_PROCESS_NAME; });

	debug_log::write(K_LOG_CATEGORY, "running game -> %s", in_progress ? K_GAME_LAUNCH_PRODUCT.data() : "none");

	return in_progress ? std::optional{K_GAME_LAUNCH_PRODUCT} : std::nullopt;
}

auto RiotClient::kill_all_client_processes(const std::atomic<bool>* t_cancel) -> void
{
	debug_log::write(K_LOG_CATEGORY, "killing every known Riot Client process");

	std::vector<pid_t> killed;
	for_each_process([&killed](pid_t t_pid, std::string_view t_path) {
		if (!is_client_process(t_path)) return;

		if (kill(t_pid, SIGKILL) == 0) {
			debug_log::write(K_LOG_CATEGORY, "killed %.*s (pid %d)", static_cast<int>(file_name_of(t_path).size()), file_name_of(t_path).data(), t_pid);
			killed.push_back(t_pid);
		} else {
			debug_log::write(K_LOG_CATEGORY, "kill FAILED for pid %d, errno=%d", t_pid, errno);
		}
	});

	const debug_log::Scope scope(K_LOG_CATEGORY, "wait for %zu killed client process(es) to exit", killed.size());

	for (const pid_t pid : killed) {
		while (kill(pid, 0) == 0 && !is_cancelled(t_cancel)) {
			std::this_thread::sleep_for(K_POLL_INTERVAL);
		}
	}
}

auto RiotClient::automation_failure_message() -> const char*
{
	return K_AUTOMATION_FAILED_MESSAGE;
}

// macOS remembers the permission for this exact build, so an update shows Pulsar as allowed while it is not. The prompt also adds Pulsar to the list in System Settings, which saves the user from adding it by hand.
auto RiotClient::request_automation_permission() -> bool
{
	NSDictionary* options = @{(__bridge NSString*)kAXTrustedCheckOptionPrompt : @YES};

	return AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
}

auto RiotClient::permission_missing_message() -> const char*
{
	return K_PERMISSION_MISSING_MESSAGE;
}

auto RiotClient::open_automation_permission_settings() -> void
{
	if (NSURL* url = [NSURL URLWithString:@(K_ACCESSIBILITY_SETTINGS_URL)]; url != nil) {
		[NSWorkspace.sharedWorkspace openURL:url];
	}
}

auto RiotClient::supports_product(std::string_view t_launch_product) -> bool
{
	return t_launch_product == "league_of_legends";
}

auto RiotClient::forwards_launch_to_open_client() -> bool
{
	return false;
}

auto RiotClient::executable_name() -> const char*
{
	return K_CLIENT_BUNDLE_NAME;
}

auto RiotClient::default_install_folder() -> std::string
{
	return K_DEFAULT_INSTALL_FOLDER;
}

auto RiotClient::executable_near(std::string_view t_chosen_path) -> std::string
{
	std::string folder{t_chosen_path};
	if (folder.ends_with(".app") && is_file(folder + std::string{K_SERVICES_IN_BUNDLE})) return folder + std::string{K_SERVICES_IN_BUNDLE};

	for (u32 level = 0; level < K_CHOSEN_PATH_SEARCH_LEVELS; level += 1) {
		const usize slash = folder.find_last_of('/');
		if (slash == std::string::npos) break;

		folder.resize(slash);

		const std::string candidate = folder + "/" + K_CLIENT_BUNDLE_NAME + std::string{K_SERVICES_IN_BUNDLE};
		if (is_file(candidate)) return candidate;
	}

	return {};
}

auto RiotClient::resolve_executable_path(std::string_view t_remembered_path) -> bool
{
	const std::string remembered{t_remembered_path};

	if (is_file(remembered)) {
		m_executable_path = remembered;
		debug_log::write(K_LOG_CATEGORY, "using the remembered Riot Client executable: %s", m_executable_path.c_str());

		return true;
	}

	if (!remembered.empty()) {
		debug_log::write(K_LOG_CATEGORY, "the remembered Riot Client executable is gone, searching again");
	}

	m_executable_path.clear();

	for (const std::string& candidate : client_path_candidates()) {
		if (!is_file(candidate)) continue;

		m_executable_path = candidate;
		debug_log::write(K_LOG_CATEGORY, "resolved Riot Client executable: %s", m_executable_path.c_str());

		return true;
	}

	debug_log::write(K_LOG_CATEGORY, "no Riot Client executable found in the installs file, LaunchServices, running processes or the default folder");

	return false;
}

auto RiotClient::launch(std::string_view t_launch_product) -> bool
{
	NSURL* bundle = bundle_of(m_executable_path);
	if (bundle == nil) return false;

	NSWorkspaceOpenConfiguration* configuration = [NSWorkspaceOpenConfiguration configuration];
	configuration.createsNewApplicationInstance = YES;

	if (!t_launch_product.empty()) {
		configuration.arguments = @[
			[@"--launch-product=" stringByAppendingString:os::macos::to_ns_string(t_launch_product)],
			@"--launch-patchline=live",
		];
	}

	// LaunchServices starts the client as its own app, so macOS does not hold Pulsar responsible for the client's permission prompts.
	__block pid_t              launched = 0;
	const dispatch_semaphore_t done     = dispatch_semaphore_create(0);
	const auto                 finished = ^(NSRunningApplication* t_application, NSError* t_error) {
		launched = t_application != nil ? t_application.processIdentifier : 0;

		if (t_error != nil) {
			debug_log::write(K_LOG_CATEGORY, "launch FAILED: %s", t_error.localizedDescription.UTF8String);
		}

		dispatch_semaphore_signal(done);
	};

	[NSWorkspace.sharedWorkspace openApplicationAtURL:bundle configuration:configuration completionHandler:finished];

	const auto timeout = std::chrono::duration_cast<std::chrono::nanoseconds>(K_LAUNCH_TIMEOUT).count();
	if (dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, timeout)) != 0 || launched == 0) return false;

	debug_log::write(K_LOG_CATEGORY, "launched Riot Client pid %d (%s)", launched, m_executable_path.c_str());

	return true;
}

auto RiotClient::has_open_window() const -> bool
{
	const pid_t pid = interface_process();

	return pid != 0 && has_visible_window(pid);
}

auto RiotClient::bring_to_front() -> void
{
	const pid_t pid = m_native->wait_for_interface_window(m_cancel);

	if (pid != 0) {
		[[NSRunningApplication runningApplicationWithProcessIdentifier:pid] activateWithOptions:NSApplicationActivateAllWindows];
	}
}

auto RiotClient::start_automation() -> bool
{
	if (!AXIsProcessTrusted()) {
		debug_log::write(K_LOG_CATEGORY, "Pulsar lost the Accessibility permission during the login");
		return false;
	}

	const pid_t pid = m_native->interface_pid != 0 ? m_native->interface_pid : interface_process();
	if (pid == 0) return false;

	const AXUIElementRef application = AXUIElementCreateApplication(pid);
	AXUIElementSetMessagingTimeout(application, K_MESSAGING_TIMEOUT);

	// Chromium-based clients only build their accessibility tree once something asks for it.
	if (AXUIElementSetAttributeValue(application, CFSTR("AXManualAccessibility"), kCFBooleanTrue) != kAXErrorSuccess) {
		AXUIElementSetAttributeValue(application, CFSTR("AXEnhancedUserInterface"), kCFBooleanTrue);
	}

	m_native->application = CFBridgingRelease(application);

	return true;
}

auto RiotClient::stop_automation() -> void
{
	m_native->cached_window = nil;
	m_native->application   = nil;
}

auto RiotClient::look() -> ClientView
{
	if (m_native->application == nil) return {};

	const id window = m_native->current_window();
	if (window == nil) return {};

	ClientView view{.window_shown = true};

	id   username_field = nil;
	id   password_field = nil;
	bool by_name        = true;

	if (find_login_fields(window, m_cancel, &username_field, &password_field, &by_name)) {
		view.login_form_shown = true;
		view.login_error      = shown_login_error(window, m_cancel, by_name);
		return view;
	}

	const id play     = find_play_button(window, m_cancel);
	view.play_shown   = play != nil;
	view.play_enabled = play != nil && is_enabled(play);

	return view;
}

auto RiotClient::fill_and_submit(std::string_view t_username, std::string_view t_password, SubmitWith t_submit) -> bool
{
	if (m_native->application == nil) return false;

	const id window         = m_native->current_window();
	id       username_field = nil;
	id       password_field = nil;
	bool     by_name        = true;

	if (window == nil || !find_login_fields(window, m_cancel, &username_field, &password_field, &by_name)) {
		debug_log::write(K_LOG_CATEGORY, "the login form went away before it could be filled");
		return false;
	}

	const pid_t pid = m_native->interface_pid;
	fill_field(pid, username_field, os::macos::to_ns_string(t_username));
	fill_field(pid, password_field, os::macos::to_ns_string(t_password));

	if (t_submit == SubmitWith::SIGN_IN_BUTTON_OR_ENTER) {
		const id sign_in = find_element(window, m_cancel, [](const AxNode& t_node) { return is_button_named(t_node, K_SIGN_IN_BUTTON_NAME); });

		if (sign_in != nil && is_enabled(sign_in) && AXUIElementPerformAction((__bridge AXUIElementRef)sign_in, kAXPressAction) == kAXErrorSuccess) {
			debug_log::write(K_LOG_CATEGORY, "submitted the login form with its Sign in button");
			return true;
		}
	}

	focus(password_field);
	wait_for_keyboard_focus(password_field);

	const char* focus_state = has_keyboard_focus(password_field) ? "has" : "does NOT have";
	debug_log::write(K_LOG_CATEGORY, "submitting the login form with Return (password field %s keyboard focus)", focus_state);
	post_key(pid, kVK_Return, nullptr, 0);

	return true;
}

auto RiotClient::press_play() -> bool
{
	if (m_native->application == nil) return false;

	const id window = m_native->current_window();
	const id play   = window != nil ? find_play_button(window, m_cancel) : nil;

	debug_log::write(K_LOG_CATEGORY, play != nil ? "pressing Play" : "Play went away before it could be pressed");

	return play != nil && AXUIElementPerformAction((__bridge AXUIElementRef)play, kAXPressAction) == kAXErrorSuccess;
}
