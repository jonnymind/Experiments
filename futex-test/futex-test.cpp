/* Test for futex */

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <iostream>

#ifdef _MSC_VER
#define NOOP __asm nop
#else
#define NOOP asm("");
#endif



template<unsigned int spinCount=0>
class Futex {
   std::atomic<bool> m_owned{false};

public:
   Futex() {}
   Futex(const Futex& )= delete;
   Futex(Futex&& )= delete;
   ~Futex() {}

   void lock() noexcept {
      bool isOwned = false;
      unsigned int sc = spinCount;
      while(!m_owned.compare_exchange_weak(
            isOwned, true,
            std::memory_order_release,
            std::memory_order_relaxed) || isOwned ) {
         isOwned = false;

		 // This will be optimized into a Yield, noop or a proper
		 if (spinCount == 1) {
			 std::this_thread::yield();
		 }
         else if(spinCount && --sc == 0) {
            std::this_thread::yield();
         }
      }
   }

   void unlock() noexcept {
      m_owned.store(false, std::memory_order_release);
   }
};

template<class _Mutex>
int check_func(_Mutex& mutex, int perfCount, int outOfBusyLoopCount) {
	volatile int counter = 0;
	volatile int dummy = 1;
	for (int i = 0; i < perfCount; ++i) {
		// Simulate some out of the main loop operation
		for (int j = 0; j < outOfBusyLoopCount; ++j) {
			++dummy;
			NOOP
		}

		std::lock_guard guard(mutex);
		++counter;
		NOOP
	}

	return counter;
};


template<class _Mutex>
auto performance_test(_Mutex& mutex, int threadCount, int perfCount, int outOfBusyLoopCount)
{
	auto now = std::chrono::high_resolution_clock::now();

   // Thread launcher
   std::vector<std::thread> threads;
   for (int i = 0; i < threadCount; ++i) {
	   threads.emplace_back(
		   [&]() {
			   int counter = check_func(mutex, perfCount, outOfBusyLoopCount);
			   // this should ensure we're not killing the busy loops
			   if (counter != perfCount)
			   {
				   throw std::runtime_error("Lock failed");
			   }
		   }
	   );
   }

   // Waiting for threads to be done
   for (auto& thread: threads) {
      thread.join();
   }

   auto after = std::chrono::high_resolution_clock::now();
   return std::chrono::duration_cast<std::chrono::milliseconds>(after - now).count();
}


auto all_timings(int threadCount, int perfCount, int outOfBusyLoopCount)
{

	Futex<0> futex;
	Futex<1> yield_futex;
	Futex<40> sl_futex;
	std::mutex mutex;

	std::vector timings = {
		performance_test(futex, threadCount, perfCount, outOfBusyLoopCount),
		performance_test(yield_futex, threadCount, perfCount, outOfBusyLoopCount),
		performance_test(sl_futex, threadCount, perfCount, outOfBusyLoopCount),
		performance_test(mutex, threadCount, perfCount, outOfBusyLoopCount)
	};

	return timings;
}


void line_test(int threadCount, int perfCount, int outOfBusyLoopCount)
{
	std::cout << threadCount << "; "
			  << perfCount << "; "
			  << outOfBusyLoopCount << "; " << std::flush;

	auto timings = all_timings(threadCount, perfCount, outOfBusyLoopCount);

	double min_time = 9999999999.0;
	for(const auto& timing: timings) {
		if (static_cast<double>(timing) < min_time && timing > 0.0) {
			min_time = static_cast<double>(timing);
		}
		std::cout << timing << "; ";
	}

	for(const auto& timing: timings) {
		std::cout << static_cast<double>(timing) / min_time << "; ";
	}

	std::cout << "\n";
}


struct test_params
{
	int threadCount;
	int perfCount;
	int outOfBusyLoopCount;
};


auto generate_tests()
{
	std::vector<test_params> tests;
	std::vector<std::pair<int, int>> lparms = {
			{100000000, 0},
			{100000000, 5},
			{ 98000000, 20},
			{ 95000000, 40},
			{ 90000000, 80},
			{ 85000000, 120},
			{ 70000000, 200},
			{ 40000000, 1000},
	};

	for(const auto& parms: lparms) {
		for(int threadCount = 1; threadCount <= 16; ++threadCount){
			tests.push_back({threadCount, parms.first/threadCount, parms.second});
		}
	}

	return tests;
}


int main(int, char*[])
{
	auto tests = generate_tests();

	std::cout << "\"Threads\"; \"Iterations\"; \"Non-contended Loops\"; "
			  << "\"Time Futex<0>\"; \"Time Futex<1>\"; \"Time Futex<40>\"; \"Time std::mutex\"; "
			  << "\"Rel Futex<0>\"; \"Rel Futex<1>\"; \"Rel Futex<40>\"; \"Rel std::mutex\";\n";
	for(const auto& tp: tests) {
		line_test(tp.threadCount, tp.perfCount, tp.outOfBusyLoopCount);
	}
	return 0;
}

