#pragma once
namespace xraft
{
namespace detail
{
	struct lock_free 
	{
		void lock() {};
		void unlock() {};
	};

	template<typename mutex = std::mutex>
	class metadata
	{
		enum type:char
		{
			e_string,
			e_integral
		};
		enum op:char
		{
			e_set,
			e_del,
		};
	public:
		metadata()
		{

		}
		~metadata()
		{

		}
		bool init(const std::string &path)
		{
			if (!functors::fs::mkdir()(path))
				return false;

			auto files = functors::fs::ls()(path);
			std::sort(files.begin().files.end());
			load();
		}
		bool set(const std::string &key, const std::string &value)
		{
			std::lock_guard<std::mutex> lock(mtx_);
			if (!write_log(build_log(key, value, op::e_set)))
				return false;
			string_map_[key] = value;
			return true;
		}
		bool set(const std::string &key, int64_t value)
		{
			std::lock_guard<std::mutex> lock(mtx_);
			if (!write_log(build_log(key, value, op::e_set)))
				return false;
			string_map_[key] = value;
			return true;
		}
		bool get(const std::string &key, std::string &value)
		{
			std::lock_guard<std::mutex> lock(mtx_);
			auto itr = string_map_.find(key);
			if (itr == string_map_.end())
				return false;
			value = itr->second;
			return true;
		}
		bool get(const std::string &key, int64_t &value)
		{
			std::lock_guard<std::mutex> lock(mtx_);
			auto itr = integral_map_.find(key);
			if (itr == integral_map_.end())
				return false;
			value = itr->second;
			return true;
		}
		
	private:
		std::string build_log(const std::string &key, const std::string &value,op _op)
		{
			std::string buffer;
			buffer.resize(endec::get_sizeof(key) +
				endec::get_sizeof(key) +
				endec::get_sizeof(std::underlying_type<type>::type()) +
				endec::get_sizeof(std::underlying_type<op>::type()));
			unsigned char* ptr = (unsigned char*)buffer.data();
			endec::put_uint8(ptr, _op);
			endec::put_uint8(ptr, type::e_string);
			endec::put_string(ptr, key);
			endec::put_string(ptr, value);
			return std::move(buffer);
		}
		std::string build_log(const std::string &key, int64_t &value, op _op)
		{
			std::string buffer;
			buffer.resize(endec::get_sizeof(key) +
				endec::get_sizeof(key) +
				endec::get_sizeof(std::underlying_type<type>::type()) +
				endec::get_sizeof(std::underlying_type<op>::type()));

			unsigned char* ptr = (unsigned char*)buffer.data();
			endec::put_uint8(ptr, _op);
			endec::put_uint8(ptr, type::e_integral);
			endec::put_string(ptr, key);
			endec::put_uint64(ptr, (uint64_t)value);
			return std::move(buffer);
		}
		bool write_log(const std::string &data)
		{
			uint32_t len = (uint32_t)data.size();
			log_.write((char*)(&len), sizeof len);
			log_.write(data.data(), data.size());
			log_.flush();
			return log_.good();
		}
		void load()
		{

		}
		template<typename T>
		bool write(std::ofstream &file, T &map)
		{
			for (auto &itr : map)
			{
				std::string log = build_log(itr->first, itr->second, op::e_set);
				file.write(log.data(), log.size());
				if (!file.good())
					return false;
			}
			return true;
		}
		void make_snapshot()
		{
			if (max_log_file_ > log_.tellp())
				return;
			std::ofstream file;
			int mode = std::ios::binary |
				std::ios::trunc |
				std::ios::out;
			++file_index_;
			file.open(get_snapshot_file().c_str(), mode);
			if (!file.good())
			{
				//process error
			}
			if (!write(file, string_map_))
			{
				//process error
			}
			if (!write(file, integral_map_))
			{
				//process error
			}
			file.flush();
			file.close();
			if (!reopen_log())
			{

			}
			if (!touch_metadata_file())
			{

			}
			if (!rm_old_files())
			{

			}
		}
		bool reopen_log()
		{
			log_.close();
			int mode = 
				std::ios::binary | 
				std::ios::trunc | 
				std::ios::out;
			log_.open(get_log_file().c_str(), mode);
			return log_.good();
		}
		bool touch_metadata_file()
		{
			std::ofstream file;
			file.open(get_metadata_file().c_str());
			auto is_ok = file.good();
			file.close();
			return is_ok;
		}
		bool rm_old_files()
		{
			if (!functors::fs::rm()(get_old_log_file()))
			{
				//todo log error
			}
			if (!functors::fs::rm()(get_old_snapshot_file()))
			{
				//todo log error
			}
			if (!functors::fs::rm()(get_old_metadata_file()))
			{
				//todo log error
			}
		}
		std::string get_snapshot_file()
		{
			return std::to_string(file_index_)+ ".metadata.ss";
		}
		std::string get_log_file()
		{
			return std::to_string(file_index_) + ".metadata.log";
		}
		std::string get_metadata_file()
		{
			return std::to_string(file_index_) + ".metadata";
		}
		std::string get_old_snapshot_file()
		{
			return std::to_string(file_index_ - 1) + ".metadata.ss";
		}
		std::string get_old_log_file()
		{
			return std::to_string(file_index_ - 1) + ".metadata.log";
		}
		std::string get_old_metadata_file()
		{
			return std::to_string(file_index_ - 1) + ".metadata";
		}
		uint64_t file_index_ = 0;
		std::size_t max_log_file_ = 1024 * 1024;
		std::ofstream log_;
		std::string filepath_;
		mutex mtx_;
		std::map<std::string, std::string> string_map_;
		std::map<std::string, int64_t> integral_map_;
	};
}
}