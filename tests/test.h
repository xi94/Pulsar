#pragma once

// A small test harness. TEST_CASE registers a function that main.cpp runs, CHECK records a failure and carries on, and REQUIRE
// records one and leaves the test case. Wrap an expression that has a braced initializer in extra parentheses: CHECK((a == Rect{1, 2})).

namespace test {

using Function = auto (*)() -> void;

auto add_case(const char* t_name, Function t_run) -> bool;
auto fail(const char* t_expression, const char* t_file, int t_line) -> void;

}

#define PULSAR_TEST_JOIN_IMPL(a, b) a##b
#define PULSAR_TEST_JOIN(a, b)      PULSAR_TEST_JOIN_IMPL(a, b)

#define PULSAR_TEST_CASE(name, function)                                                                                                                       \
	static auto function() -> void;                                                                                                                            \
	static const bool PULSAR_TEST_JOIN(function, _added) = test::add_case(name, function);                                                                     \
	static auto function() -> void

#define TEST_CASE(name) PULSAR_TEST_CASE(name, PULSAR_TEST_JOIN(test_case_, __LINE__))

#define CHECK(expression) ((expression) ? void() : test::fail(#expression, __FILE__, __LINE__))

#define REQUIRE(expression)                                                                                                                                    \
	do {                                                                                                                                                       \
		if (!(expression)) {                                                                                                                                   \
			test::fail(#expression, __FILE__, __LINE__);                                                                                                       \
			return;                                                                                                                                            \
		}                                                                                                                                                      \
	} while (false)
