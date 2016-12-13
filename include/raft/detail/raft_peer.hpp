#pragma once
namespace xraft
{
namespace detail
{
	using namespace std::chrono;
	class raft_peer
	{
		
	public:
		enum class cmd_t
		{
			e_connect,
			e_election,
			e_append_entries,
			e_interrupt_vote,
			e_sleep,
			e_exit,

		};
		raft_peer(xsimple_rpc::rpc_proactor_pool &pool)
			:rpc_proactor_pool_(pool),
			peer_thread_([this] { run(); })
		{

		}
		void send_cmd(cmd_t cmd)
		{
			cmd_queue_.push(std::move(cmd));
			notify();
		}
		void notify()
		{
			utils::lock_guard locker(mtx_);
			cv_.notify_one();
		}
		std::function<void(raft_peer&, bool)> connect_callback_;
		std::function<int64_t(void)> get_current_term_;
		std::function<int64_t(void)> get_last_log_index_;
		std::function<append_entries_request(int64_t)> build_append_entries_request_;
		std::function<vote_request()> build_vote_request_;
		std::function<void(const vote_response &)> vote_response_callback_;
		std::function<void(int64_t)> new_term_callback_;
		std::function<void(const std::vector<int64_t>&)> append_entries_success_callback_;
		raft_config::raft_node myself_;
		std::int64_t heatbeat_inteval_;
	private:
		void run()
		{
			do
			{
				if (!try_execute_cmd())
				{
					if (!send_heartbeat_ )
						do_sleep(next_heartbeat_delay());
					else
						do_sleep(0);
				}
			} while (!stop_);
		}
		void do_append_entries()
		{
			next_index_ = 0;
			match_index_ = 0;
			send_heartbeat_ = false;
			do
			{
				try
				{
					if (try_execute_cmd())
						break;
					int64_t index = get_last_log_index_();
					if (index == match_index_ && send_heartbeat_)
					{
						send_heartbeat_ = false;
						break;
					}
					if (!next_index_)
						next_index_ = index;
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
						--next_index_;
						continue;
					}
					std::vector<int64_t> indexs;
					indexs.reserve(request.entries_.size());
					for (auto &itr : request.entries_)
						indexs.push_back(itr.index_);
					append_entries_success_callback_(indexs);
					match_index_ = response.last_log_index_;
					next_index_ = match_index_ + 1;
				}
				catch (std::exception &e)
				{
					std::cout << e.what() << std::endl;
					break;
				}
			} while (true);
		}
		append_entries_response 
			send_append_entries_request(const append_entries_request &req ,int timeout = 10000)
		{
			if (!check_rpc())
				throw std::runtime_error("rpc is connected");
			try
			{
				DEFINE_RPC_PROTO(append_entries_request_rpc, append_entries_response(append_entries_request));
				return rpc_client_->rpc_call<append_entries_request_rpc>(req);
			}
			catch (const std::exception& e)
			{
				std::cout << e.what() << std::endl;
			}
		}
		int64_t next_heartbeat_delay()
		{
			if (rpc_client_)
			{
				auto delay = high_resolution_clock::now() - last_heart_beat_;
				return std::abs(heatbeat_inteval_ - duration_cast<milliseconds>(delay).count());
			}
			return 0;
		}
		bool try_execute_cmd()
		{
			if (!cmd_queue_.pop(cmd_))
				return false;
			switch (cmd_)
			{
			case cmd_t::e_connect:
				do_connect();
				break;
			case cmd_t::e_sleep:
				do_sleep();
				break;
			case cmd_t::e_election:
				do_election();
				break;
			case cmd_t::e_append_entries:
				do_append_entries();
				break;
			case cmd_t::e_exit:
				do_exist();
			default:
				//todo log error
				break;
			}
			return true;
		}
		void do_sleep(int64_t milliseconds = 0)
		{
			std::unique_lock<std::mutex> lock(mtx_);
			if(!milliseconds)
				cv_.wait(lock);
			else {
				cv_.wait_for(lock, std::chrono::milliseconds(milliseconds));
			}
		}
		
		void update_heartbeat_time()
		{
			last_heart_beat_ = high_resolution_clock::now();
			send_heartbeat_ = true;
		}
		void do_connect()
		{
			try
			{
				rpc_client_.reset(new xsimple_rpc::client(
					rpc_proactor_pool_.connect(myself_.ip_, myself_.port_, 0)));
				connect_callback_(*this, true);
			}
			catch (const std::exception &e)
			{
				std::cout << e.what() << std::endl;
			}
		}
		bool check_rpc()
		{
			if (!rpc_client_)
				do_connect();
			return !!rpc_client_;
		}
		void do_election()
		{
			if (!check_rpc())
				return;
			auto req = build_vote_request_();
			try
			{
				struct RPC
				{ 
					DEFINE_RPC_PROTO(vote_request, detail::vote_response(detail::vote_request));
				};
				auto resp = rpc_client_->rpc_call<RPC::vote_request>(req);
				vote_response_callback_(resp);
			}
			catch (const std::exception& e)
			{
				std::cout << e.what() << std::endl;
			}
		}

		void do_exist()
		{
			stop_ = true;
		}
		xsimple_rpc::rpc_proactor_pool &rpc_proactor_pool_;
		std::unique_ptr<xsimple_rpc::client> rpc_client_;
		high_resolution_clock::time_point last_heart_beat_;
		bool stop_ = false;
		std::mutex mtx_;
		std::condition_variable cv_;

		utils::lock_queue<cmd_t> cmd_queue_;

		//ratf info
		int64_t match_index_ = 0;
		int64_t next_index_ = 0;

		bool send_heartbeat_ = false;
		cmd_t cmd_;
		std::thread peer_thread_;
	};
}
}