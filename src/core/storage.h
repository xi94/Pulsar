#pragma once

#include <string>

#include "core/crypto.h"
#include "core/library.h"
#include "core/settings.h"

namespace storage {

enum class LoadResult : u8 {
	NO_FILE,
	FAILED,
	OK,
	LOCKED,
};

[[nodiscard]] auto data_directory() -> std::string;

auto load_settings(Settings* t_settings) -> LoadResult;
auto save_settings(const Settings* t_settings) -> bool;

auto load_accounts(Library* t_library, const MasterKey* t_master_key) -> LoadResult;
auto save_accounts(const Library* t_library, const MasterKey* t_master_key) -> bool;

[[nodiscard]] auto can_save_settings() -> bool;
[[nodiscard]] auto can_save_accounts() -> bool;

}
