#include "../include/raft/raft.hpp"
#include "../../xtest/include/xtest.hpp"
xtest_run;

XTEST_SUITE(raft)
{
	XUNIT_TEST(init)
	{
		xraft::raft::raft_config config;
		config.append_log_timeout_ = 10000;
		config.election_timeout_ = 10000;
		config.metadata_base_path_ = "/data/metadata/";
		config.raftlog_base_path_ = "/data/log/";
		config.snapshot_base_path_ = "/data/snapshot/";
		config.myself_ = {"127.0.0.1",9001,"9001"};
		config.nodes_ = { {"127.0.0.1", 9002, "9002"}/*,{ "127.0.0.1", 9003, "9003" } */};
		
		xraft::raft raft;
		raft.init(config);
		getchar();
	}
}