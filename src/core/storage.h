#pragma once

#include <string>

#include "core/library.h"
#include "core/master_key.h"
#include "core/settings.h"

namespace storage {

enum class LoadResult : u8 {
	no_file,
	failed,
	ok,
	locked,
};

std::string data_directory();

LoadResult load_settings(Settings &t_settings);
bool save_settings(const Settings &t_settings);

LoadResult load_accounts(Library &t_library, const MasterKey &t_master_key);
bool save_accounts(const Library &t_library, const MasterKey &t_master_key);

bool can_save_settings();
bool can_save_accounts();

}
