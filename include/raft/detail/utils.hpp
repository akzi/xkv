#pragma once
namespace xraft
{
namespace detail
{
	namespace utils
	{
		using lock_guard = std::lock_guard<std::mutex>;
	}
}
}