#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "core/types.h"

namespace os {

enum class InjectionGuard : u8 {
	BLOCKED,
	REFUSED,
	UNSUPPORTED,
};

[[nodiscard]] auto executable_path() -> std::string;
auto launch_process(std::string_view t_executable, std::string_view t_arguments = {}) -> void;
auto open_path(std::string_view t_path) -> void;
auto open_url(std::string_view t_url) -> void;

auto register_app_identity() -> void;
[[nodiscard]] auto block_injection() -> InjectionGuard;
[[nodiscard]] auto injected_overlay() -> std::optional<std::string>;
[[nodiscard]] auto last_error() -> u32;

class SingleInstanceGuard {
  public:
	SingleInstanceGuard();
	~SingleInstanceGuard();

	SingleInstanceGuard(const SingleInstanceGuard&)                    = delete;
	auto operator=(const SingleInstanceGuard&) -> SingleInstanceGuard& = delete;

	[[nodiscard]] auto is_first_instance() const -> bool
	{
		return m_first_instance;
	}

	auto release() -> void;

  private:
	struct Native;

	std::unique_ptr<Native> m_native;
	bool                    m_first_instance = false;
};

}
