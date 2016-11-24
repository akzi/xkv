#pragma once
namespace xraft
{
namespace detail
{
	using namespace std::chrono;
	class raft_peer
	{
		
	public:
		enum cmd_t
		{
			e_connect,
			e_election,
			e_append_entries,
			e_sleep,

		};
		struct config
			
		{
			std::string peer_ip_;
			int port_;
		};

		raft_peer()
			:peer_thread_([this] { run(); })
		{

		}
		void init(config _config)
		{
			config_ = _config;
		}
	
		void send_cmd(cmd_t cmd)
		{
			cmd_queue_.push(std::move(cmd));
			notify();
		}
		void notify()
		{
			utils::lock_guard locker(mtx_);
			if (is_sleep_)
				cv_.notify_one();
		}
		std::function<void(raft_peer&, bool)> connect_callback_;
		std::function<int64_t(void)> get_current_term_;
		std::function<int64_t(void)> get_last_log_index_;
		std::function<append_entries_request(int64_t)> build_append_entries_request_;
		std::function<void(append_entries_request &&)> append_entries_request_;
		std::function<void(append_entries_response &&)> append_entries_response_;
		std::function<vote_request()> build_vote_request_;
		std::function<void(vote_response &&)> vote_response_;
		std::function<void(install_snapshot &&)> install_snapshot_;
		std::function<void(install_snapshot_result &&)> install_snapshot_result_;
		std::function<void(int64_t)> new_term_callback_;
		std::function<void(std::vector<int64_t>)> append_entries_success_callback_;
		config config_;
	private:
		void run()
		{
			do
			{
				if (!try_execute_cmd())
					sleep(0);
			} while (true);
		}
		void try_append_log()
		{
			do
			{
				try
				{
					int64_t index = get_last_log_index_();
					if (index == match_index_ && send_heartbeat_)
						break;
					auto request = build_append_entries_request_(next_index_);
					auto response = send_append_entries_request(request);
					update_heartbeat_time();
					if (!response.success_)
					{
						if (get_current_term_() < response.term_)
						{
							new_term_callback_(response.term_);
							return;
						}
						next_index_ -= 1;
					}
					std::vector<int64_t> indexs;
					indexs.reserve(request.entries_.size());
					for (auto &itr : request.entries_)
						indexs.push_back(itr.index_);
					append_entries_success_callback_(indexs);
					match_index_ = request.prev_log_index_ + request.entries_.size();
					next_index_ = match_index_ + 1;
					try_execute_cmd();
				}
				catch (...)
				{
					//todo process error;
					break;
				}
			} while (true);
		}
		append_entries_response send_append_entries_request(
				const append_entries_request &req,int timeout = 10000)
		{

		}
		int64_t next_heartbeat_delay()
		{
			auto delay = high_resolution_clock::now() - last_heart_beat_;
			return duration_cast<milliseconds>(delay).count();
		}
		bool try_execute_cmd()
		{
			cmd_t cmd;
			if (!cmd_queue_.pop(cmd))
				return false;
			switch (cmd)
			{
			case xraft::detail::raft_peer::e_connect:
				break;
			case xraft::detail::raft_peer::e_sleep:
				break;
			default:
				break;
			}
			return true;
		}
		void sleep(int milliseconds = 0)
		{
			std::unique_lock<std::mutex> lock(mtx_);
			is_sleep_ = true;
			if(!milliseconds)
				cv_.wait(lock);
			else {
				cv_.wait_for(lock, std::chrono::milliseconds(milliseconds));
			}
			is_sleep_ = false;
		}
		
		void update_heartbeat_time()
		{
			last_heart_beat_ = high_resolution_clock::now();
			send_heartbeat_ = true;
		}
		high_resolution_clock::time_point last_heart_beat_;
		std::thread peer_thread_;

		bool is_sleep_ = false;
		std::mutex mtx_;
		std::condition_variable cv_;

		utils::lock_queue<cmd_t> cmd_queue_;

		//ratf info
		int64_t match_index_ = 0;
		int64_t next_index_ = 0;

		bool send_heartbeat_ = false;
	};
}
}