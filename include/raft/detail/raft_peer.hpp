#pragma once
namespace xraft
{
namespace detail
{
	class raft_peer
	{
	public:
		using log_entrys = std::vector<log_entry>;
		using get_last_log_index_handle = std::function<int64_t(void)>;
		using get_log_entries_handle = std::function<log_entrys(int64_t)>;
		using append_entries_request_callback = std::function<void(append_entries_request &&)>;
		using append_entries_response_callback = std::function<void(append_entries_response &&)>;
		using vote_request_callback = std::function<void(vote_request &&)>;
		using vote_response_callback = std::function<void(vote_response &&)>;
		using install_snapshot_callback = std::function<void(install_snapshot &&)>;
		using install_snapshot_result_callback = std::function<void(install_snapshot_result &&)>;
		raft_peer()
		{

		}

		raft_peer &regist_handle(get_last_log_index_handle && handle)
		{
			get_last_log_index_ = handle;
			return *this;
		}
		raft_peer &regist_handle(get_log_entries_handle && handle)
		{
			get_log_entries_ = handle;
			return *this;
		}
		raft_peer &regist_callback(append_entries_request_callback && handle)
		{
			append_entries_request_ = handle;
			return *this;
		}
		raft_peer &regist_callback(append_entries_response_callback && handle)
		{
			append_entries_response_ = handle;
			return *this;
		}
		raft_peer &regist_callback(vote_request_callback && handle)
		{
			vote_request_ = handle;
			return *this;
		}
		raft_peer &regist_callback(vote_response_callback && handle)
		{
			vote_response_ = handle;
			return *this;
		}
		raft_peer &regist_callback(install_snapshot_callback && handle)
		{
			install_snapshot_ = handle;
			return *this;
		}
		raft_peer &regist_callback(install_snapshot_result_callback && handle)
		{
			install_snapshot_result_ = handle;
			return *this;
		}
	private:
		get_last_log_index_handle get_last_log_index_;
		get_log_entries_handle get_log_entries_;
		append_entries_request_callback append_entries_request_;
		append_entries_response_callback append_entries_response_;
		vote_request_callback vote_request_;
		vote_response_callback vote_response_;
		install_snapshot_callback install_snapshot_;
		install_snapshot_result_callback install_snapshot_result_;
	};
}
}