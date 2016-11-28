#pragma once
namespace xraft
{
	namespace detail
	{
		struct snapshot_head
		{
			uint32_t version = 1;
			static constexpr uint32_t magic_num = 'X'+'R'+'A'+'F'+'T';
			int64_t last_included_index_;
			int64_t last_included_term_;
		};
		class snapshot_reader
		{
		public:
			snapshot_reader()
			{

			}
			bool open(const std::string &filepath)
			{
				filepath_ = filepath;
				assert(!file_.is_open());
				int mode = std::ios::out | std::ios::binary;
				file_.open(filepath_.c_str(), mode);
				return file_.good();
			}
			bool read_sanpshot_head(snapshot_head &head)
			{
				std::string buffer;
				buffer.resize(sizeof(head));
				file_.read((char*)buffer.data(), buffer.size());
				if (!file_.good())
					return false;
				unsigned char *ptr = (unsigned char*)buffer.data();
				if (endec::get_uint32(ptr) != head.version || 
					endec::get_uint32(ptr) != head.magic_num)
					return false;
				head.last_included_index_ = (int64_t)endec::get_uint64(ptr);
				head.last_included_term_ = (int64_t)endec::get_uint64(ptr);
				return true;
			}
			std::ifstream get_snapshot_stream()
			{
				return std::move(file_);
			}
		private:
			std::string filepath_;
			std::ifstream file_;
		};
		class snapshot_writer
		{
		public:
			snapshot_writer() { }

			operator bool()
			{
				return file_.good();
			}
			bool open(const std::string &filepath)
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
			void close()
			{
				if(file_.is_open())
					file_.close();
			}
			bool write_sanpshot_head(const snapshot_head &head)
			{
				std::string buffer;
				buffer.resize(sizeof(head));
				unsigned char *ptr = (unsigned char*)buffer.data();
				endec::put_uint32(ptr, head.version);
				endec::put_uint32(ptr, head.magic_num);
				endec::put_uint64(ptr, (uint64_t)head.last_included_index_);
				endec::put_uint64(ptr, (uint64_t)head.last_included_term_);
				assert(buffer.size() == ptr - (unsigned char*)buffer.data());
				return write(buffer);
			}
			bool write(const std::string &buffer)
			{
				file_.write(buffer.data(), buffer.size());
				file_.flush();
				return file_.good();
			}
			void discard()
			{
				close();
				functors::fs::rm()(filepath_);
			}
			std::size_t get_bytes_writted()
			{
				return file_.tellp();
			}
			std::string get_snapshot_filepath()
			{
				return filepath_;
			}
		private:
			std::string filepath_;
			std::ofstream file_;
		};
	}
}