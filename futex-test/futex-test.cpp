/* Test for futex */

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>
#include <iostream>
#include <sstream>


#ifdef _MSC_VER
#define NOOP __asm nop
#else
#define NOOP asm("");
#endif

enum {
   DRY_RUN=0xFFFFFFFF,
   CHANGE_COUNT=0x10
};


template<unsigned int spinCount=0>
class Futex {
   std::atomic<bool> m_owned{false};

public:
   Futex() {}
   Futex(const Futex& )= delete;
   Futex(Futex&& )= delete;
   ~Futex() {}

   void lock() noexcept {
      if (spinCount == DRY_RUN) {
         return;
      }

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
      if (spinCount == DRY_RUN) {
         return;
      }
      m_owned.store(false, std::memory_order_release);
   }
   
   unsigned int get_spin() const { return spinCount; }
};

template<class mutex_type>
void check_func(std::vector<int>& shared_data, mutex_type& mutex, int perfCount, int outOfBusyLoopCount) {
	volatile int dummy = 1;

	for (int i = 0; i < perfCount; ++i) {
		// Simulate some out of the main loop operation
		for (int j = 0; j < outOfBusyLoopCount; ++j) {
			++dummy;
			NOOP
		}
		
		std::lock_guard<mutex_type> guard(mutex);
    	for(int pos = 0; pos < shared_data.size(); pos += shared_data.size()/CHANGE_COUNT) {
            shared_data[pos]++;
        }
	}
};


long long checkSharedData(std::vector<int>& shared_data)
{
    long long count = 0;
    for(int pos = 0; pos < shared_data.size(); pos += shared_data.size()/CHANGE_COUNT) {
        count += shared_data[pos];
    }
    return count;
}


template<class mutex_type>
auto performance_test(bool dry, mutex_type& mutex, int threadCount, int perfCount, int outOfBusyLoopCount)
{
	auto now = std::chrono::high_resolution_clock::now();

    std::vector<int> shared_data(0x100000);
	
   // Thread launcher
   std::vector<std::thread> threads;
   for (int i = 0; i < threadCount; ++i) {
	   threads.emplace_back(
		   [&]() {
			check_func(shared_data, mutex, perfCount, outOfBusyLoopCount);
		   }
	   );
   }

   // Waiting for threads to be done
   for (auto& thread: threads) {
      thread.join();
   }

   auto after = std::chrono::high_resolution_clock::now();
   
   // this will ensure memory coherency
   long long fullCount = checkSharedData(shared_data);
   long long paragon = static_cast<long long>(perfCount) * 
                        static_cast<long long>(threadCount) * CHANGE_COUNT;
                        
   if (!dry && fullCount != paragon)
   {
       std::ostringstream ss;
       ss << "Lock failed: " << fullCount << "/" << paragon;
       throw std::runtime_error(ss.str().c_str());
   }
   return std::chrono::duration_cast<std::chrono::milliseconds>(after - now).count();
}


auto all_timings(bool dry_run, int threadCount, int perfCount, int outOfBusyLoopCount)
{
	Futex<DRY_RUN> dry;
	Futex<0> futex;
	Futex<1> yield_futex;
	Futex<0x40> sl_futex;
	std::mutex mutex;

	std::vector<long long> timings;
	if (dry_run) {
           timings = {performance_test(true, dry, threadCount, perfCount, outOfBusyLoopCount),};
	}
        else {
	   timings = {
          performance_test(false, futex, threadCount, perfCount, outOfBusyLoopCount),
          performance_test(false, yield_futex, threadCount, perfCount, outOfBusyLoopCount),
          performance_test(false, sl_futex, threadCount, perfCount, outOfBusyLoopCount),
	      performance_test(false, mutex, threadCount, perfCount, outOfBusyLoopCount)
	   };
        }

	return timings;
}


void line_test(bool dry_run, int threadCount, int perfCount, int outOfBusyLoopCount)
{
	std::cout << threadCount << "; "
			  << perfCount << "; "
			  << outOfBusyLoopCount << "; " << std::flush;

	auto timings = all_timings(dry_run, threadCount, perfCount, outOfBusyLoopCount);

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
			{10000000, 0},
			{1000000, 50},
			{ 980000, 200},
			{ 950000, 400},
			{ 900000, 800},
			{ 850000, 1200},
			{ 700000, 2000},
			{ 400000, 10000},
	};

	for(const auto& parms: lparms) {
		for(int threadCount = 1; threadCount <= 16; ++threadCount){
			tests.push_back({threadCount, parms.first/threadCount, parms.second});
		}
	}

	return tests;
}


int main(int argc, char*[])
{
	auto tests = generate_tests();

	if(argc == 1) {
		std::cout << "\"Threads\"; \"Iterations\"; \"Non-contended Loops\"; "
			  << "\"Time Futex<0>\"; \"Time Futex<1>\"; \"Time Futex<40>\"; \"Time std::mutex\"; "
			  << "\"Rel Futex<0>\"; \"Rel Futex<1>\"; \"Rel Futex<40>\"; \"Rel std::mutex\";\n";
	}
	else {
		std::cout << "\"Threads\"; \"Iterations\"; \"Non-contended Loops\"; \"Baseline\";\n";
	
	}

	for(const auto& tp: tests) {
		line_test(argc > 1, tp.threadCount, tp.perfCount, tp.outOfBusyLoopCount);
	}
	return 0;
}

