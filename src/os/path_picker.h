#pragma once

#include <memory>
#include <optional>
#include <string>

#include "core/types.h"

namespace os {

class Window;

enum class PathKind : u8 {
	Folder,
	File,
};

struct PathRequest {
	PathKind    kind     = PathKind::Folder;
	const char* title    = "";
	const char* ok_label = nullptr;
	std::string start_path;
	const char* file_type_name    = nullptr;
	const char* file_type_pattern = nullptr;
};

class PathPicker {
  public:
	PathPicker();
	~PathPicker();

	PathPicker(const PathPicker&)                    = delete;
	auto operator=(const PathPicker&) -> PathPicker& = delete;

	auto open(const Window* t_owner, PathRequest t_request) -> void;
	[[nodiscard]] auto is_open() const -> bool;
	[[nodiscard]] auto take_result() -> std::optional<std::string>;

  private:
	struct Native;

	std::unique_ptr<Native> m_native;
};

}
