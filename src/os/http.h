#pragma once

#include <atomic>
#include <string>
#include <string_view>
#include <vector>

#include "core/types.h"

namespace os {

enum class HttpResult : u8 {
	Ok,
	Cancelled,
	Failed,
};

struct HttpProgress {
	const std::atomic<bool>* cancel_requested = nullptr;
	std::atomic<u64>*        bytes_downloaded = nullptr;
	std::atomic<u64>*        total_bytes      = nullptr;
	std::atomic<double>*     bytes_per_second = nullptr;
};

[[nodiscard]] auto http_get(std::string_view t_url, usize t_max_bytes, std::vector<u8>* t_out_body, const HttpProgress& t_progress, std::string* t_out_error)
	-> HttpResult;

}
