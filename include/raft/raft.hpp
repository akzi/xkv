#pragma once
#include "detail/detail.hpp"
#define TRACE std::cout <<__FUNCTION__ <<std::endl;
namespace xraft
{
	using namespace detail;
	class raft
	{
	public:
		using raft_config = detail::raft_config;
		using append_log_callback = std::function<void(bool, int64_t)>;
		using commit_entry_callback = std::function<void(std::string &&, int64_t)>;
		using install_snapshot_callback = std::function<void(std::ifstream &)>;
		using build_snapshot_callback = std::function<bool(const std::function<bool(const std::string &)>&, int64_t)>;
		enum state
		{
			e_follower,
			e_candidate,
			e_leader
		};
		raft()
			:rpc_server_(rpc_proactor_pool_)
		{
		}
		~raft()
		{

		}
		bool check_leader()
		{
			return state_ == state::e_leader;
		}
		void replicate(std::string &&data, append_log_callback&&callback)
		{
			do_relicate(std::move(data), std::move(callback));
		}
		void regist_commit_entry_callback(const commit_entry_callback &callback)
		{
			commit_entry_callback_ = callback;
		}
		void regist_build_snapshot_callback(const  build_snapshot_callback& callback)
		{
			build_snapshot_callback_ = callback;
		}
		void regist_install_snapshot_handle(const install_snapshot_callback &callback)
		{
			install_snapshot_callback_ = callback;
		}
		void init(raft_config config)
		{
			state_ = e_follower;
			init_config(config);
			init_raft_log();
			load_metadata();
			init_rpc();
			init_snapshot_builder();
 			init_pees();
			init_timer();
 			set_election_timer();
		}
	private:
		void init_raft_log()
		{
			if (!log_.init(filelog_base_path_))
			{
				std::cout << "raft log init failed" << std::endl;
				throw std::runtime_error("raft log init failed");
			}
		}
		void load_metadata()
		{
			int64_t current_term;
			int64_t committed_index;
			int64_t last_snapshot_term;
			int64_t last_snapshot_index;
			if (!metadata_.init(metadata_base_path_))
			{
				std::cout << "init metadata failed" << std::endl;
				std::exit(0);
			}
			metadata_.get("last_applied_index", last_applied_index_);
			metadata_.get("voted_for", voted_for_);
			if (metadata_.get("current_term", current_term))
				current_term_ = current_term;
			if (metadata_.get("committed_index", committed_index))
				committed_index_ = committed_index;
			if (metadata_.get("last_snapshot_term", last_snapshot_term))
				last_snapshot_term_ = last_snapshot_term;
			if (metadata_.get("last_snapshot_index", last_snapshot_index))
				last_snapshot_index_ = last_snapshot_index;

		}
		void init_timer()
		{
			timer_.start();
		}
		void init_config(raft_config config)
		{
			raft_config_mgr_.set(config.peers_);
			raft_config_mgr_.get_nodes().emplace_back(config.myself_);
			myself_ = config.myself_;
			filelog_base_path_ = config.raftlog_base_path_;
			metadata_base_path_ = config.metadata_base_path_;
			snapshot_base_path_ = config.snapshot_base_path_;
			append_log_timeout_ = config.append_log_timeout_;
			election_timeout_ = config.election_timeout_;
		}
		void init_snapshot_builder()
		{
			snapshot_builder_.regist_build_snapshot_callback(
				std::bind(&raft::make_snapshot_callback, this,
					std::placeholders::_1, std::placeholders::_2));
			snapshot_builder_.regist_get_last_commit_index(
				std::bind(&raft::get_last_log_entry_index, this));
			snapshot_builder_.regist_get_log_entry_term_handle(
				std::bind(&raft::get_last_log_entry_term, this));
			snapshot_builder_.regist_get_log_start_index(
				std::bind(&raft::get_log_start_index, this));
			snapshot_builder_.set_snapshot_distance(1024);//for test

			snapshot_builder_.start();
		}
		void init_rpc()
		{
			rpc_server_.regist("append_entries_request", [this]( append_entries_request &req) { 
				return handle_append_entries_request(req); 
			}).regist("vote_request", [this](const vote_request &req) { 
				return handle_vote_request(req); 
			}).regist("install_snapshot_request", [this](install_snapshot_request &req) { 
				return handle_install_snapshot(req); 
			}).bind(myself_.ip_, myself_.port_);

			rpc_proactor_pool_.start();
		}
		void init_pees()
		{
			using namespace std::placeholders;
			for (auto &itr: raft_config_mgr_.get_nodes())
			{
				if (itr.raft_id_ == myself_.raft_id_)
					continue;
				pees_.emplace_back(new raft_peer(rpc_proactor_pool_, itr));
				raft_peer &peer = *(pees_.back());
				peer.append_entries_success_callback_ = std::bind(&raft::append_entries_callback, this, _1);
				peer.build_append_entries_request_ = std::bind(&raft::build_append_entries_request, this, _1);
				peer.build_vote_request_ = std::bind(&raft::build_vote_request, this);
				peer.vote_response_callback_ = std::bind(&raft::handle_vote_response, this, _1);
				peer.new_term_callback_ = std::bind(&raft::handle_new_term, this, _1);
				peer.get_current_term_ = [this] { return current_term_.load(); };
				peer.get_last_log_index_ = std::bind(&raft::get_last_log_entry_index, this);
				peer.connect_callback_ = std::bind(&raft::peer_connect_callback, this, _1, _2);
				peer.get_snapshot_path_ = std::bind(&raft::get_snapshot_filepath, this);
				peer.raft_id_ = myself_.raft_id_;
				peer.start();
				peer.send_cmd(raft_peer::cmd_t::e_connect);
			}
		}
		void peer_connect_callback(raft_peer &peer, bool result)
		{
			auto result_str = result ? "connect success" : "connect failed";
			std::cout <<
				"IP:"<< 
				peer.myself_.ip_ << 
				" PORT:" << 
				peer.myself_.port_ << 
				"ID:" << 
				peer.myself_.raft_id_<< 
				result_str << 
				std::endl;
		}
		void do_relicate(std::string &&data, append_log_callback&&callback)
		{
			int64_t index;
			if (!log_.write(build_log_entry(std::move(data)), index))
			{
				append_log_callback handle;
				commiter_.push([handle = std::move(callback)] {
					handle(false, 0);
				});
				return;
			}
			insert_callback(index, set_timeout(index), std::move(callback));
			notify_peers();
		}
		append_entries_response 
			handle_append_entries_request(append_entries_request & request)
		{
			TRACE;
			std::lock_guard<std::mutex> locker(mtx_);
			append_entries_response response;
			response.success_ = false;
			response.term_ = current_term_;
			if (request.term_ < current_term_)
				//todo log Warm
				return response;
			
			if (current_term_ < request.term_)
			{
				step_down(request.term_);
				response.term_ = current_term_;
			}

			if (leader_id_.empty())
				leader_id_ = request.leader_id_;
			else if (leader_id_ != request.leader_id_)
			{
				leader_id_ = request.leader_id_;
				//todo log Warm
			}
			xnet::guard guard([this] { set_election_timer(); });
			if (request.prev_log_term_ == get_last_log_entry_term() &&
				request.prev_log_index_ > get_last_log_entry_index())
				return response;

			if (request.prev_log_index_ > get_log_start_index() &&
				get_log_entry(request.prev_log_index_).term_ != request.prev_log_term_)
			{
				//todo log Warm
				return response;
			}
			response.success_ = true;
			auto check_log = true;
			for (auto &itr : request.entries_)
			{
				if (check_log)
				{
					if (itr.index_ < get_log_start_index())
						continue;
					if (itr.index_ <= get_last_log_entry_index())
					{
						if (get_log_entry(itr.index_).term_ == itr.term_)
							continue;
						assert(committed_index_ < itr.index_);
						log_.truncate_suffix(itr.index_);
						check_log = false;
					}
				}
				int64_t index;
				log_.write(std::move(itr), index);
			}
			response.last_log_index_ = get_last_log_entry_index();
			if (committed_index_ < request.leader_commit_)
			{
				auto leader_commit_ = request.leader_commit_;
				commiter_.push([leader_commit_,this] {
					auto entries = log_.get_log_entries(committed_index_ + 1, 
						leader_commit_ - committed_index_);
					for (auto &itr : entries)
					{
						assert(itr.index_ == committed_index_ + 1);
						commit_entry_callback_(std::move(itr.log_data_),itr.index_);
						set_committed_index(committed_index_+1);
					}
				});
			}
			return response;
		}
		vote_response handle_vote_request(const vote_request &request)
		{
			TRACE;
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

			if (request.term_ == current_term_) 
			{
				if (is_ok && voted_for_.empty()) 
				{
					step_down(current_term_);
					set_voted_for(request.candidate_);
				}
			}
			response.term_ = current_term_;
			response.vote_granted_ = (request.term_ == current_term_ && 
					voted_for_ == request.candidate_);
			response.log_ok_ = is_ok;
			return response;
		}
		install_snapshot_response
			handle_install_snapshot (install_snapshot_request &request)
		{
			install_snapshot_response response;
			response.term_ = current_term_;
			if (request.term_ < current_term_)
			{
				//todo LOG WARM
				return response;
			}

			if (request.term_ > current_term_)
			{
				//todo log Info
				response.term_ = request.term_;
			}
			step_down(request.term_);
			set_election_timer();
			if (leader_id_.empty())
			{
				leader_id_ = request.leader_id_;
				//todo log info
			}
			else if (leader_id_ != request.leader_id_)
			{
				//logo error;
			}

			if (!snapshot_writer_)
				open_snapshot_writer(request.last_snapshot_index_);
			response.bytes_stored_ = snapshot_writer_.get_bytes_writted();
			if (request.offset_ != snapshot_writer_.get_bytes_writted())
			{
				//todo LOG WRAM
				return response;
			}
			if (!snapshot_writer_.write(request.data_))
			{
				//todo LOG ERROR;
				return response;
			}
			response.bytes_stored_ = snapshot_writer_.get_bytes_writted();
			if (request.done_)
			{
				if (request.last_snapshot_index_ < last_snapshot_index_)
				{
					//todo Warm
					snapshot_writer_.discard();
					return response;
				}
				load_snapshot();
			}
			return response;
		}
		void load_snapshot()
		{
			snapshot_writer_.close();
			if (!snapshot_reader_.open(snapshot_writer_.get_snapshot_filepath()))
			{
				//todo process error;
			}
			commiter_.push([&] {
				snapshot_head head;
				if (!snapshot_reader_.read_sanpshot_head(head))
				{
					//todo process error;
				}
				install_snapshot_callback_(snapshot_reader_.get_snapshot_stream());;
				log_.truncate_suffix(head.last_included_index_);

				if (head.last_included_index_ > committed_index_)
					set_committed_index(head.last_included_index_);
			});
			
		}
		void open_snapshot_writer(int64_t index)
		{
			if(functors::fs::mkdir()(snapshot_path_))
				snapshot_writer_.open(snapshot_path_+std::to_string(index)+".SS");
			else
			{
				//todo process Error;
			}
		}

