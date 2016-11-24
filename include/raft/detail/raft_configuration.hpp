#pragma once
namespace xraft
{
	namespace detail
	{
		struct raft_config
		{
			struct raft_node
			{

			};
			std::vector<raft_node> nodes_;
		};
		class raft_config_mgr
		{
		public:
			raft_config_mgr()
			{

			}
			int get_majority()
			{
				(current_config_.nodes_.size()) / 2 + 1;
			}
		private:
			raft_config current_config_;
		};
	}
}