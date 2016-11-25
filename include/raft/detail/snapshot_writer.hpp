#pragma once
namespace xraft
{
	namespace detail
	{
		class snapshot_writer
		{
		public:
			snapshot_writer() { }

			operator bool()
			{
				return file_.good();
			}
			bool open(std::string filepath)
			{
				filepath_ = filepath;
				assert(!file_.is_open());
				int mode =
					std::ios::out |
					std::ios::in |
					std::ios::trunc|
					std::ios::ate;
				file_.open(filepath_.c_str(), mode);
				return file_.good();
			}
			bool write(const std::string &buffer)
			{
				file_.write(buffer.data(), buffer.size());
				file_.sync();
				return file_.good();
			}
			std::size_t get_bytes_writted()
			{
				return file_.tellp();
			}
		private:
			std::string filepath_;
			std::fstream file_;
		};
	}
}