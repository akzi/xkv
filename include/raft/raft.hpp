#pragma once
#include "detail/detail.hpp"
namespace xraft
{
	using namespace detail;
	class raft
	{
	public:
		using append_log_callback = std::function<void(bool)>;
		enum status
		{
			e_follower,
			e_candidate,
			e_leader
		};
		raft()
		{

		}
		bool check_leader()
		{
			return status_ == status::e_leader;
		}
		void append_log(std::string &&log, append_log_callback&&callback)
		{
			auto index = log_.write(build_log_entry(std::move(log)));
			auto timer_id = set_timeout(index);
			insert_callback(index, timer_id, std::move(callback));
			notify_peers();
		}
	private:
		struct append_log_callback_info
		{
			append_log_callback_info(int waits, int64_t timer_id, append_log_callback && callback)
				:waits_(waits),
				timer_id_(timer_id),
				callback_(callback){}

			int waits_;
			int64_t timer_id_;
			append_log_callback callback_;
		};
		void notify_peers()
		{
			for (auto &itr : pees_)
				itr->notify();
		}
		void insert_callback(int64_t index, int64_t timer_id, append_log_callback &&callback)
		{
			utils::lock_guard lock(append_log_callbacks_mtx_);
			append_log_callbacks_.emplace(
				std::piecewise_construct, std::forward_as_tuple(index),
				std::forward_as_tuple(raft_config_mgr_.get_majority() - 1, timer_id, std::move(callback)));
		}
		int64_t set_timeout(int64_t index)
		{
			return timer_.set_timer(append_log_timeout_, [this, index] {
				utils::lock_guard lock(append_log_callbacks_mtx_);
				auto itr = append_log_callbacks_.find(index);
				if (itr == append_log_callbacks_.end())
					return;
				append_log_callback func;
				timer_.cancel(itr->second.timer_id_);
				commiter_.push(std::move([func = std::move(itr->second.callback_)]{ func(false); }));
				append_log_callbacks_.erase(itr);
			});
		}
		log_entry build_log_entry(std::string &&log, log_entry::type type = log_entry::type::e_append_log)
		{
			log_entry entry;
			entry.term_ = term_;
			entry.log_data_ = std::move(log);
			entry.type_ = type;
			return std::move(entry);
		}
		append_entries_request build_append_entries_request(int64_t index)
		{
			log_.get_log_entries(index);
		}
		void append_entries_callback(const std::vector<int64_t> &indexs)
		{
			utils::lock_guard lock(append_log_callbacks_mtx_);
			for (auto &itr : indexs)
			{
				auto item = append_log_callbacks_.find(itr);
				if (item == append_log_callbacks_.end())
					continue;
				item->second.waits_--;
				if (item->second.waits_ == 0)
				{
					auto func = std::move(item->second.callback_);
					commiter_.push(std::move([func] {func(true); }));
					append_log_callbacks_.erase(item);
				}
			}
		}

		int64_t term_;
		raft_config_mgr raft_config_mgr_;
		int64_t append_log_timeout_ = 100000;//10 seconds;
		std::vector<std::unique_ptr<raft_peer>> pees_;
		status status_;
		std::mutex append_log_callbacks_mtx_;
		std::map<int64_t, append_log_callback_info> append_log_callbacks_;
		detail::filelog log_;
		detail::timer timer_;
		committer commiter_;
	};
}