#pragma once
namespace xraft
{
	namespace detail
	{
		
		class timer
		{
		public:
			timer()
			{

			}
			int64_t set_timer(int64_t timeout, std::function<void()> &&callback)
			{
				using namespace std::chrono;
				utils::lock_guard lock(mtx_);
				auto timer_point = high_resolution_clock::now()
					+ high_resolution_clock::duration(milliseconds(timeout));
				auto timer_id = gen_timer_id();
				events_.emplace(std::piecewise_construct, 
					std::forward_as_tuple(timer_point), 
					std::forward_as_tuple(timer_id,std::move(callback)));
				return timer_id;
			}
			void cancel(int64_t timer_id)
			{
				utils::lock_guard lock(mtx_);
				for (auto &itr = events_.begin(); itr != events_.end(); itr++)
				{
					if (itr->second.first == timer_id)
					{
						events_.erase(itr);
						return;
					}
				}
			}
		private:
			int64_t gen_timer_id()
			{
				return ++timer_id_;
			}
			std::mutex mtx_;
			int64_t timer_id_ = 1;
			std::multimap<int64_t, std::pair<int64_t, std::function<void()>>> events_;
			std::thread checker_;
		};
	}
}