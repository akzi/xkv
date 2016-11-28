#pragma once
namespace xraft
{
	namespace detail
	{
		class raft_config_mgr
		{
		public:
			raft_config_mgr()
			{

			}
			int get_majority()
			{
				return (int)current_config_.nodes_.size() / 2 + 1;
			}
		private:
			raft_config current_config_;
		};
	}
}