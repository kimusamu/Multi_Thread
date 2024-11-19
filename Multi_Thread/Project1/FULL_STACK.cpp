#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <random>
#include <array>
#include <atomic>
#include <queue>
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

class C_STACK {
	NODE* volatile m_top;
	std::mutex st_lock;

public:
	C_STACK()
	{
		m_top = nullptr;
	}

	void clear()
	{
		while (-2 != Pop());
	}

	void Push(int x)
	{
		NODE* e = new NODE{ x };

		st_lock.lock();

		e->next = m_top;
		m_top = e;

		st_lock.unlock();
	}

	int Pop()
	{
		st_lock.lock();

		if (nullptr == m_top) {
			st_lock.unlock();
			return -2;
		}

		int value = m_top->key;
		NODE* temp = m_top;
		m_top = m_top->next;

		st_lock.unlock();

		delete temp;
		return value;
	}

	void print20()
	{
		NODE* p = m_top;

		for (int i = 0; i < 20; ++i) {
			if (nullptr == p) {
				break;
			}

			std::cout << p->key << ", ";
			p = p->next;
		}

		std::cout << std::endl;
	}
};

class LF_STACK {
	NODE* volatile m_top;

	bool CAS(NODE* old_ptr, NODE* new_ptr)
	{
		return std::atomic_compare_exchange_strong(
			reinterpret_cast<volatile std::atomic_llong*>(&m_top),
			reinterpret_cast<long long*>(&old_ptr),
			reinterpret_cast<long long>(new_ptr)
		);
	}

public:
	LF_STACK()
	{
		m_top = nullptr;
	}

	void clear()
	{
		while (-2 != Pop());
	}

	void Push(int x)
	{
		NODE* e = new NODE{ x };

		while (true) {
			auto old_top = m_top;
			e->next = old_top;

			if (true == (CAS(old_top, e))) {
				return;
			}
		}
	}

	int Pop()
	{
		while (true) {
			auto old_top = m_top;

			if (old_top == nullptr) {
				return -2;
			}

			auto new_top = old_top->next;
			int value = old_top->key;

			if (true == (CAS(old_top, new_top))) {
				return value;
			}
		}
	}

	void print20()
	{
		NODE* p = m_top;

		for (int i = 0; i < 20; ++i) {
			if (nullptr == p) {
				break;
			}

			std::cout << p->key << ", ";
			p = p->next;
		}

		std::cout << std::endl;
	}
};

constexpr int MAX_EXCHANGER = (MAX_THREADS / 2) - 1;
constexpr int RET_TIMEOUT = -2;
constexpr int RET_BUSYTIMEOUT = -3;
constexpr int RET_POP = -1;

constexpr int ST_EMPTY = 0;
constexpr int ST_WAIT = 1;
constexpr int ST_BUSY = 2;

constexpr int MAX_LOOP = 10;

std::atomic_int exchange_count = 0;

class LockFreeExchanger {
	volatile unsigned int slot;

	unsigned int get_slot(unsigned int* st)
	{
		unsigned t = slot;
		*st = t >> 30;
		return t & 0x3FFFFFFF;
	}

	unsigned int get_state()
	{
		return slot >> 30;
	}

	bool CAS(unsigned old_v, unsigned new_v, unsigned old_st, unsigned new_st)
	{
		unsigned int o_slot = old_v + (old_st << 30);
		unsigned n_slot = new_v + (new_st << 30);
		return std::atomic_compare_exchange_strong(
			reinterpret_cast<volatile std::atomic_uint*>(&slot),
			&o_slot,
			n_slot
		);
	}

public:
	int exchange(int v)
	{
		unsigned st = 0;

		for (int i = 0; i < MAX_LOOP; ++i) {
			unsigned int old_v = get_slot(&st);

			switch (st) {
			case ST_EMPTY:
				if (true == CAS(old_v, v, ST_EMPTY, ST_WAIT)) {
					bool time_out = true;

					for (int j = 0; j < MAX_LOOP; ++j) {
						if (ST_WAIT != get_state()) {
							time_out = false;
							break;
						}

						if (false == time_out) {
							int ret = get_slot(&st);
							slot = 0;
							return ret;
						}

						else {
							if (true == CAS(old_v, 0, ST_WAIT, ST_EMPTY)) {
								return RET_TIMEOUT; // 기다려도 오지 않아서 return
							}

							else {
								int ret = get_slot(&st);
								slot = 0;
								return ret;
							}
						}
					}
				}

				break;

			case ST_WAIT:
				if (true == CAS(old_v, v, ST_WAIT, ST_BUSY)) {
					++exchange_count;
					return old_v;
				}

				break;

			case ST_BUSY:
				break;

			default:
				std::cout << "Invalid Exchange State Error" << std::endl;
				exit(-1);
			}
		}

		return RET_BUSYTIMEOUT;
	}
};

class EliminationArray {
	int range;
	LockFreeExchanger exchanger[MAX_EXCHANGER];

public:
	EliminationArray()
	{
		range = 1;
	}

	~EliminationArray()
	{
	}

