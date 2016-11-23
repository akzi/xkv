#pragma once
#include <string>
#include <fstream>
namespace xkv
{
	namespace detail
	{
		class file
		{
		public:
			file(const std::string &&filepath)
			{
				int mode =
					std::ios::out |
					std::ios::in |
					std::ios::app |
					std::ios::ate;
				file_.open(filepath.c_str(), mode);
			}
			int64_t write(std::string &&data)
			{
				if (!file_)
					return -1;
				auto curr = file_.tellp();
				uint32_t len = (uint32_t)data.size();
				file_.write((char*)(&len), sizeof len);
				file_.write(data.data(), data.size());
				if (!file_.good())
					return -1;
				file_.sync();
				return curr;
			}

			bool read(int64_t offset, std::string &buffer)
			{
				if (!seek(offset))
					return false;
				if (!file_.good())
					return false;
				uint32_t len{ 0 };
				file_.read((char*)&len, sizeof(len));
				if (!file_.good())
					return false;
				buffer.resize(len);
				file_.read(const_cast<char*>(buffer.data()), len);
				if (!file_.good())
					return false;
				return true;
			}
			int64_t size()
			{
				return file_.tellg();
			}
		private:
			bool seek(int64_t offset)
			{
				if (!file_.good())
					return false;
				file_.seekg(offset, std::ios::beg);
				if (file_.good())
					return true;
				return false;
			}
			std::fstream file_;
		};
	}
	
}