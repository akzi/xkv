#include "../../xtest/include/xtest.hpp"
#include "../include/raft/raft.hpp"
xtest_run;

XTEST_SUITE(metadata)
{
	XUNIT_TEST(do_metadata_set)
	{
		using namespace::xraft::detail;
		metadata<lock_free> db;

		xassert(db.init("F:/fork/akzi/xraft/unit_test/data/"));
		
		for (size_t i = 0; i < 10000; i++)
		{
			xassert(db.set(std::to_string(i), std::to_string(i)));
		}
		std::cout <<"ok"<< std::endl;
	}

	XUNIT_TEST(do_metadata_get)
	{
		using namespace::xraft::detail;
		metadata<lock_free> db;

		xassert(db.init("F:/fork/akzi/xraft/unit_test/data/"));

		for (size_t i = 0; i < 10000; i++)
		{
			std::string value;
			xassert(db.get(std::to_string(i), value));
			xassert(std::to_string(i) == value);
		}
		std::cout << "ok" << std::endl;
	}
	XUNIT_TEST(do_metadata_del)
	{
		using namespace::xraft::detail;
		metadata<lock_free> db;

		xassert(db.init("F:/fork/akzi/xraft/unit_test/data/"));

		for (size_t i = 0; i < 10000; i++)
		{
			std::string value;
			xassert(db.del(std::to_string(i)));
		}
		std::cout << "ok" << std::endl;
	}
	XUNIT_TEST(do_metadata_batchmark)
	{
		using namespace::xraft::detail;
		metadata<lock_free> db;

		xassert(db.init("F:/fork/akzi/xraft/unit_test/data/"));
		int64_t count_ = 0;
		std::thread counter([&] {
			auto  last = count_;
			do
			{
				std::cout <<"writes:"<<count_ - last << std::endl;;
				std::cout << "count:"<<count_ << std::endl;;
				last = count_;
				std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			} while (true);
		});
		counter.detach();
		for (size_t i = 0; i < 1000000000; i++)
		{
			count_++;
			xassert(db.set(std::to_string(i), std::to_string(i)));
		}
		std::cout << "ok" << std::endl;
	}
}