		void handle_new_term(int64_t new_term)
		{
			TRACE;
			step_down(new_term);
		}
		void step_down(int64_t new_term)
		{
			TRACE;
			std::cout << new_term << std::endl;
			if (current_term_ < new_term)
			{
				current_term_ = new_term;
				leader_id_.clear();
				set_voted_for("");
				update_Log_metadata();
				if(snapshot_writer_)
					snapshot_writer_.discard();
			}
			if (state_ == state::e_candidate)
			{
				vote_responses_.clear();
				cancel_election_timer();
			}
			if (state_ == state::e_leader)
			{
				sleep_peer_threads();
			}
			state_ = state::e_follower;
			set_election_timer();
		}
		void set_election_timer()
		{
			TRACE;
			cancel_election_timer();
			election_timer_id_ = timer_.set_timer(election_timeout_ ,[this] {
				std::cout << "------election timer callback------" << std::endl;
				std::lock_guard<std::mutex> lock(mtx_);
				set_term(current_term_ + 1);
				state_ = state::e_candidate;
				for (auto &itr : pees_)
					itr->send_cmd(raft_peer::cmd_t::e_election);
				set_election_timer();
			});
		}
		void sleep_peer_threads()
		{
			TRACE;
			std::cout << "-----------------------" << std::endl;
			for (auto &itr : pees_)
				itr->send_cmd(raft_peer::cmd_t::e_sleep);
		}
		void update_Log_metadata()
		{
			metadata_.set("leader_id", leader_id_);
		}
		struct append_log_callback_info
		{
			append_log_callback_info(int waits, int64_t index, int64_t timer_id,
				append_log_callback && callback)
					:waits_(waits),
					timer_id_(timer_id),
					callback_(callback),
					index_(index)
			{
			}
			int64_t index_;
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
				std::forward_as_tuple(raft_config_mgr_.get_majority() - 1, index,  timer_id, std::move(callback)));
		}
		int64_t set_timeout(int64_t index)
		{
			return timer_.set_timer(append_log_timeout_, [this, index] {
				utils::lock_guard lock(mtx_);
				auto itr = append_log_callbacks_.find(index);
				if (itr == append_log_callbacks_.end())
					return;
				append_log_callback func;
				commiter_.push([func = std::move(itr->second.callback_)]{ func(false, 0); });
				append_log_callbacks_.erase(itr);
			});
		}
		log_entry build_log_entry(std::string &&log, 
			log_entry::type type = log_entry::type::e_append_log)
		{
			log_entry entry;
			entry.term_ = current_term_;
			entry.log_data_ = std::move(log);
			entry.type_ = type;
			return std::move(entry);
		}
		append_entries_request build_append_entries_request(int64_t index)
		{
			TRACE;
			append_entries_request request;
			request.term_ = current_term_;
			request.entries_ = log_.get_log_entries(index > 1 ? index -1: index);
			request.leader_commit_ = committed_index_;
			request.leader_id_ = myself_.raft_id_;
			if (request.entries_.size() > 1 && index > 1)
			{
				request.prev_log_index_ = request.entries_.front().index_;
				request.prev_log_term_ = request.entries_.front().term_;
				request.entries_.pop_front();
			}
			else 
			{
				request.prev_log_index_ = 0;
				request.prev_log_term_ = 0;
			}
			
			return std::move(request);
		}
		void handle_vote_response(const vote_response &response)
		{
			TRACE;
			if (state_ != e_candidate)
			{
				return;
			}
			if (response.term_ < current_term_)
			{
				return;
			}
			if (response.term_ > current_term_)
				step_down(response.term_);
			vote_responses_.push_back(response);
			int votes = 1;//mysql 
			for (auto &itr : vote_responses_)
				if (itr.vote_granted_) votes++;
			if (votes >= raft_config_mgr_.get_majority())
			{
				vote_responses_.clear();
				become_leader();
			}
		}
		void become_leader()
		{
			TRACE;
			state_ = e_leader;
			cancel_election_timer();
			for (auto &itr : pees_)
				itr->send_cmd(raft_peer::cmd_t::e_append_entries);
		}
		void cancel_election_timer()
		{
			TRACE;
			timer_.cancel(election_timer_id_);
		}
		vote_request build_vote_request()
		{
			TRACE;
			vote_request request;
			request.candidate_ = myself_.raft_id_;
			request.term_ = current_term_;
			request.last_log_index_ = get_last_log_entry_index();
			request.last_log_term_ = get_last_log_entry_term();
			return request;
		}
		std::string get_snapshot_filepath()
		{
			auto files = functors::fs::ls_files()(snapshot_path_);
			if (files.empty())
				return	{};
			std::sort(files.begin(), files.end(),std::greater<std::string>());
			return files[0];
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
					set_committed_index(item->first);
					append_log_callback func;
					int64_t index = committed_index_;
					commiter_.push([func = std::move(item->second.callback_),this, index]
					{ 
						func(true, index); 
						set_last_applied(index);
					});
					append_log_callbacks_.erase(item);
				}
			}
		}
		bool make_snapshot_callback(const std::function<bool(const std::string &)> &writer, int64_t index)
		{
			return build_snapshot_callback_(writer, index);
		}
		void make_snapshot_done_callback(int64_t index)
		{
			log_.truncate_prefix(index);
		}
		int64_t get_last_log_entry_term()
		{
			return log_.get_last_log_entry_term();
		}
		int64_t get_last_log_entry_index()
		{
			return log_.get_last_index();
		}
		int64_t get_log_start_index()
		{
			return log_.get_log_start_index();
		}
		log_entry get_log_entry(int64_t index)
		{
			log_entry entry;
			if (!log_.get_log_entry(index, entry))
			{
				//todo log error;
			}
			return std::move(entry);
		}
		void set_voted_for(const std::string &raft_id)
		{
			TRACE;
			voted_for_ = raft_id;
			if (!metadata_.set("voted_for", voted_for_))
			{
				//todo log error;
				throw std::runtime_error("metadata::set [voted_for] failed");
			}
		}
		void set_committed_index(int64_t index)
		{
			committed_index_ = index;
			if (!metadata_.set("committed_index", committed_index_.load()))
			{
				//todo log error;
				throw std::runtime_error("metadata::set [committed_index] failed");
			}
		}
		void set_last_applied(int64_t index)
		{
			last_applied_index_ = index;
			if (!metadata_.set("last_applied_index", index))
			{
				//todo log error;
				throw std::runtime_error("metadata::set [committed_index] failed");
			}
		}
		void set_term(int64_t term)
		{
			TRACE;
			std::cout << "term:" << term << std::endl;
			current_term_ = term;
			if (!metadata_.set("current_term", term))
			{
				//todo log error;
				throw std::runtime_error("metadata::set [current_term] failed");
			}
		}
		void set_last_snapshot_index(int64_t index)
		{
			last_snapshot_index_ = index;
			if (metadata_.set("last_snapshot_index", index))
			{
				//todo log error;
				throw std::runtime_error("metadata::set [last_snapshot_index] failed");
			}
		}
		void set_last_snapshot_term(int64_t term)
		{
			last_snapshot_term_ = term;
			if (metadata_.set("last_snapshot_term", term))
			{
				//todo log error;
				throw std::runtime_error("metadata::set [last_snapshot_term] failed");
			}
		}
		detail::raft_config::raft_node myself_;
		//rpc
		xsimple_rpc::rpc_proactor_pool rpc_proactor_pool_;
		xsimple_rpc::rpc_server rpc_server_;
		//raft
		std::atomic_int64_t last_snapshot_index_ = 0;
		std::atomic_int64_t last_snapshot_term_ = 0;
		std::atomic_int64_t current_term_ = 0;
		std::atomic_int64_t committed_index_ = 0;
		int64_t last_applied_index_ = 0;

		std::string voted_for_;
		std::string leader_id_;
		int64_t election_timeout_ = 10000;
		int64_t election_timer_id_ = 0;

		commit_entry_callback commit_entry_callback_;

		snapshot_builder snapshot_builder_;
		snapshot_writer snapshot_writer_;
		snapshot_reader snapshot_reader_;
		build_snapshot_callback build_snapshot_callback_;
		install_snapshot_callback install_snapshot_callback_;

		raft_config_mgr raft_config_mgr_;
		int64_t append_log_timeout_ = 10000;//10 seconds;
		std::vector<std::unique_ptr<raft_peer>> pees_;
		
		state state_;
		std::mutex mtx_;
		std::map<int64_t, append_log_callback_info> append_log_callbacks_;
		detail::timer timer_;
		committer<> commiter_;

		metadata<> metadata_;
		std::string metadata_base_path_;
		std::string metadata_path_ = "data/metadata";

		detail::filelog log_;
		std::string filelog_path_ = "data/log";
		std::string filelog_base_path_;

		std::string current_snapshot_;
		std::string snapshot_base_path_;
		std::string snapshot_path_ = "data/snapshot/";

		std::vector<vote_response>  vote_responses_;
	};
}