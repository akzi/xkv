#pragma once
#include "detail/detail.hpp"
namespace xraft
{
	using namespace detail;
	class raft
	{
	public:
		using append_log_callback = std::function<void(bool)>;
		enum state
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
			return state_ == state::e_leader;
		}
		void replicate(std::string &&data, append_log_callback&&callback)
		{
			auto index = log_.write(build_log_entry(std::move(data)));
			insert_callback(index, set_timeout(index), std::move(callback));
			notify_peers();
		}
		
	private:
		append_entries_response handle_append_entries_request(append_entries_request && request)
		{

		}
		vote_response handle_vote_request(const vote_request &request)
		{
			vote_response response;
			auto is_ok = false;
			if (request.last_log_term_ > get_last_log_entry_term() ||
				(request.last_log_term_ == get_last_log_entry_term() &&
					request.last_log_index_ >= get_last_log_entry_index()))
				is_ok = true;
			//todo hold votes check

			if (request.term_ > current_term_)
			{
				step_down(request.term_);
			}

			if (request.term_ == current_term_) {
				if (is_ok && voted_for_.empty()) {
					step_down(current_term_);
					voted_for_ = request.candidate_;
				}
			}
			response.term_ = current_term_;
			response.vote_granted_ = 
				(request.term_ == current_term_ && 
					voted_for_ == request.candidate_);
			response.log_ok_ = is_ok;
			return response;
		}
		install_snapshot_response handle_install_snapshot (install_snapshot_request &snapshot)
		{

		}
		void step_down(int64_t new_term)
		{
			if (current_term_ < new_term)
			{
				current_term_ = new_term;
				leader_id_.clear();
				voted_for_.clear();
				update_Log_metadata();
			}
			if (state_ == state::e_leader)
				sleep_peer_threads();
			state_ = state::e_follower;
			set_election_timer();
		}
		void set_election_timer()
		{
			timer_.cancel(election_timer_id_);
			timer_.set_timer(election_timeout_ ,[this] {
				std::lock_guard<std::mutex> lock(mtx_);
				for (auto &itr : pees_)
					itr->send_cmd(raft_peer::cmd_t::e_election);
			});
		}
		void sleep_peer_threads()
		{
			for (auto &itr : pees_)
				itr->send_cmd(raft_peer::cmd_t::e_sleep);
		}
		void update_Log_metadata()
		{

		}
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
			utils::lock_guard lock(mtx_);
			append_log_callbacks_.emplace(
				std::piecewise_construct, std::forward_as_tuple(index),
				std::forward_as_tuple(raft_config_mgr_.get_majority() - 1, timer_id, std::move(callback)));
		}
		int64_t set_timeout(int64_t index)
		{
			return timer_.set_timer(append_log_timeout_, [this, index] {
				utils::lock_guard lock(mtx_);
				auto itr = append_log_callbacks_.find(index);
				if (itr == append_log_callbacks_.end())
					return;
				append_log_callback func;
				commiter_.push([func = std::move(itr->second.callback_)]{ func(false); });
				append_log_callbacks_.erase(itr);
			});
		}
		log_entry build_log_entry(std::string &&log, log_entry::type type = log_entry::type::e_append_log)
		{
			log_entry entry;
			entry.term_ = current_term_;
			entry.log_data_ = std::move(log);
			entry.type_ = type;
			return std::move(entry);
		}
		append_entries_request build_append_entries_request(int64_t index)
		{
			append_entries_request request;
			request.entries_ = log_.get_log_entries(index - 1);
			request.leader_commit_ = committed_index_;
			request.leader_id_ = raft_id_;
			request.prev_log_index_ = request.entries_.size() ? (request.entries_.front().index_) : 0;
			request.prev_log_term_ = request.entries_.size() ? (request.entries_.front().term_) : 0;
			request.entries_.pop_front();
			return std::move(request);
		}
		void append_entries_callback(const std::vector<int64_t> &indexs)
		{
			utils::lock_guard lock(mtx_);
			for (auto &itr : indexs)
			{
				auto item = append_log_callbacks_.find(itr);
				if (item == append_log_callbacks_.end())
					continue;
				item->second.waits_--;
				if (item->second.waits_ == 0)
				{
					timer_.cancel(item->second.timer_id_);
					committed_index_ = item->first;
					append_log_callback func;
					commiter_.push([func = std::move(item->second.callback_),this]{func(true); });
					append_log_callbacks_.erase(item);
				}
			}
		}
		int64_t get_last_log_entry_term()
		{
			return log_.get_last_log_entry_term();
		}
		int64_t get_last_log_entry_index()
		{
			return log_.get_last_index();
		}
		int64_t current_term_;
		int64_t committed_index_;
		std::string raft_id_;
		std::string voted_for_;
		std::string leader_id_;
		int64_t election_timeout_ = 10000;
		int64_t election_timer_id_;

		raft_config_mgr raft_config_mgr_;
		int64_t append_log_timeout_ = 100000;//10 seconds;
		std::vector<std::unique_ptr<raft_peer>> pees_;
		state state_;
		std::mutex mtx_;
		std::map<int64_t, append_log_callback_info> append_log_callbacks_;
		detail::filelog log_;
		detail::timer timer_;
		committer commiter_;
	};
}