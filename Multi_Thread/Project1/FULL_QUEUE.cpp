#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <array>
#include <atomic>
#include <queue>
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

class C_QUEUE {
	NODE* volatile head;
	NODE* volatile tail;
	std::mutex head_lock, tail_lock;

public:
	C_QUEUE()
	{
		head = tail = new NODE{ -1 };
	}

	void clear()
	{
		while (-1 != Dequeue());
	}

	void Enqueue(int x)
	{
		tail_lock.lock();

		NODE* e = new NODE{ x };
		tail->next = e;
		tail = e;

		tail_lock.unlock();
	}

	int Dequeue()
	{
		head_lock.lock();

		if (nullptr == head->next) {
			head_lock.unlock();
			return -1;
		}

		int v = head->next->key;
		NODE* e = head;

		head = head->next;
		head_lock.unlock();

		delete e;
		return v;
	}

	void print20()
	{
		for (int i = 0; i < 20; ++i) {
			int v = Dequeue();

			if (-1 == v) {
				break;
			}

			std::cout << v << ", ";
		}

		std::cout << std::endl;
	}
};

std::atomic_bool stop = false;

class LF_QUEUE {
	NODE* volatile head;
	NODE* volatile tail;
	std::mutex head_lock;

public:
	LF_QUEUE()
	{
		head = tail = new NODE{ -1 };
	}

	void clear()
	{
		while (-1 != Dequeue());
	}

	bool CAS(NODE* volatile* ptr, NODE* old_ptr, NODE* new_ptr)
	{
		return std::atomic_compare_exchange_strong(
			reinterpret_cast<volatile std::atomic_llong*>(ptr),
			reinterpret_cast<long long*>(&old_ptr),
			reinterpret_cast<long long>(new_ptr)
		);
	}

	void Enqueue(int x)
	{
		NODE* e = new NODE{ x };

		while (true) {
			auto last = tail;
			auto next = last->next;

			if (last != tail) {
				continue;
			}

			if (next != nullptr) {
				CAS(&tail, last, next);
			}

			if (true == CAS(&last->next, nullptr, e)) {
				CAS(&tail, last, e);
				return;
			}
		}
	}

	int Dequeue()
	{
		while (true) {
			auto first = head;
			auto next = first->next;
			auto last = tail;

			if (first != head) {
				continue;
			}

			if (nullptr == next) {
				return -1;
			}

			if (first == last) {
				CAS(&tail, last, next);
				continue;
			}

			auto v = next->key;

			if (true == CAS(&head, first, next)) {
				//delete first;
				return v;
			}
		}
	}

	void print20()
	{
		for (int i = 0; i < 20; ++i) {
			int v = Dequeue();

			if (-1 == v) {
				break;
			}

			std::cout << v << ", ";
		}

		std::cout << std::endl;
	}
};

class STNODE;

class STPTR {
	// NODE* volatile m_ptr; => 프로그래밍 시 실수 확률 매우 높음
	// int m_stamp;

public:
	std::atomic_llong m_stpr;

	STPTR(STNODE* p, int st) {
		set_ptr(p, st);
	}

	STPTR(const STPTR& new_v) {
		m_stpr = new_v.m_stpr.load();
	}

	void set_ptr(STNODE* p, int st) {
		long long t = reinterpret_cast<int>(p);
		m_stpr = (t << 32) + st;
	}

	long long get_value() {
		return m_stpr;
	}

	STNODE* get_ptr() {
		return reinterpret_cast<STNODE*>(m_stpr >> 32);
	}

	int get_stamp() {
		return m_stpr & 0xFFFFFFFF;
	}

	void copy(const STPTR& new_v) {
		m_stpr = new_v.m_stpr.load();
	}

	bool CAS(STNODE* old_ptr, STNODE* new_ptr, int old_st, int new_st)
	{
		long long old_v = reinterpret_cast<int>(old_ptr);
		old_v = (old_v << 32) + old_st;

		long long new_v = reinterpret_cast<int>(new_ptr);
		new_v = (new_v << 32) + new_st;

		return std::atomic_compare_exchange_strong(&m_stpr, &old_v, new_v);
	}
};

class STNODE {
public:
	int	key;
	STPTR next;

	STNODE(int x) : key(x), next(nullptr, 0) {}
};

class ST_LF_QUEUE {
	STPTR head{ nullptr, 0 }, tail{ nullptr, 0 };

public:
	ST_LF_QUEUE()
	{
		auto n = new STNODE{ -1 };
		head.set_ptr(n, 0);
		tail.set_ptr(n, 0);
	}

	void clear()
	{
		while (-1 != Dequeue());
	}

	void Enqueue(int x)
	{
		STNODE* e = new STNODE{ x };

		while (true) {
			STPTR last{ tail };
			STPTR next{ last.get_ptr()->next };

			if (last.get_ptr() != tail.get_ptr()) {
				continue;
			}

			if (next.get_ptr() != nullptr) {
				tail.CAS(last.get_ptr(), next.get_ptr(), last.get_stamp(), last.get_stamp() + 1);
				continue;
			}

			if (true == last.get_ptr()->next.CAS(nullptr, e, next.get_stamp(), next.get_stamp() + 1)) {
				tail.CAS(last.get_ptr(), e, last.get_stamp(), last.get_stamp() + 1);
				return;
			}
		}
	}

	int Dequeue()
	{
		while (true) {
			STPTR first{ head };
			STPTR next = { first.get_ptr()->next };
			STPTR last{ tail };

			if (first.get_ptr() != head.get_ptr()) {
				continue;
			}

			if (next.get_ptr() == nullptr) {
				return -1;
			}

			if (first.get_ptr() == last.get_ptr()) {
				tail.CAS(last.get_ptr(), next.get_ptr(), last.get_stamp(), last.get_stamp() + 1);
				continue;
			}

			auto v = next.get_ptr()->key;

			if (true == head.CAS(first.get_ptr(), next.get_ptr(), first.get_stamp(), first.get_stamp() + 1)) {
				delete first.get_ptr();
				return v;
			}
		}
	}


	void print20()
	{
		for (int i = 0; i < 20; ++i) {
			int v = Dequeue();

			if (-1 == v) {
				break;
			}

			std::cout << v << ", ";
		}

		std::cout << std::endl;
	}
};

ST_LF_QUEUE my_queue;

thread_local int thread_id;

const int NUM_TEST = 10000000;

std::atomic_int loop_count = NUM_TEST;

void benchmark(const int th_id, const int num_thread)
{
	int key = 0;

	thread_id = th_id;

	while (loop_count-- > 0) {
		if ((rand() % 2 == 0)) {
			my_queue.Enqueue(key++);
		}

		else {
			my_queue.Dequeue();
		}
	}
}


int main()
{
	using namespace std::chrono;

	for (int n = 1; n <= MAX_THREADS; n = n * 2) {
		loop_count = NUM_TEST;
		my_queue.clear();

		std::vector<std::thread> tv;
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

		std::cout << n << " Threads,  " << ms << "ms.";
		my_queue.print20();
	}
}