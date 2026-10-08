#include "core/storage.h"

#include <cstring>
#include <filesystem>
#include <span>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>
#include <sodium.h>

#include "core/app_identity.h"
#include "core/debug_log.h"
#include "core/file.h"
#include "core/str.h"
#include "os/files.h"

using nlohmann::json;

namespace {
constexpr int         K_FORMAT_VERSION     = 1;
constexpr const char* K_ACCOUNTS_FILE_NAME = "accounts.vault";
constexpr const char* K_SETTINGS_FILE_NAME = "settings.json";

struct BoolSetting {
	const char* key;
	bool Settings::* value;
};

struct FloatSetting {
	const char* key;
	float Settings::* value;
	float             min;
	float             max;
};

constexpr BoolSetting K_BOOL_SETTINGS[]{
	{"animations_enabled", &Settings::animations_enabled},
	{"caret_trail", &Settings::caret_trail},
	{"background_light", &Settings::background_light},
	{"background_grain", &Settings::background_grain},
	{"snow", &Settings::snow},
	{"glass", &Settings::glass},
	{"show_notifications", &Settings::show_notifications},
	{"exclude_account_list_from_capture", &Settings::hide_from_capture},
	{"block_overlay_injection", &Settings::block_overlay_injection},
	{"close_to_tray", &Settings::close_to_tray},
};

constexpr FloatSetting K_FLOAT_SETTINGS[]{
	{"animation_speed", &Settings::animation_speed, K_ANIMATION_SPEED_MIN, K_ANIMATION_SPEED_MAX},
	{"corner_roundness", &Settings::corner_roundness, K_CORNER_ROUNDNESS_MIN, K_CORNER_ROUNDNESS_MAX},
	{"font_pixel_size", &Settings::font_size, K_FONT_SIZE_MIN, K_FONT_SIZE_MAX},
	{"secondary_font_pixel_size", &Settings::secondary_font_size, K_SECONDARY_FONT_SIZE_MIN, K_SECONDARY_FONT_SIZE_MAX},
	{"background_intensity", &Settings::background_intensity, 0.0f, 1.0f},
	{"background_light_intensity", &Settings::background_light_intensity, 0.0f, 1.0f},
	{"background_grain_intensity", &Settings::background_grain_intensity, 0.0f, 1.0f},
	{"caret_trail_strength", &Settings::caret_trail_strength, 0.0f, 1.0f},
	{"glass_blur", &Settings::glass_blur, 0.0f, 1.0f},
	{"glass_tint", &Settings::glass_tint, K_GLASS_TINT_MIN, K_GLASS_TINT_MAX},
};

bool g_settings_writable = true;
bool g_accounts_writable = true;
u8   g_saved_accounts_digest[crypto_generichash_BYTES]{};

auto accounts_digest(std::span<const u8> t_plaintext, const MasterKey* t_master_key, u8* t_out_digest) -> void
{
	crypto_generichash(t_out_digest, crypto_generichash_BYTES, t_plaintext.data(), t_plaintext.size(), t_master_key->data_key(), crypto::K_KEY_SIZE);
}

auto remember_saved_accounts(std::span<const u8> t_plaintext, const MasterKey* t_master_key) -> void
{
	accounts_digest(t_plaintext, t_master_key, g_saved_accounts_digest);
}

[[nodiscard]] auto matches_saved_accounts(std::span<const u8> t_plaintext, const MasterKey* t_master_key) -> bool
{
	u8 digest[crypto_generichash_BYTES];
	accounts_digest(t_plaintext, t_master_key, digest);

	return sodium_memcmp(digest, g_saved_accounts_digest, sizeof(digest)) == 0;
}

[[nodiscard]] auto path_exists(const std::string& t_path) -> bool
{
	std::error_code error;

	return std::filesystem::exists(to_path(t_path), error);
}

auto migrate_legacy_file(const std::string& t_root, const std::string& t_directory, const char* t_file_name) -> void
{
	const std::string path = joined_path(t_directory, t_file_name);
	if (path_exists(path)) return;

	for (const char* legacy_folder : K_LEGACY_DATA_FOLDER_NAMES) {
		const std::string legacy_path = joined_path(joined_path(t_root, legacy_folder), t_file_name);
		std::error_code   error;

		if (path_exists(legacy_path) && std::filesystem::copy_file(to_path(legacy_path), to_path(path), std::filesystem::copy_options::none, error)) {
			debug_log::write("storage", "migrated %s from the %s folder", t_file_name, legacy_folder);
			return;
		}
	}
}

[[nodiscard]] auto storage_path(const char* t_file_name) -> std::string
{
	const std::string directory = storage::data_directory();

	return directory.empty() ? std::string{} : joined_path(directory, t_file_name);
}

[[nodiscard]] auto to_hex(std::span<const u8> t_bytes) -> std::string
{
	std::string hex(t_bytes.size() * 2 + 1, '\0');
	sodium_bin2hex(hex.data(), hex.size(), t_bytes.data(), t_bytes.size());
	hex.pop_back();

	return hex;
}

auto from_hex(const std::string& t_hex, std::span<u8> t_out) -> bool
{
	usize decoded = 0;

	return t_hex.size() == t_out.size() * 2 && sodium_hex2bin(t_out.data(), t_out.size(), t_hex.c_str(), t_hex.size(), nullptr, &decoded, nullptr) == 0 &&
	       decoded == t_out.size();
}

[[nodiscard]] auto to_base64(std::span<const u8> t_bytes) -> std::string
{
	std::string encoded(sodium_base64_ENCODED_LEN(t_bytes.size(), sodium_base64_VARIANT_ORIGINAL), '\0');
	sodium_bin2base64(encoded.data(), encoded.size(), t_bytes.data(), t_bytes.size(), sodium_base64_VARIANT_ORIGINAL);
	encoded.resize(std::strlen(encoded.c_str()));

	return encoded;
}

[[nodiscard]] auto from_base64(const std::string& t_text, std::vector<u8>* t_out) -> bool
{
	t_out->resize(t_text.size());

	usize decoded = 0;
	if (sodium_base642bin(t_out->data(), t_out->size(), t_text.c_str(), t_text.size(), nullptr, &decoded, nullptr, sodium_base64_VARIANT_ORIGINAL) != 0) {
		return false;
	}

	t_out->resize(decoded);

	return true;
}

[[nodiscard]] auto parse_json_file(const std::string& t_path, json* t_out) -> bool
{
	std::vector<u8> bytes;
	if (!read_whole_file(t_path, &bytes)) return false;

	*t_out = json::parse(bytes.begin(), bytes.end(), nullptr, false);

	return t_out->is_object();
}

[[nodiscard]] auto read_json_with_backup(const std::string& t_path, json* t_out) -> bool
{
	return parse_json_file(t_path, t_out) || parse_json_file(backup_path_for(t_path), t_out);
}

[[nodiscard]] auto missing_or_failed(const std::string& t_path) -> storage::LoadResult
{
	return path_exists(t_path) ? storage::LoadResult::FAILED : storage::LoadResult::NO_FILE;
}

[[nodiscard]] auto master_password_to_json(const Settings* t_settings) -> json
{
	const MasterKeyParams& key = t_settings->master_key;

	return json{
		{"enabled", t_settings->master_password_enabled},
		{"salt_hex", to_hex(key.salt)},
		{"ops_limit", key.ops_limit},
		{"mem_limit", static_cast<u64>(key.mem_limit)},
		{"wrap_nonce_hex", to_hex(key.wrap_nonce)},
		{"wrapped_dek_hex", to_hex(key.wrapped_data_key)},
	};
}

auto read_master_password(const json& t_json, Settings* t_settings) -> void
{
	t_settings->master_password_enabled = false;

	const auto found = t_json.find("master_password");
	if (found == t_json.end() || !found->is_object()) return;

	MasterKeyParams* key = &t_settings->master_key;

	t_settings->master_password_enabled = found->value("enabled", false);
	key->ops_limit                      = found->value("ops_limit", u64{0});
	key->mem_limit                      = static_cast<usize>(found->value("mem_limit", u64{0}));
	from_hex(found->value("salt_hex", std::string{}), key->salt);
	from_hex(found->value("wrap_nonce_hex", std::string{}), key->wrap_nonce);
	from_hex(found->value("wrapped_dek_hex", std::string{}), key->wrapped_data_key);
}

auto read_bool_and_float_settings(const json& t_json, Settings* t_settings) -> void
{
	t_settings->close_to_tray = t_json.value("minimize_to_tray", t_settings->close_to_tray);

	for (const BoolSetting& setting : K_BOOL_SETTINGS) {
		t_settings->*setting.value = t_json.value(setting.key, t_settings->*setting.value);
	}

	for (const FloatSetting& setting : K_FLOAT_SETTINGS) {
		const float value          = t_json.value(setting.key, t_settings->*setting.value);
		t_settings->*setting.value = std::clamp(value, setting.min, setting.max);
	}

	if (!t_json.value("rounded_corners_enabled", true)) {
		t_settings->corner_roundness = 0.0f;
	}
}

auto read_appearance(const json& t_json, Settings* t_settings) -> void
{
	const auto accent = t_json.find("accent_color");
	if (accent != t_json.end() && accent->is_array() && accent->size() == 4) {
		t_settings->accent = Color{accent->at(0).get<u8>(), accent->at(1).get<u8>(), accent->at(2).get<u8>(), accent->at(3).get<u8>()};
	}

	copy_to(t_json.value("font_name", std::string{t_settings->font_name}), t_settings->font_name);

	const std::string theme = t_json.value("theme", std::string{});
	for (u32 i = 0; i < K_THEME_COUNT; i += 1) {
		if (theme == K_THEME_LABELS[i].id) {
			t_settings->theme = static_cast<ThemeKind>(i);
		}
	}

	const std::string background = t_json.value("background", std::string{});
	for (u32 i = 0; i < K_BACKGROUND_COUNT; i += 1) {
		if (background == K_BACKGROUND_LABELS[i].id) {
			t_settings->background_style = static_cast<BackgroundStyle>(i);
		}
	}
}

template <typename Choice>
[[nodiscard]] auto read_choice(const json& t_json, const char* t_key, std::span<const std::string_view> t_ids, Choice t_fallback) -> Choice
{
	const std::string id = t_json.value(t_key, std::string{});

	for (u32 i = 0; i < t_ids.size(); i += 1) {
		if (id == t_ids[i]) return static_cast<Choice>(i);
	}

	return t_fallback;
}

auto read_game_order(const json& t_json, Settings* t_settings) -> void
{
	const auto order = t_json.find("carousel_order");
	if (order == t_json.end() || !order->is_array()) return;

	t_settings->game_order_count = 0;

	for (const json& title : *order) {
		if (!title.is_string() || t_settings->game_order_count == K_MAX_GAME_ORDER) continue;

		copy_to(title.get<std::string>(), t_settings->game_order[t_settings->game_order_count]);
		t_settings->game_order_count += 1;
	}
}

[[nodiscard]] auto read_zoom_stop(const json& t_json, i32 t_fallback) -> i32
{
	if (t_json.contains("view_zoom")) return t_json.value("view_zoom", t_fallback);

	// Older saves used earlier stop layouts: carousel, grid, list, then carousel, shelf, grid, list, icons.
	const i32 legacy    = t_json.value("carousel_zoom_stop", t_fallback);
	const i32 shelf_era = t_json.value("view_zoom_stop", legacy > 0 ? legacy + 1 : legacy);

	if (shelf_era <= 1) return shelf_era;
	if (shelf_era <= 4) return shelf_era + 1;
	if (shelf_era <= 7) return shelf_era + 4;

	return shelf_era - 2;
}

[[nodiscard]] auto read_settings(Settings* t_settings) -> storage::LoadResult
{
	const std::string path = storage_path(K_SETTINGS_FILE_NAME);
	if (path.empty()) return storage::LoadResult::FAILED;

	json settings;
	if (!read_json_with_backup(path, &settings)) return missing_or_failed(path);

	try {
		t_settings->window_width      = settings.value("window_width", t_settings->window_width);
		t_settings->window_height     = settings.value("window_height", t_settings->window_height);
		t_settings->auto_lock_minutes = settings.value("auto_lock_minutes", t_settings->auto_lock_minutes);
		t_settings->zoom_stop         = read_zoom_stop(settings, t_settings->zoom_stop);
		t_settings->selected_game     = settings.value("carousel_selected_banner", t_settings->selected_game);
		t_settings->renderer          = read_choice(settings, "renderer", K_GRAPHICS_API_IDS, t_settings->renderer);
		t_settings->caret_style       = read_choice(settings, "caret_style", K_CARET_STYLE_IDS, t_settings->caret_style);
		copy_to(settings.value("riot_client_path", std::string{}), t_settings->riot_client_path);
		copy_to(settings.value("last_run_version", std::string{}), t_settings->last_run_version);
		copy_to(settings.value("release_notes_version", std::string{}), t_settings->release_notes_version);
		copy_to(settings.value("release_notes", std::string{}), t_settings->release_notes);

		read_bool_and_float_settings(settings, t_settings);
		read_game_order(settings, t_settings);
		read_appearance(settings, t_settings);
		read_master_password(settings, t_settings);
	} catch (const json::exception&) {
		return storage::LoadResult::FAILED;
	}

	return storage::LoadResult::OK;
}

[[nodiscard]] auto visible_titles(const Library* t_library, u16 t_mask) -> json
{
	json titles = json::array();

	for (u32 game = 0; game < t_library->game_count; game += 1) {
		if ((t_mask & (1u << game)) != 0) {
			titles.push_back(t_library->games[game].title);
		}
	}

	return titles;
}

[[nodiscard]] auto visible_mask(const Library* t_library, const json& t_account) -> u16
{
	const auto titles = t_account.find("visible_in");
	if (titles == t_account.end() || !titles->is_array()) return t_account.value("visible_mask", u16{0});

	u16 mask = 0;

	for (const json& title : *titles) {
		for (u32 game = 0; game < t_library->game_count && title.is_string(); game += 1) {
			if (t_library->games[game].title == title.get_ref<const std::string&>()) {
				mask |= static_cast<u16>(1u << game);
			}
		}
	}

	return mask;
}

[[nodiscard]] auto game_to_json(const Library* t_library, const Game& t_game) -> json
{
	json accounts = json::array();

	for (const Account& account : std::span{t_game.accounts, t_game.account_count}) {
		json entry{
			{"username", account.username}, {"note", account.note},           {"region", account.region}, {"password", account.password},
			{"favorite", account.favorite}, {"last_used", account.last_used}, {"order", account.order},
		};

		if (account.visible_game_mask != 0) {
			entry["visible_in"] = visible_titles(t_library, account.visible_game_mask);
		}

		accounts.push_back(std::move(entry));
	}

	return json{{"title", std::string{t_game.title}}, {"accounts", std::move(accounts)}};
}

auto read_game_accounts(const json& t_json, const Library* t_library, Game* t_game) -> void
{
	t_game->account_count = 0;

	const auto accounts = t_json.find("accounts");
	if (accounts == t_json.end() || !accounts->is_array()) return;

	for (const json& entry : *accounts) {
		if (t_game->account_count >= K_MAX_ACCOUNTS_PER_GAME) break;

		Account* account = &t_game->accounts[t_game->account_count];
		account->assign(entry.value("username", std::string{}), entry.value("note", std::string{}), entry.value("password", std::string{}));
		copy_to(entry.value("region", std::string{}), account->region);
		account->visible_game_mask = visible_mask(t_library, entry);
		account->favorite          = entry.value("favorite", false);
		account->last_used         = entry.value("last_used", i64{0});
		account->order             = entry.value("order", u32{0});

		t_game->account_count += 1;
	}
}

[[nodiscard]] auto find_game(Library* t_library, std::string_view t_title) -> Game*
{
	for (Game& game : std::span{t_library->games, t_library->game_count}) {
		if (game.title == t_title) return &game;
	}

	return nullptr;
}

template <typename Buffer>
struct WipedOnExit {
	Buffer* buffer;

