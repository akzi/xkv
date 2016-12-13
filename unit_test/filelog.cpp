#include "../include/raft/raft.hpp"
#include "../../xtest/include/xtest.hpp"

using namespace xraft;
using namespace xraft::detail;
#if 0

XTEST_SUITE(filelog)
{
#if 1
	XUNIT_TEST(to_string)
	{
		log_entry entry, entry2;
		entry.log_data_ = "hello world";
		entry.term_ = 100;
		entry.index_ = 1002;
		auto buffer = entry.to_string();

		entry2.from_string(buffer);

	}
	XUNIT_TEST(init)
	{
		filelog flog;
		xassert(flog.init("data/"));
	}
	XUNIT_TEST(write)
	{
		filelog flog;
		xassert(flog.init("data/write/"));
		log_entry entry;
		entry.log_data_ = "hello world";
		entry.term_ = 1000;
		entry.type_ = log_entry::type::e_append_log;
		int64_t index;
		xassert(flog.write(std::move(entry), index));
	}

	XUNIT_TEST(get_log_entry)
	{
		filelog flog;
		xassert(flog.init("data/write/"));
		log_entry entry;
		xassert(flog.get_log_entry(1, entry));
	}

	XUNIT_TEST(get_log_entries)
	{
		filelog flog;
		xassert(flog.init("data/get_log_entries/"));
		log_entry entry;
		entry.log_data_ = "hello world";
		entry.term_ = 1000;
		entry.type_ = log_entry::type::e_append_log;
		int64_t index;
		for (size_t i = 0; i < 200; i++)
		{
			xassert(flog.write(std::move(entry), index));
		}

		auto entries = flog.get_log_entries(1, 300);
		int i = 1;
		for (auto &itr: entries)
		{
			xassert(itr.index_ == i);
			i++;
		}

	}
	XUNIT_TEST(truncate_suffix)
	{
		filelog flog;
		xassert(flog.init("data/truncate_suffix/"));
		log_entry entry;
		entry.log_data_ = "hello world";
		entry.term_ = 1000;
		entry.type_ = log_entry::type::e_append_log;
		int64_t index;
		for (size_t i = 0; i < 200; i++)
		{
			xassert(flog.write(std::move(entry), index));
		}
		flog.truncate_suffix(101);
		auto entries = flog.get_log_entries(1, 300);
		int i = 1;
		for (auto &itr : entries)
		{
			xassert(itr.index_ == i);
			i++;
		}
	}
#endif
	XUNIT_TEST(truncate_prefix)
	{
		filelog flog;
		xassert(flog.init("data/truncate_prefix/"));
		log_entry entry;
		entry.log_data_ = "hello world";
		entry.term_ = 1000;
		entry.type_ = log_entry::type::e_append_log;
		int64_t index;
		for (size_t i = 0; i < 200; i++)
		{
			xassert(flog.write(std::move(entry), index));
		}
		flog.truncate_prefix(100);
		auto entries = flog.get_log_entries(101, 200);
		int i = 101;
		for (auto &itr : entries)
		{
			xassert(itr.index_ == i);
			i++;
		}
	}
}
#endif
