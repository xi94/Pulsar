#pragma once

namespace overlay_guard {

enum class BlockResult : u8 {
	blocked,
	refused,
	unsupported,
};

void apply_process_identity();
BlockResult block_hook_injection();
const wchar_t *injected_overlay_module();

}
