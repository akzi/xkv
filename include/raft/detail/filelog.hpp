#pragma once
namespace xraft
{
	namespace detail
	{
		class file
		{
		public:
			file()
			{

			}
			~file()
			{
				//stop filelog remove this file immediately .
				//when someone is  writing or reading.
				std::lock_guard<std::mutex> lock(mtx_);
			}
			void operator = (file &&f)
			{
				std::lock_guard<std::mutex> lock(mtx_);
				std::lock_guard<std::mutex> lock2(f.mtx_);
				if (&f == this)
					return;
				data_file_ = std::move(data_file_);
				index_file_ = std::move(index_file_);
				last_log_index_ = f.last_log_index_;
				log_index_start_ = f.log_index_start_;
			}
			file(file &&f)
			{
				std::lock_guard<std::mutex> lock(mtx_);
				std::lock_guard<std::mutex> lock2(f.mtx_);
				if (&f == this)
					return;
				data_file_ = std::move(data_file_);
				index_file_ = std::move(index_file_);
				last_log_index_ = f.last_log_index_;
				log_index_start_ = f.log_index_start_;
			}
			bool open(const std::string &filepath)
			{
				std::lock_guard<std::mutex> lock(mtx_);
				filepath_ = filepath;
				int mode = std::ios::out | 
					std::ios::in | 
					std::ios::app | 
					std::ios::binary | 
					std::ios::ate;
				data_file_.open(filepath_.c_str(), mode);
				index_file_.open((filepath_ + ".index").c_str(), mode);
				if (!data_file_.good() || !index_file_.good())
					return false;
				return true;
			}
			
			bool write(int64_t index, std::string &&data)
			{
				std::lock_guard<std::mutex> lock(mtx_);

				if (data_file_.good())
					return false;
				int64_t file_pos = data_file_.tellp();
				uint32_t len = (uint32_t)data.size();
				data_file_.write(reinterpret_cast<char*>(&len), sizeof len);
				data_file_.write(data.data(), data.size());
				data_file_.flush();
				if (!data_file_.good())
					return false;
				index_file_.write(reinterpret_cast<char*>(&index),sizeof(index));
				index_file_.write(reinterpret_cast<char*>(&file_pos), sizeof(file_pos));
				index_file_.flush();
				if (!index_file_.good())
					return false;
				return true;
			}

			bool get_log_entries(int64_t &index, 
								std::size_t &count,
								std::list<log_entry> &log_entries, 
								std::unique_lock<std::mutex> &lock)
			{
				std::lock_guard<std::mutex> lock_guard(mtx_);
				lock.unlock();

				auto diff = index - last_log_index_;
				std::size_t offset = diff * sizeof(int64_t) * 2;
				index_file_.seekg(offset, std::ios::beg);
				int64_t index_buffer_;
				index_file_.read((char*)&index_buffer_, sizeof(int64_t));
				if (!index_file_.good())
					return false;
				if(index_buffer_ != index)
					//todo log error.
					return false;
				int64_t data_file_offset;
				index_file_.read((char*)&data_file_offset, sizeof(int64_t));
				if (!index_file_.good())
					return false;

				data_file_.seekg(data_file_offset, std::ios::beg);
				if (!data_file_.good())
					return false;
				do 
				{
					uint32_t len;
					data_file_.read((char*)&len, sizeof(uint32_t));
					if (!data_file_.good())
						return data_file_.eof();
					std::string buffer;
					buffer.resize(len);
					data_file_.read((char*)buffer.data(), len);
					if (!data_file_.good())
						return false;
					log_entry entry;
					entry.from_string(buffer);
					log_entries.emplace_back(std::move(entry));
					index ++ ;
				} while (--count > 0);
				return true;
			}
			std::size_t size()
			{
				data_file_.seekp(0, std::ios::end);
				return data_file_.tellp();
			}
			//rm file from disk.
			void rm()
			{

			}
			int64_t get_last_log_index()
			{
				std::lock_guard<std::mutex> lock(mtx_);
				if (last_log_index_ == -1)
				{
					char buffer[sizeof(int64_t)] = { 0 };
					index_file_.seekg(sizeof(buffer), std::ios::end);
					index_file_.read(buffer, sizeof(buffer));
					if (!index_file_.good())
						return -1;
					last_log_index_ = *(int64_t*)(buffer);
				}
				return last_log_index_;
			}
			int64_t get_log_start()
			{
				std::lock_guard<std::mutex> lock(mtx_);
				if (log_index_start_ == -1)
				{
					index_file_.seekg(0, std::ios::beg);
					char buffer[sizeof(int64_t)] = { 0 };
					index_file_.read(buffer, sizeof(buffer));
					if (!index_file_.good())
						return -1;
					log_index_start_ = *(int64_t*)(buffer);
				}
				return log_index_start_;
			}
			bool is_open()
			{
				std::lock_guard<std::mutex> lock(mtx_);
				return data_file_.is_open();
			}
		private:
			bool read(int64_t offset, std::string &buffer)
			{
				if (!seek(offset))
					return false;
				if (!data_file_.good())
					return false;
				uint32_t len{ 0 };
				data_file_.read((char*)&len, sizeof(len));
				if (!data_file_.good())
					return false;
				buffer.resize(len);
				data_file_.read(const_cast<char*>(buffer.data()), len);
				if (!data_file_.good())
					return false;
				return true;
			}
			bool seek(int64_t offset)
			{
				if (!data_file_.good())
					return false;
				data_file_.seekg(offset, std::ios::beg);
				if (data_file_.good())
					return true;
				return false;
			}
			std::mutex mtx_;
			int64_t last_log_index_ = -1;
			int64_t log_index_start_ = -1;
			std::fstream data_file_;
			std::fstream index_file_;
			std::string filepath_;
		};

