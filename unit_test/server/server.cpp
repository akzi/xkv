#include "../../include/raft/raft.hpp"
class server
{
public:
	using raft_config = xraft::raft::raft_config;

	server()
		:rpc_server_(rpc_proactor_pool_)
	{

	}
	void init(raft_config cfg, const std::string &ip, int port)
	{
		config_ = cfg;
		ip_ = ip;
		port_ = port;
		init_raft();
		init_rpc();
	}
private:
	struct sync_t
	{
		std::mutex mtx_;
		std::condition_variable cv_;
	};
	void init_raft()
	{
		raft_.regist_commit_entry_callback([this](std::string &&log, int64_t) {
			using namespace xsimple_rpc::detail;
			uint8_t *ptr = (uint8_t *)log.data();
			auto end = ptr + log.size();

			std::string cmd = endec::get<std::string>(ptr, end);
			if(cmd == "set")
			{
				std::string key = endec::get<std::string>(ptr, end);
				std::string value = endec::get<std::string>(ptr, end);
				metadata_.set(key, value);
			}
			else if(cmd == "del")
			{
				std::string key = endec::get<std::string>(ptr, end);
				metadata_.del(key);
			}
		});
		raft_.regist_install_snapshot_handle(
			[](std::ifstream &file) {
		
		});
		raft_.regist_build_snapshot_callback(
			[](const std::function<bool(const std::string &)>& writer, int64_t index) {

			return true;
		});
		raft_.init(config_);
	}
	void init_rpc()
	{
		rpc_proactor_pool_.start();
		rpc_server_.regist("set", 
			[this](const std::string &key, const std::string &value)
				->std::pair<bool, std::string>
		{
			using namespace xsimple_rpc::detail;
			std::string cmd("set");
			std::string log;
			log.resize(endec::get_sizeof(cmd)+ 
				endec::get_sizeof(key) + 
				endec::get_sizeof(value));
			uint8_t *ptr = (uint8_t *)log.data();
			endec::put(ptr, cmd);
			endec::put(ptr, key);
			endec::put(ptr, value);

			bool result = false;
			auto sync = get_sync_item();
			raft_.replicate(std::move(log), 
				[this, &result, &sync](bool value, int64_t index){
					std::unique_lock<std::mutex> locker_(sync->mtx_);
					std::cout << index << std::endl;
					result = value;
					sync->cv_.notify_one();
			});
			std::unique_lock<std::mutex> locker(sync->mtx_);
			sync->cv_.wait(locker);
			if (result)
			{
				metadata_.set(key, value);
				return{ true,{} };
			}
			return {false, "raft error" };
		});
		rpc_server_.regist("get", [this](const std::string &key) ->std::pair<bool, std::string>
		{
			if (!raft_.check_leader())
				return{ false, "no leader" };
			std::string value;
			if (metadata_.get(key, value))
				return {true, value};
			return{ false, "noexist" };
		});
		rpc_server_.regist("del", 
			[this](const std::string &key) ->std::pair<bool, std::string> 
		{
			using namespace xsimple_rpc::detail;
			std::string cmd("set");
			std::string log;
			log.resize(endec::get_sizeof(cmd) +
				endec::get_sizeof(key));
			uint8_t *ptr = (uint8_t *)log.data();
			endec::put(ptr, cmd);
			endec::put(ptr, key);

			bool result = false;
			auto sync = get_sync_item();
			raft_.replicate(std::move(log), 
				[this, &result, &sync](bool value, int64_t index){
				std::unique_lock<std::mutex> locker_(sync->mtx_);
				std::cout << index << std::endl;
				result = value;
				sync->cv_.notify_one();
			});
			std::unique_lock<std::mutex> locker(sync->mtx_);
			sync->cv_.wait(locker);
			if (result)
			{
				if (metadata_.del(key))
					return{ true,{} };
				return{ false, "noexist" };
			}
			return{ false, "raft error" };
		});
		rpc_server_.bind(ip_, port_);
	}
	void push_back_to_cache(sync_t *item) 
	{
		std::lock_guard<std::mutex> locker(mtx_);
		sync_cache_.emplace_back(std::shared_ptr<sync_t>(item, 
			[this](sync_t *_item) {	
			push_back_to_cache(_item); 
		}));
	};
	std::shared_ptr<sync_t> get_sync_item()
	{
		std::lock_guard<std::mutex> locker(mtx_);
		if (sync_cache_.empty())
			return  std::shared_ptr<sync_t>(new sync_t, [this](sync_t *_item) { 
			push_back_to_cache(_item); 
		});
		auto sync = sync_cache_.front();
		sync_cache_.pop_front();
		return std::move(sync);
	}
	raft_config config_;
	std::string ip_;
	int port_;
	xraft::raft raft_;
	xraft::detail::metadata<> metadata_;
	xsimple_rpc::rpc_proactor_pool rpc_proactor_pool_;
	xsimple_rpc::rpc_server rpc_server_;
	std::mutex mtx_;
	std::list<std::shared_ptr<sync_t>>  sync_cache_;
};

int main(int args, char **argc)
{
	if (args == 1 || argc[1] == std::string("9001"))
	{
		server::raft_config config;
		config.append_log_timeout_ = 10000;
		config.election_timeout_ = 3000;
		config.metadata_base_path_ = "9001/data/metadata/";
		config.raftlog_base_path_ = "9001/data/log/";
		config.snapshot_base_path_ = "9001/data/snapshot/";
		config.myself_ = { "127.0.0.1",9011,"9011" };
		config.nodes_ = { { "127.0.0.1", 9012, "9012" }/*,{ "127.0.0.1", 9003, "9003" } */ };

		server _server;
		_server.init(config, "127.0.0.1", 9001);
		getchar();
	}
	else if (argc[1] == std::string("9002"))
	{
		server::raft_config config;
		config.append_log_timeout_ = 10000;
		config.election_timeout_ = 3000;
		config.metadata_base_path_ = "9002/data/metadata/";
		config.raftlog_base_path_ = "9002/data/log/";
		config.snapshot_base_path_ = "9002/data/snapshot/";
		config.myself_ = { "127.0.0.1",9012,"9012" };
		config.nodes_ = { { "127.0.0.1", 9011, "9011" }/*,{ "127.0.0.1", 9003, "9003" } */ };

		server _server;
		_server.init(config, "127.0.0.1", 9002);
		getchar();
	}

}