	~WipedOnExit()
	{
		sodium_memzero(buffer->data(), buffer->size());
	}
};

[[nodiscard]] auto decrypt_vault(const json& t_envelope, const MasterKey* t_master_key, std::vector<u8>* t_out_plaintext) -> bool
{
	u8              nonce[crypto::K_NONCE_SIZE];
	u8              tag[crypto::K_TAG_SIZE];
	std::vector<u8> ciphertext;

	if (!from_hex(t_envelope.value("nonce_hex", std::string{}), nonce) || !from_hex(t_envelope.value("tag_hex", std::string{}), tag) ||
	    !from_base64(t_envelope.value("ciphertext_b64", std::string{}), &ciphertext)) {
		return false;
	}

	t_out_plaintext->resize(ciphertext.size());

	return crypto::decrypt(t_master_key->data_key(), nonce, ciphertext, tag, t_out_plaintext->data());
}

[[nodiscard]] auto read_accounts(Library* t_library, const MasterKey* t_master_key) -> storage::LoadResult
{
	if (!t_master_key->is_unlocked()) return storage::LoadResult::LOCKED;

	const std::string path = storage_path(K_ACCOUNTS_FILE_NAME);
	if (path.empty()) return storage::LoadResult::FAILED;

	json envelope;
	if (!read_json_with_backup(path, &envelope)) return missing_or_failed(path);

	try {
		std::vector<u8>   plaintext;
		const WipedOnExit wipe_plaintext{&plaintext};

		if (!decrypt_vault(envelope, t_master_key, &plaintext)) return storage::LoadResult::FAILED;

		const json games = json::parse(plaintext.begin(), plaintext.end(), nullptr, false);
		if (!games.is_array()) return storage::LoadResult::FAILED;

		remember_saved_accounts(plaintext, t_master_key);

		json unlisted = json::array();

		for (const json& entry : games) {
			if (Game* game = find_game(t_library, entry.value("title", std::string{}))) {
				read_game_accounts(entry, t_library, game);
			} else {
				unlisted.push_back(entry);
			}
		}

		t_library->unlisted_games = unlisted.empty() ? std::string{} : unlisted.dump();

		t_library->number_unordered_accounts();
	} catch (const json::exception&) {
		return storage::LoadResult::FAILED;
	}

	return storage::LoadResult::OK;
}
}

