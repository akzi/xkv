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
			insert_callback(index, std::move(callback));
			set_timeout(index);
			notify_peers();
		}
	private:
		void notify_peers()
		{
			for (auto &itr : pees_)
				itr->notify();
		}
		void insert_callback(int64_t index, append_log_callback &&callback)
		{
			utils::lock_guard lock(append_log_callbacks_mtx_);
			append_log_callbacks_.emplace(
				std::piecewise_construct, std::forward_as_tuple(index),
				std::forward_as_tuple(raft_config_mgr_.get_majority() -1, std::move(callback)));
		}
		void set_timeout(int64_t index)
		{
			timer_.set_timer(append_log_timeout_, [this, index] {
				utils::lock_guard lock(append_log_callbacks_mtx_);
				auto itr = append_log_callbacks_.find(index);
				if (itr == append_log_callbacks_.end())
					return;
				append_log_callback func;
				commiter_.push(std::move([func = std::move(itr->second.second)]{ func(false); }));
				append_log_callbacks_.erase(itr);
			});
		}
		log_entry build_log_entry(std::string &&log, log_entry::type type = log_entry::type::e_append_log)
		{
			return {};
		}
		void append_entries_callback(const std::vector<int64_t> &indexs)
		{
			utils::lock_guard lock(append_log_callbacks_mtx_);
			for (auto &itr: indexs)
			{
				auto item = append_log_callbacks_.find(itr);
				if (item == append_log_callbacks_.end())
					continue;
				item->second.first--;
				if (item->second.first == 0)
				{
					auto func = std::move(item->second.second);
					commiter_.push(std::move([func] {func(true); }));
					append_log_callbacks_.erase(item);
				}
			}
		}
		raft_config_mgr raft_config_mgr_;
		int node_count_ = 3;
		int append_log_timeout_ = 100000;//10 seconds;
		std::vector<std::unique_ptr<raft_peer>> pees_;
		status status_;
		std::mutex append_log_callbacks_mtx_;
		std::map<int64_t,std::pair<int,append_log_callback>> append_log_callbacks_;
		detail::filelog log_;
		detail::timer timer_;
		committer commiter_;
	};
}