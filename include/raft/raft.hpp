#pragma once
#include "detail/detail.hpp"
namespace xraft
{
	class raft
	{
	public:
		using append_log_callback = std::function<void(bool)>;
		raft()
		{

		}
		void append_log(std::string &&log, append_log_callback&&callback)
		{
		}
	private:
		std::map<int64_t, append_log_callback> append_log_callbacks_;
		int64_t last_index_;
		detail::filelog log_;
		detail::timer timer_;
	};
}