auto storage::data_directory() -> std::string
{
	const std::string root = os::user_data_folder();
	if (root.empty()) return {};

	const std::string directory = joined_path(root, K_APP_DATA_FOLDER_NAME);
	std::error_code   error;
	std::filesystem::create_directory(to_path(directory), error);

	migrate_legacy_file(root, directory, K_ACCOUNTS_FILE_NAME);
	migrate_legacy_file(root, directory, K_SETTINGS_FILE_NAME);

	return directory;
}

auto storage::load_settings(Settings* t_settings) -> storage::LoadResult
{
	const LoadResult result = read_settings(t_settings);

	if (result == LoadResult::FAILED) {
		// accounts.vault can only be decrypted with the key parameters in settings.json. Without them the app offers
		// a fresh master password, and saving under that would overwrite the real vault.
		g_settings_writable = false;
		g_accounts_writable = false;
		debug_log::write("storage", "%s did not load - neither file will be written this session", K_SETTINGS_FILE_NAME);
	}

	return result;
}

auto storage::save_settings(const Settings* t_settings) -> bool
{
	if (!g_settings_writable) return false;

	const std::string path = storage_path(K_SETTINGS_FILE_NAME);
	if (path.empty()) return false;

	json game_order = json::array();
	for (u32 i = 0; i < t_settings->game_order_count; i += 1) {
		game_order.push_back(t_settings->game_order[i]);
	}

	const Color accent = t_settings->accent;
	json        settings{
		{"format_version", K_FORMAT_VERSION},
		{"window_width", t_settings->window_width},
		{"window_height", t_settings->window_height},
		{"background", K_BACKGROUND_LABELS[static_cast<u32>(t_settings->background_style)].id},
		{"accent_color", json::array({accent.r, accent.g, accent.b, accent.a})},
		{"font_name", t_settings->font_name},
		{"theme", K_THEME_LABELS[static_cast<u32>(t_settings->theme)].id},
		{"renderer", K_GRAPHICS_API_IDS[static_cast<u32>(t_settings->renderer)]},
		{"caret_style", K_CARET_STYLE_IDS[static_cast<u32>(t_settings->caret_style)]},
		{"auto_lock_minutes", t_settings->auto_lock_minutes},
		{"riot_client_path", t_settings->riot_client_path},
		{"last_run_version", t_settings->last_run_version},
		{"release_notes_version", t_settings->release_notes_version},
		{"release_notes", t_settings->release_notes},
		{"view_zoom", t_settings->zoom_stop},
		{"carousel_selected_banner", t_settings->selected_game},
		{"carousel_order", game_order},
		{"master_password", master_password_to_json(t_settings)},
	};

	for (const BoolSetting& setting : K_BOOL_SETTINGS) {
		settings[setting.key] = t_settings->*setting.value;
	}

	for (const FloatSetting& setting : K_FLOAT_SETTINGS) {
		settings[setting.key] = t_settings->*setting.value;
	}

	return write_file_atomic(path, settings.dump(2));
}

