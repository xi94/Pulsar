#pragma once

#include "core/types.h"

// Arrays indexed by an enum and loops over every value of one, so code never casts between enums and integers. The enum's last value is
// COUNT, which counts the others.
template <typename E>
constexpr usize K_ENUM_COUNT = static_cast<usize>(E::COUNT);

template <typename E, typename T>
struct EnumArray {
	T items[K_ENUM_COUNT<E>];

	[[nodiscard]] constexpr auto operator[](E t_key) -> T&
	{
		return items[static_cast<usize>(t_key)];
	}

	[[nodiscard]] constexpr auto operator[](E t_key) const -> const T&
	{
		return items[static_cast<usize>(t_key)];
	}

	[[nodiscard]] constexpr auto begin() -> T*
	{
		return items;
	}

	[[nodiscard]] constexpr auto end() -> T*
	{
		return items + K_ENUM_COUNT<E>;
	}

	[[nodiscard]] constexpr auto begin() const -> const T*
	{
		return items;
	}

	[[nodiscard]] constexpr auto end() const -> const T*
	{
		return items + K_ENUM_COUNT<E>;
	}
};

template <typename E>
struct EnumValues {
	struct Iterator {
		usize index;

		[[nodiscard]] constexpr auto operator*() const -> E
		{
			return static_cast<E>(index);
		}

		constexpr auto operator++() -> Iterator&
		{
			index += 1;
			return *this;
		}

		[[nodiscard]] constexpr auto operator==(const Iterator&) const -> bool = default;
	};

	[[nodiscard]] constexpr auto begin() const -> Iterator
	{
		return Iterator{0};
	}

	[[nodiscard]] constexpr auto end() const -> Iterator
	{
		return Iterator{K_ENUM_COUNT<E>};
	}
};

// Every value of E in order: for (const ThemeKind theme : each<ThemeKind>()).
template <typename E>
[[nodiscard]] constexpr auto each() -> EnumValues<E>
{
	return EnumValues<E>{};
}

// Where t_value falls among E's values, and the value at a place, for controls that list every value in order and for data handed to the GPU.
// These are the only conversions between an enum and its position.
template <typename E>
[[nodiscard]] constexpr auto enum_index(E t_value) -> u32
{
	return static_cast<u32>(t_value);
}

template <typename E>
[[nodiscard]] constexpr auto enum_at(u32 t_index) -> E
{
	return static_cast<E>(t_index);
}

// The value t_step places from t_value, wrapping round past either end.
template <typename E>
[[nodiscard]] constexpr auto stepped(E t_value, i32 t_step) -> E
{
	const auto count = static_cast<i32>(K_ENUM_COUNT<E>);
	const i32  index = (static_cast<i32>(enum_index(t_value)) + t_step) % count;

	return enum_at<E>(static_cast<u32>((index + count) % count));
}
