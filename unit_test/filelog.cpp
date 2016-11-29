#include "../../xtest/include/xtest.hpp"
#include "../include/raft/raft.hpp"

using namespace xraft;
using namespace xraft::detail;

XTEST_SUITE(filelog)
{
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
		xassert(flog.init("1/"));
	}
	XUNIT_TEST(write)
	{
		filelog flog;
		xassert(flog.init("1/"));
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
		xassert(flog.init("1/"));
		log_entry entry;
		xassert(flog.get_log_entry(1, entry));
	}

	XUNIT_TEST(get_log_entries)
	{
		filelog flog;
		xassert(flog.init("2/"));
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
		xassert(entries.size() == 200);
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
		xassert(flog.init("3/"));
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
		xassert(entries.size() == 100);
		int i = 1;
		for (auto &itr : entries)
		{
			xassert(itr.index_ == i);
			i++;
		}
	}
}