	int Visit(int value)
	{
		int slot = rand() % range;
		int ret = exchanger[slot].exchange(value);
		int old_range = range;

		if (ret == RET_BUSYTIMEOUT) {
			if (old_range < MAX_EXCHANGER - 1) {
				std::atomic_compare_exchange_strong(
					reinterpret_cast<std::atomic_int*>(&range),
					&old_range,
					old_range + 1);
			}
		}

		else if (ret == RET_TIMEOUT) {
			if (old_range > 1) {
				std::atomic_compare_exchange_strong(
					reinterpret_cast<std::atomic_int*>(&range),
					&old_range,
					old_range - 1);
			}
		}

		else {
			return ret;
		}
	}
};

class LF_EL_STACK {
	EliminationArray m_earr;

	NODE* volatile m_top;

	bool CAS(NODE* old_ptr, NODE* new_ptr)
	{
		return std::atomic_compare_exchange_strong(
			reinterpret_cast<volatile std::atomic_llong*>(&m_top),
			reinterpret_cast<long long*>(&old_ptr),
			reinterpret_cast<long long>(new_ptr)
		);
	}

public:
	LF_EL_STACK()
	{
		m_top = nullptr;
	}

	void clear()
	{
		while (-2 != Pop());
	}

	void Push(int x)
	{
		NODE* e = new NODE{ x };

		while (true) {
			NODE* last = m_top;
			e->next = last;

			if (true == CAS(last, e)) {
				return;
			}

			int ret = m_earr.Visit(x);

			if (ret == RET_POP) {
				return;
			}
		}
	}

	int Pop()
	{
		while (true) {
			NODE* volatile last = m_top;

			if (nullptr == last) {
				return -2;
			}

			NODE* volatile next = last->next;

			if (last != m_top) {
				continue;
			}

			int v = last->key;

			if (true == CAS(last, next)) {
				return v;
			}

			m_earr.Visit(RET_POP);
		}
	}

	void print20()
	{
		NODE* p = m_top;

		for (int i = 0; i < 20; ++i) {
			if (nullptr == p) {
				break;
			}

			std::cout << p->key << ", ";
			p = p->next;
		}

		std::cout << std::endl;
	}
};

LF_EL_STACK my_stack;

thread_local int thread_id;

const int NUM_TEST = 10000000;

void benchmark_test(const int th_id, const int num_threads, HISTORY& h)
{
	int loop_count = NUM_TEST / num_threads;
	thread_id = th_id;

	for (int i = 0; i < loop_count; i++) {
		if ((rand() % 2) || i < 128 / num_threads) {
			h.push_values.push_back(i);
			stack_size++;
			my_stack.Push(i);
		}

		else {
			stack_size--;
			int res = my_stack.Pop();

			if (res == -2) {
				stack_size++;

				if (stack_size > (num_threads * 2 + 2)) {
					//std::cout << "ERROR Non_Empty Stack Returned NULL\n";
					//exit(-1);
				}
			}

			else {
				h.pop_values.push_back(res);
			}
		}
	}
}

void check_history(std::vector <HISTORY>& h)
{
	std::unordered_multiset <int> pushed, poped, in_stack;

	for (auto& v : h)
	{
		for (auto num : v.push_values) {
			pushed.insert(num);
		}

		for (auto num : v.pop_values) {
			poped.insert(num);
		}

		while (true) {
			int num = my_stack.Pop();
			if (num == -2) break;
			poped.insert(num);
		}
	}

	for (auto num : pushed) {
		if (poped.count(num) < pushed.count(num)) {
			std::cout << "Pushed Number " << num << " does not exists in the STACK.\n";
			exit(-1);
		}

		if (poped.count(num) > pushed.count(num)) {
			std::cout << "Pushed Number " << num << " is poped more than " << poped.count(num) - pushed.count(num) << " times.\n";
			exit(-1);
		}
	}

	for (auto num : poped)
		if (pushed.count(num) == 0) {
			std::multiset <int> sorted;
			for (auto num : poped) {
				sorted.insert(num);
			}

			std::cout << "There was elements in the STACK no one pushed : ";
			int count = 20;

			for (auto num : sorted) {
				std::cout << num << ", ";
			}

			std::cout << std::endl;
			exit(-1);

		}

	std::cout << "NO ERROR detectd.\n";
}

int main()
{
	using namespace std::chrono;

	for (int n = 1; n <= MAX_THREADS; n = n * 2) {
		my_stack.clear();

		std::vector<std::thread> tv;
		std::vector<HISTORY> history;

		history.resize(n);

		auto start_t = high_resolution_clock::now();

		for (int i = 0; i < n; ++i) {
			tv.emplace_back(benchmark_test, i, n, std::ref(history[i]));
		}

		for (auto& th : tv) {
			th.join();
		}

		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		size_t ms = duration_cast<milliseconds>(exec_t).count();

		std::cout << n << " Threads,  " << ms << "ms. ---- ";

		my_stack.print20();
		check_history(history);

		std::cout << "Total Exchanges : " << exchange_count.load() << std::endl;
	}
}