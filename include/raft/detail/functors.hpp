#pragma once
namespace xraft
{
namespace functors
{
namespace fs
{
#ifdef _MSC_VER
	struct mkdir
	{
		bool operator()(const std::string &dir)
		{
			return CreateDirectory(dir.c_str(), NULL);
		}
	};
	struct ls
	{
		std::vector<std::string> operator()(const std::string &dir)
		{
			std::vector<std::string> files;
			WIN32_FIND_DATA find_data;
			HANDLE handle = ::FindFirstFile((dir+"*.*").c_str(), &find_data);
			if (INVALID_HANDLE_VALUE == handle)
				return {};
			while (TRUE)
			{
				if (find_data.dwFileAttributes & FILE_ATTRIBUTE_NORMAL)
				{
					files.emplace_back(std::string(dir) + find_data.cFileName);
				}
				if (!FindNextFile(handle, &find_data)) 
					break;
			}
			FindClose(handle);
			return files;
		}
	};

	struct rm 
	{
		bool operator()(const std::string &filepath)
		{
			return DeleteFile(filepath.c_str());
		}
	};
#endif
}
		
}
}