auto storage::load_accounts(Library* t_library, const MasterKey* t_master_key) -> storage::LoadResult
{
	const LoadResult result = read_accounts(t_library, t_master_key);

	if (result == LoadResult::FAILED) {
		g_accounts_writable = false;
		debug_log::write("storage", "%s did not load - it will not be written this session", K_ACCOUNTS_FILE_NAME);
	}

	return result;
}

auto storage::save_accounts(const Library* t_library, const MasterKey* t_master_key) -> bool
{
	if (!t_master_key->is_unlocked() || !g_accounts_writable) return false;

	json games = json::array();
	for (const Game& game : std::span{t_library->games, t_library->game_count}) {
		games.push_back(game_to_json(t_library, game));
	}

	if (!t_library->unlisted_games.empty()) {
		for (json& entry : json::parse(t_library->unlisted_games)) {
			games.push_back(std::move(entry));
		}
	}

	std::string       plaintext = games.dump();
	const WipedOnExit wipe_plaintext{&plaintext};

	const std::span<const u8> plaintext_bytes{reinterpret_cast<const u8*>(plaintext.data()), plaintext.size()};

	// A fresh nonce makes every encryption differ, so unchanged accounts have to be caught before encrypting.
	if (matches_saved_accounts(plaintext_bytes, t_master_key)) return true;

	u8 nonce[crypto::K_NONCE_SIZE];
	crypto::random_bytes(nonce);

	u8              tag[crypto::K_TAG_SIZE];
	std::vector<u8> ciphertext(plaintext.size());
	if (!crypto::encrypt(t_master_key->data_key(), nonce, plaintext_bytes, ciphertext.data(), tag)) return false;

	const json envelope{
		{"format_version", K_FORMAT_VERSION},
		{"nonce_hex", to_hex(nonce)},
		{"tag_hex", to_hex(tag)},
		{"ciphertext_b64", to_base64(ciphertext)},
	};

	const std::string path = storage_path(K_ACCOUNTS_FILE_NAME);
	if (path.empty() || !write_file_atomic(path, envelope.dump())) return false;

	remember_saved_accounts(plaintext_bytes, t_master_key);

	return true;
}

auto storage::can_save_settings() -> bool
{
	return g_settings_writable;
}

auto storage::can_save_accounts() -> bool
{
	return g_accounts_writable;
}