		class filelog
		{
		public:
			filelog()
			{
			}
			bool init(const std::string &dir)
			{
				if (!functors::fs::mkdir()(dir))
					return false;
				auto files = functors::fs::ls_files()(dir);
				for (auto &itr :files)
				{
					if (itr.find(".log") != std::string::npos)
					{
						file f;
						if (f.open(itr))
						{
							if (f.size() > max_file_size_)
								logfiles_.emplace(f.get_log_start(), std::move(f));
							else
							{
								assert(!current_file_.is_open());
								current_file_ = std::move(f);
							}
						}
						else
						{
							//todo log ERROR
						}
					}
				}
				return false;
			}

			int64_t write(detail::log_entry &&entry)
			{
				std::lock_guard<std::mutex> lock(mtx_);
				++last_index_;
				entry.index_ = last_index_;
				std::string buffer = entry.to_string();
				log_entries_cache_size_ += buffer.size();
				log_entries_cache_.emplace_back(std::move(entry));
				check_log_entries_size();
				current_file_.write(last_index_, std::move(buffer));
				check_current_file_size();
				return last_index_;
			}
			log_entry get_log_entry(uint64_t index)
			{
				//todo impl;
				return{};
			}
			std::list<log_entry> get_log_entries(int64_t index, std::size_t count = 10)
			{
				assert(index >= 0);
				assert(count);
				std::list<log_entry> log_entries;
				std::unique_lock<std::mutex> lock(mtx_);
				do
				{
					if (log_entries_cache_.size() && log_entries_cache_.front().index_ <= index)
					{
						for (auto &itr : log_entries_cache_)
						{
							if (index == itr.index_)
							{
								log_entries.push_back(itr);
								++index;
								count--;
								if (count == 0)
									return log_entries;
							}
						}
						return log_entries;
					}

					auto itr = logfiles_.upper_bound(index);
					if (itr == logfiles_.end())
						return log_entries;
					if (!itr->second.get_log_entries(index, count, log_entries, lock))
						//logo error
						return log_entries;
				} while (count > 0);

				return log_entries;
			}
			void truncate_prefix(int64_t index)
			{

			}
			void truncate_suffix(int64_t index)
			{
				last_index_ = index;
			}
			int64_t get_last_log_entry_term()
			{
				std::unique_lock<std::mutex> lock(mtx_);
				if (log_entries_cache_.size())
					return log_entries_cache_.back().term_;
			}
			int64_t get_last_index()
			{
				std::unique_lock<std::mutex> lock(mtx_);
				return last_index_;
			}
			int64_t get_log_start_index()
			{
				std::unique_lock<std::mutex> lock(mtx_);
				if (logfiles_.size())
					return logfiles_.begin()->second.get_log_start();
				else
					return current_file_.get_log_start();
				return 0;
			}
		private:
			void check_log_entries_size()
			{
				while (log_entries_cache_.empty() &&
					log_entries_cache_size_ > max_cache_size_)
				{
					log_entries_cache_size_ -= log_entries_cache_.front().bytes();
					log_entries_cache_.pop_front();
				}
			}
			void check_current_file_size()
			{
				if (current_file_.size() > max_file_size_  )
				{
					logfiles_.emplace(current_file_.get_log_start(), std::move(current_file_));
					if (!current_file_.open(std::to_string(last_index_ + 1) + ".log"))
					{
						//todo log error
					}
				}
			}

			std::mutex mtx_;
			std::list<log_entry> log_entries_cache_;
			std::size_t log_entries_cache_size_ = 10*1024*1024;
			std::size_t max_cache_size_ = 10*1024*1024;
			std::size_t max_file_size_ = 10*1024*1024;
			int64_t last_index_ = 1;
			file current_file_;
			int64_t crrent_file_last_index_;
			std::map<int64_t, detail::file> logfiles_;
		};
	}
	
}