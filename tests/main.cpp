#include <cstdio>
#include <cstdlib>
#include <exception>
#include <span>
#include <string_view>

#include <sodium.h>

#include "test.h"

namespace {
constexpr u32 K_MAX_CASES = 256;

struct Case {
	const char*    name;
	test::Function run;
};

// Plain arrays are filled in before any test file registers its cases, so registration order between files doesn't matter.
Case        g_cases[K_MAX_CASES]{};
u32         g_case_count   = 0;
u32         g_failures     = 0;
const char* g_current_case = "";
}

auto test::add_case(const char* t_name, Function t_run) -> bool
{
	if (g_case_count == K_MAX_CASES) {
		std::fprintf(stderr, "more than %u test cases - raise K_MAX_CASES in tests/main.cpp\n", K_MAX_CASES);
		std::exit(EXIT_FAILURE);
	}

	g_cases[g_case_count] = Case{t_name, t_run};
	g_case_count += 1;

	return true;
}

auto test::fail(const char* t_expression, const char* t_file, int t_line) -> void
{
	std::printf("%s:%d: failed: %s\n    in \"%s\"\n", t_file, t_line, t_expression, g_current_case);
	g_failures += 1;
}

// Runs every test case, or only those whose name contains the first argument.
auto main(int t_argc, char** t_argv) -> int
{
	if (sodium_init() < 0) {
		std::printf("libsodium could not start\n");
		return EXIT_FAILURE;
	}

	const std::string_view filter = t_argc > 1 ? t_argv[1] : "";
	u32                    ran    = 0;
	u32                    failed = 0;

	for (const Case& entry : std::span{g_cases, g_case_count}) {
		if (!filter.empty() && std::string_view{entry.name}.find(filter) == std::string_view::npos) continue;

		const u32 failures_before = g_failures;
		g_current_case            = entry.name;

		try {
			entry.run();
		} catch (const std::exception& t_error) {
			test::fail(t_error.what(), "uncaught exception", 0);
		}

		ran += 1;
		failed += g_failures != failures_before ? 1 : 0;
	}

	std::printf("%u of %u test cases passed\n", ran - failed, ran);

	return failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
