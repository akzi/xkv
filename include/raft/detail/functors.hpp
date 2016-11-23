#pragma once
namespace xraft
{
	namespace functors
	{
		namespace fs
		{
			struct mkdir
			{
				bool operator()(const std::string &dir)
				{
					return false;
				}
			};

			struct ls
			{
				std::vector<std::string>
					operator()(const std::string &dir)
				{
					//todo impl
					return{};
				}
			};
		}
		
	}
}