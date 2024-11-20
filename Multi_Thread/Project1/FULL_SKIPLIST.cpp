#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <unordered_set>
#include <set>

constexpr int MAX_THREADS = 16;

class NODE {
public:
	int	key;
	NODE* volatile next;
	NODE(int x) : key(x), next(nullptr) {}
};

class DUMMYMUTEX
{
public:
	void lock() {}
	void unlock() {}
};

struct HISTORY {
	std::vector <int> push_values, pop_values;
};

std::atomic_int stack_size;

constexpr int MAX_TOP = 9;

class SKNODE {
public:
	int	key;
	SKNODE* volatile next[MAX_TOP + 1];
	std::mutex n_lock;
	int top_level;

	SKNODE(int x, int top) : key(x), top_level(top)
	{
		for (auto& p : next) {
			p = nullptr;
		}
	}
};

class C_SKLIST {
	SKNODE head{ std::numeric_limits<int>::min(), MAX_TOP },
		tail{ std::numeric_limits<int>::max(), MAX_TOP };
	std::mutex set_lock;

public:
	C_SKLIST()
	{
		for (auto& p : head.next) {
			p = &tail;
		}
	}

	void clear()
	{
		while (head.next[0] != &tail) {
			auto p = head.next[0];
			head.next[0] = head.next[0]->next[0];
			delete p;
		}

		for (auto& p : head.next) {
			p = &tail;
		}
	}

	void Find(int x, SKNODE* prevs[], SKNODE* currs[])
	{
		for (int i = MAX_TOP; i >= 0; --i) {
			if (i == MAX_TOP) {
				prevs[i] = &head;
			}

			else {
				prevs[i] = prevs[i + 1];
			}

			currs[i] = prevs[i]->next[i];

			while (currs[i]->key < x) {
				prevs[i] = currs[i];
				currs[i] = currs[i]->next[i];
			}
		}
	}

	bool Add(int x)
	{
		SKNODE* prevs[MAX_TOP + 1];
		SKNODE* currs[MAX_TOP + 1];

		set_lock.lock();

		Find(x, prevs, currs);

		if (currs[0]->key == x) {
			set_lock.unlock();
			return false;
		}

		else {
			int lv = 0;

			for (int i = 0; i < MAX_TOP; ++i) {
				if (rand() % 2 == 0) {
					break;
				}

				lv++;
			}

			SKNODE* n = new SKNODE{ x, lv };

			for (int i = 0; i < lv; ++i) {
				n->next[i] = currs[i];
				prevs[i]->next[i] = n;
			}

			set_lock.unlock();
			return true;
		}
	}

	bool Remove(int x)
	{
		SKNODE* prevs[MAX_TOP + 1];
		SKNODE* currs[MAX_TOP + 1];

		set_lock.lock();

		Find(x, prevs, currs);

		if (currs[0]->key == x) {
			for (int i = 0; i < currs[0]->top_level; ++i) {
				prevs[i]->next[i] = currs[i]->next[i];
			}

			delete currs[0];
			set_lock.unlock();
			return true;
		}

		else {
			set_lock.unlock();
			return false;
		}
	}

	bool Contains(int x)
	{
		SKNODE* prevs[MAX_TOP + 1];
		SKNODE* currs[MAX_TOP + 1];

		set_lock.lock();

		Find(x, prevs, currs);

		if (currs[0]->key == x) {
			set_lock.unlock();
			return true;
		}

		else {
			set_lock.unlock();
			return false;
		}
	}

	void print20()
	{
		auto p = head.next[0];

		for (int i = 0; i < 20; ++i) {
			if (p == &tail) {
				break;
			}

			std::cout << p->key << ", ";
			p = p->next[0];
		}

		std::cout << std::endl;
	}
};

C_SKLIST my_sklist;

thread_local int thread_id;

const int NUM_TEST = 4000000;
const int KEY_RANGE = 1000;

void benchmark(const int th_id, const int num_thread)
{
	int key = 0;
	int loop_count = NUM_TEST / num_thread;
	thread_id = th_id;

	for (auto i = 0; i < loop_count; ++i) {
		switch (rand() % 3) {
		case 0:
			key = rand() % KEY_RANGE;
			my_sklist.Add(key);
			break;

		case 1:
			key = rand() % KEY_RANGE;
			my_sklist.Remove(key);
			break;

		case 2:
			key = rand() % KEY_RANGE;
			my_sklist.Contains(key);
			break;

		default:
			exit(-1);
		}
	}
}

int main()
{
	using namespace std::chrono;

	for (int n = 1; n <= MAX_THREADS; n = n * 2) {
		my_sklist.clear();

		std::vector<std::thread> tv;
		std::vector<HISTORY> history;

		history.resize(n);

		auto start_t = high_resolution_clock::now();

		for (int i = 0; i < n; ++i) {
			tv.emplace_back(benchmark, i, n);
		}

		for (auto& th : tv) {
			th.join();
		}

		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		size_t ms = duration_cast<milliseconds>(exec_t).count();

		std::cout << n << " Threads,  " << ms << "ms. ---- ";

		my_sklist.print20();
	}
}