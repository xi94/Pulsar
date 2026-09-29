#include "core/storage.h"

#include <cstring>
#include <span>
#include <vector>

#include <Windows.h>

#include <nlohmann/json.hpp>
#include <sodium.h>

#include "core/app_identity.h"
#include "core/debug_log.h"
#include "core/file.h"
#include "core/str.h"

using nlohmann::json;

namespace {
constexpr int format_version = 1;
constexpr const char *accounts_file_name = "accounts.vault";
constexpr const char *settings_file_name = "settings.json";

bool g_settings_writable = true;
bool g_accounts_writable = true;
u8 g_saved_accounts_digest[crypto_generichash_BYTES]{};

void accounts_digest(std::span<const u8> t_plaintext, const MasterKey &t_master_key, u8 *t_out_digest)
{
	crypto_generichash(t_out_digest, crypto_generichash_BYTES, t_plaintext.data(), t_plaintext.size(),
					   t_master_key.data_key(), crypto::key_size);
}

void remember_saved_accounts(std::span<const u8> t_plaintext, const MasterKey &t_master_key)
{
	accounts_digest(t_plaintext, t_master_key, g_saved_accounts_digest);
}

bool matches_saved_accounts(std::span<const u8> t_plaintext, const MasterKey &t_master_key)
{
	u8 digest[crypto_generichash_BYTES];
	accounts_digest(t_plaintext, t_master_key, digest);

	return sodium_memcmp(digest, g_saved_accounts_digest, sizeof(digest)) == 0;
}

bool path_exists(const std::string &t_path)
{
	return GetFileAttributesA(t_path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

std::string local_app_data_root()
{
	char buffer[MAX_PATH];
	const DWORD length = GetEnvironmentVariableA("LOCALAPPDATA", buffer, sizeof(buffer));

	return length > 0 && length < sizeof(buffer) ? std::string{buffer, length} : std::string{};
}

void migrate_legacy_file(const std::string &t_root, const std::string &t_directory, const char *t_file_name)
{
	const std::string path = t_directory + "\\" + t_file_name;
	if (path_exists(path)) return;

	for (const char *legacy_folder : legacy_data_folder_names) {
		const std::string legacy_path = t_root + "\\" + legacy_folder + "\\" + t_file_name;

		if (path_exists(legacy_path) && CopyFileA(legacy_path.c_str(), path.c_str(), TRUE)) {
			debug_log::write("storage", "migrated %s from the %s folder", t_file_name, legacy_folder);
			return;
		}
	}
}

std::string storage_path(const char *t_file_name)
{
	const std::string directory = storage::data_directory();

	return directory.empty() ? std::string{} : directory + "\\" + t_file_name;
}

std::string to_hex(std::span<const u8> t_bytes)
{
	std::string hex(t_bytes.size() * 2 + 1, '\0');
	sodium_bin2hex(hex.data(), hex.size(), t_bytes.data(), t_bytes.size());
	hex.pop_back();

	return hex;
}

bool from_hex(const std::string &t_hex, std::span<u8> t_out)
{
	usize decoded = 0;

	return t_hex.size() == t_out.size() * 2 &&
		   sodium_hex2bin(t_out.data(), t_out.size(), t_hex.c_str(), t_hex.size(), nullptr, &decoded, nullptr) == 0 &&
		   decoded == t_out.size();
}

std::string to_base64(std::span<const u8> t_bytes)
{
	std::string encoded(sodium_base64_ENCODED_LEN(t_bytes.size(), sodium_base64_VARIANT_ORIGINAL), '\0');
	sodium_bin2base64(encoded.data(), encoded.size(), t_bytes.data(), t_bytes.size(), sodium_base64_VARIANT_ORIGINAL);
	encoded.resize(std::strlen(encoded.c_str()));

	return encoded;
}

bool from_base64(const std::string &t_text, std::vector<u8> &t_out)
{
	t_out.resize(t_text.size());

	usize decoded = 0;
	if (sodium_base642bin(t_out.data(), t_out.size(), t_text.c_str(), t_text.size(), nullptr, &decoded, nullptr,
						  sodium_base64_VARIANT_ORIGINAL) != 0) {
		return false;
	}

	t_out.resize(decoded);

	return true;
}

bool parse_json_file(const std::string &t_path, json &t_out)
{
	std::vector<u8> bytes;
	if (!read_whole_file(t_path.c_str(), bytes)) return false;

	t_out = json::parse(bytes.begin(), bytes.end(), nullptr, false);

	return t_out.is_object();
}

bool read_json_with_backup(const std::string &t_path, json &t_out)
{
	return parse_json_file(t_path, t_out) || parse_json_file(backup_path_for(t_path), t_out);
}

storage::LoadResult missing_or_failed(const std::string &t_path)
{
	return path_exists(t_path) ? storage::LoadResult::failed : storage::LoadResult::no_file;
}

json master_password_to_json(const Settings &t_settings)
{
	const MasterKeyParams &key = t_settings.master_key;

	return json{
		{"enabled", t_settings.master_password_enabled},
		{"salt_hex", to_hex(key.salt)},
		{"ops_limit", key.ops_limit},
		{"mem_limit", static_cast<u64>(key.mem_limit)},
		{"wrap_nonce_hex", to_hex(key.wrap_nonce)},
		{"wrapped_dek_hex", to_hex(key.wrapped_data_key)},
	};
}

void read_master_password(const json &t_json, Settings &t_settings)
{
	t_settings.master_password_enabled = false;

	const auto found = t_json.find("master_password");
	if (found == t_json.end() || !found->is_object()) return;

	const json &master_password = *found;
	MasterKeyParams &key = t_settings.master_key;

	t_settings.master_password_enabled = master_password.value("enabled", false);
	key.ops_limit = master_password.value("ops_limit", u64{0});
	key.mem_limit = static_cast<usize>(master_password.value("mem_limit", u64{0}));
	from_hex(master_password.value("salt_hex", std::string{}), key.salt);
	from_hex(master_password.value("wrap_nonce_hex", std::string{}), key.wrap_nonce);
	from_hex(master_password.value("wrapped_dek_hex", std::string{}), key.wrapped_data_key);
}

void read_appearance(const json &t_json, Settings &t_settings)
{
	t_settings.animations_enabled = t_json.value("animations_enabled", t_settings.animations_enabled);
	t_settings.animation_speed = t_json.value("animation_speed", t_settings.animation_speed);
	t_settings.font_size = t_json.value("font_pixel_size", t_settings.font_size);
	t_settings.secondary_font_size = t_json.value("secondary_font_pixel_size", t_settings.secondary_font_size);
	t_settings.corner_roundness = t_json.value("corner_roundness", t_settings.corner_roundness);

	const bool legacy_rounded_corners = t_json.value("rounded_corners_enabled", true);
	if (!legacy_rounded_corners) {
		t_settings.corner_roundness = 0.0f;
	}

	const auto accent = t_json.find("accent_color");
	if (accent != t_json.end() && accent->is_array() && accent->size() == 4) {
		const json &channels = *accent;
		t_settings.accent =
			Color{channels[0].get<u8>(), channels[1].get<u8>(), channels[2].get<u8>(), channels[3].get<u8>()};
	}

	copy_to(t_json.value("font_name", std::string{t_settings.font_name}), t_settings.font_name);

	const std::string theme = t_json.value("theme", std::string{});
	for (u32 i = 0; i < theme_count; i += 1) {
		if (theme == theme_labels[i].id) {
			t_settings.theme = static_cast<ThemeKind>(i);
		}
	}
}

void read_game_order(const json &t_json, Settings &t_settings)
{
	const auto order = t_json.find("carousel_order");
	if (order == t_json.end() || !order->is_array()) return;

	t_settings.game_order_count = 0;

	for (const json &title : *order) {
		if (!title.is_string() || t_settings.game_order_count == max_game_order) continue;

		copy_to(title.get<std::string>(), t_settings.game_order[t_settings.game_order_count]);
		t_settings.game_order_count += 1;
	}
}

storage::LoadResult read_settings(Settings &t_settings)
{
	const std::string path = storage_path(settings_file_name);
	if (path.empty()) return storage::LoadResult::failed;

	json settings;
	if (!read_json_with_backup(path, settings)) return missing_or_failed(path);

	try {
		t_settings.window_width = settings.value("window_width", t_settings.window_width);
		t_settings.window_height = settings.value("window_height", t_settings.window_height);
		t_settings.hide_accounts_from_capture =
			settings.value("exclude_account_list_from_capture", t_settings.hide_accounts_from_capture);

		const bool legacy_minimize_to_tray = settings.value("minimize_to_tray", t_settings.close_to_tray);
		t_settings.close_to_tray = settings.value("close_to_tray", legacy_minimize_to_tray);

		t_settings.block_overlay_injection =
			settings.value("block_overlay_injection", t_settings.block_overlay_injection);
		t_settings.show_notifications = settings.value("show_notifications", t_settings.show_notifications);
		t_settings.auto_lock_minutes = settings.value("auto_lock_minutes", t_settings.auto_lock_minutes);
		t_settings.zoom_stop = settings.value("carousel_zoom_stop", t_settings.zoom_stop);
		t_settings.selected_game = settings.value("carousel_selected_banner", t_settings.selected_game);
		copy_to(settings.value("last_run_version", std::string{}), t_settings.last_run_version);
		copy_to(settings.value("release_notes_version", std::string{}), t_settings.release_notes_version);
		copy_to(settings.value("release_notes", std::string{}), t_settings.release_notes);

		read_game_order(settings, t_settings);
		read_appearance(settings, t_settings);
		read_master_password(settings, t_settings);
	} catch (const json::exception &) {
		return storage::LoadResult::failed;
	}

	return storage::LoadResult::ok;
}

json game_to_json(const Game &t_game)
{
	json accounts = json::array();

	for (const Account &account : std::span{t_game.accounts, t_game.account_count}) {
		accounts.push_back(json{
			{"username", account.username},
			{"note", account.note},
			{"region", account.region},
			{"password", account.password},
			{"visible_mask", account.visible_game_mask},
			{"favorite", account.favorite},
			{"last_used", account.last_used},
			{"order", account.order},
		});
	}

	return json{{"title", std::string{t_game.title}}, {"accounts", std::move(accounts)}};
}

void read_game_accounts(const json &t_json, Game &t_game)
{
	t_game.account_count = 0;

	const auto accounts = t_json.find("accounts");
	if (accounts == t_json.end() || !accounts->is_array()) return;

	for (const json &entry : *accounts) {
		if (t_game.account_count >= max_accounts_per_game) break;

		Account &account = t_game.accounts[t_game.account_count];
		account.assign(entry.value("username", std::string{}), entry.value("note", std::string{}),
					   entry.value("password", std::string{}));
		copy_to(entry.value("region", std::string{}), account.region);
		account.visible_game_mask = entry.value("visible_mask", u16{0});
		account.favorite = entry.value("favorite", false);
		account.last_used = entry.value("last_used", i64{0});
		account.order = entry.value("order", u32{0});

		t_game.account_count += 1;
	}
}

Game *find_game(Library &t_library, std::string_view t_title)
{
	for (Game &game : t_library.games()) {
		if (game.title == t_title) return &game;
	}

	return nullptr;
}

template <typename Buffer>
struct WipedOnExit {
	Buffer &buffer;

	~WipedOnExit()
	{
		sodium_memzero(buffer.data(), buffer.size());
	}
};

bool decrypt_vault(const json &t_envelope, const MasterKey &t_master_key, std::vector<u8> &t_out_plaintext)
{
	u8 nonce[crypto::nonce_size];
	u8 tag[crypto::tag_size];
	std::vector<u8> ciphertext;

	if (!from_hex(t_envelope.value("nonce_hex", std::string{}), nonce) ||
		!from_hex(t_envelope.value("tag_hex", std::string{}), tag) ||
		!from_base64(t_envelope.value("ciphertext_b64", std::string{}), ciphertext)) {
		return false;
	}

	t_out_plaintext.resize(ciphertext.size());

	return crypto::decrypt(t_master_key.data_key(), nonce, ciphertext, tag, t_out_plaintext.data());
}

storage::LoadResult read_accounts(Library &t_library, const MasterKey &t_master_key)
{
	if (!t_master_key.is_unlocked()) return storage::LoadResult::locked;

	const std::string path = storage_path(accounts_file_name);
	if (path.empty()) return storage::LoadResult::failed;

	json envelope;
	if (!read_json_with_backup(path, envelope)) return missing_or_failed(path);

	try {
		std::vector<u8> plaintext;
		const WipedOnExit wipe_plaintext{plaintext};

		if (!decrypt_vault(envelope, t_master_key, plaintext)) return storage::LoadResult::failed;

		const json games = json::parse(plaintext.begin(), plaintext.end(), nullptr, false);
		if (!games.is_array()) return storage::LoadResult::failed;

		remember_saved_accounts(plaintext, t_master_key);

		for (const json &entry : games) {
			if (Game *game = find_game(t_library, entry.value("title", std::string{}))) {
				read_game_accounts(entry, *game);
			}
		}

		t_library.number_unordered_accounts();
	} catch (const json::exception &) {
		return storage::LoadResult::failed;
	}

	return storage::LoadResult::ok;
}
}

std::string storage::data_directory()
{
	const std::string root = local_app_data_root();
	if (root.empty()) return {};

	const std::string directory = root + "\\" + app_data_folder_name;
	CreateDirectoryA(directory.c_str(), nullptr);

	migrate_legacy_file(root, directory, accounts_file_name);
	migrate_legacy_file(root, directory, settings_file_name);

	return directory;
}

storage::LoadResult storage::load_settings(Settings &t_settings)
{
	const LoadResult result = read_settings(t_settings);

	if (result == LoadResult::failed) {
		// accounts.vault can only be decrypted with the key parameters in settings.json. Without them the app offers
		// a fresh master password, and saving under that would overwrite the real vault.
		g_settings_writable = false;
		g_accounts_writable = false;
		debug_log::write("storage", "%s did not load - neither file will be written this session", settings_file_name);
	}

	return result;
}

bool storage::save_settings(const Settings &t_settings)
{
	if (!g_settings_writable) return false;

	const std::string path = storage_path(settings_file_name);
	if (path.empty()) return false;

	json game_order = json::array();
	for (u32 i = 0; i < t_settings.game_order_count; i += 1) {
		game_order.push_back(t_settings.game_order[i]);
	}

	const Color accent = t_settings.accent;
	const json settings{
		{"format_version", format_version},
		{"window_width", t_settings.window_width},
		{"window_height", t_settings.window_height},
		{"animations_enabled", t_settings.animations_enabled},
		{"animation_speed", t_settings.animation_speed},
		{"corner_roundness", t_settings.corner_roundness},
		{"font_pixel_size", t_settings.font_size},
		{"secondary_font_pixel_size", t_settings.secondary_font_size},
		{"accent_color", json::array({accent.r, accent.g, accent.b, accent.a})},
		{"font_name", t_settings.font_name},
		{"theme", theme_labels[static_cast<u32>(t_settings.theme)].id},
		{"exclude_account_list_from_capture", t_settings.hide_accounts_from_capture},
		{"close_to_tray", t_settings.close_to_tray},
		{"block_overlay_injection", t_settings.block_overlay_injection},
		{"show_notifications", t_settings.show_notifications},
		{"auto_lock_minutes", t_settings.auto_lock_minutes},
		{"last_run_version", t_settings.last_run_version},
		{"release_notes_version", t_settings.release_notes_version},
		{"release_notes", t_settings.release_notes},
		{"carousel_zoom_stop", t_settings.zoom_stop},
		{"carousel_selected_banner", t_settings.selected_game},
		{"carousel_order", game_order},
		{"master_password", master_password_to_json(t_settings)},
	};

	return write_file_atomic(path, settings.dump(2));
}

storage::LoadResult storage::load_accounts(Library &t_library, const MasterKey &t_master_key)
{
	const LoadResult result = read_accounts(t_library, t_master_key);

	if (result == LoadResult::failed) {
		g_accounts_writable = false;
		debug_log::write("storage", "%s did not load - it will not be written this session", accounts_file_name);
	}

	return result;
}

bool storage::save_accounts(const Library &t_library, const MasterKey &t_master_key)
{
	if (!t_master_key.is_unlocked() || !g_accounts_writable) return false;

	json games = json::array();
	for (const Game &game : t_library.games()) {
		games.push_back(game_to_json(game));
	}

	std::string plaintext = games.dump();
	const WipedOnExit wipe_plaintext{plaintext};

	const std::span<const u8> plaintext_bytes{reinterpret_cast<const u8 *>(plaintext.data()), plaintext.size()};

	// A fresh nonce makes every encryption differ, so unchanged accounts have to be caught before encrypting.
	if (matches_saved_accounts(plaintext_bytes, t_master_key)) return true;

	u8 nonce[crypto::nonce_size];
	crypto::random_bytes(nonce);

	u8 tag[crypto::tag_size];
	std::vector<u8> ciphertext(plaintext.size());
	if (!crypto::encrypt(t_master_key.data_key(), nonce, plaintext_bytes, ciphertext.data(), tag)) return false;

	const json envelope{
		{"format_version", format_version},
		{"nonce_hex", to_hex(nonce)},
		{"tag_hex", to_hex(tag)},
		{"ciphertext_b64", to_base64(ciphertext)},
	};

	const std::string path = storage_path(accounts_file_name);
	if (path.empty() || !write_file_atomic(path, envelope.dump())) return false;

	remember_saved_accounts(plaintext_bytes, t_master_key);

	return true;
}

bool storage::can_save_settings()
{
	return g_settings_writable;
}

bool storage::can_save_accounts()
{
	return g_accounts_writable;
}
