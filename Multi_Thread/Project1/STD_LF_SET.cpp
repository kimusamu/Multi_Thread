#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <random>
#include <array>
#include <set>
#include <queue>

constexpr int MAX_THREADS = 32;

class LFNODE;

class SPTR
{
	std::atomic<long long> sptr;

public:
	SPTR() : sptr(0) {}

	void set_ptr(LFNODE* ptr) {
		sptr = reinterpret_cast<long long>(ptr);
	}

	LFNODE* get_ptr() {
		return reinterpret_cast<LFNODE*>(sptr & 0xFFFFFFFFFFFFFFFE);
	}

	LFNODE* get_ptr(bool* removed) {
		long long p = sptr;
		*removed = (1 == (p & 1));

		return reinterpret_cast<LFNODE*>(p & 0xFFFFFFFFFFFFFFFE);
	}

	bool get_removed() {
		return (1 == (sptr & 1));
	}

	bool CAS(LFNODE* old_p, LFNODE* new_p, bool old_m, bool new_m) {
		long long old_v = reinterpret_cast<long long>(old_p);

		if (true == old_m) {
			old_v = old_v | 1;
		}

		else {
			old_v = old_v & 0xFFFFFFFFFFFFFFFE;
		}

		long long new_v = reinterpret_cast<long long>(new_p);

		if (true == new_m) {
			new_v = new_v | 1;
		}

		else {
			new_v = new_v & 0xFFFFFFFFFFFFFFFE;
		}

		return std::atomic_compare_exchange_strong(&sptr, &old_v, new_v);
	}
};

class LFNODE
{
public:
	int key;
	SPTR next;
	int ebr_number;

	LFNODE(int v) : key(v), ebr_number(0) {}
};

thread_local std::queue<LFNODE*> m_free_queue;
thread_local int thread_id;

struct RESPONSE {
	bool m_bool;
};

enum METHOD {
	M_ADD, M_REMOVE, M_CONTAINS, M_CLEAR, M_PRINT20
};

struct INVOCATION {
	METHOD m_method;
	int x;
};

class SEQOBJECT {
	std::set <int> m_set;

public:
	SEQOBJECT()
	{
	}


	void clear() {
		m_set.clear();
	}

	RESPONSE apply(INVOCATION& inv)
	{
		RESPONSE r{ true };

		switch (inv.m_method) {
		case M_ADD:
			r.m_bool = m_set.insert(inv.x).second;
			break;

		case M_REMOVE:
			r.m_bool = (0 != m_set.count(inv.x));

			if (r.m_bool == true) {
				m_set.erase(inv.x);
			}
			break;

		case M_CONTAINS:
			r.m_bool = (0 != m_set.count(inv.x));
			break;

		case M_CLEAR:
			m_set.clear();
			break;

		case M_PRINT20: {
			int count = 20;
			for (auto x : m_set) {
				if (count-- == 0) {
					break;
				}

				std::cout << x << ", ";
			}

			std::cout << std::endl;
		}
					  break;
		}

		return r;
	}
};

struct U_NODE {
	INVOCATION m_inv;
	int m_seq;
	U_NODE* volatile next;
};

class LFUNV_OBJECT {
	U_NODE* volatile m_head[MAX_THREADS];
	U_NODE tail;

	U_NODE* get_max_head()
	{
		U_NODE* h = m_head[0];

		for (int i = 1; i < MAX_THREADS; ++i) {
			if (m_head[i] && h->m_seq < m_head[i]->m_seq) {
				h = m_head[i];
			}
		}

		return h;
	}

public:
	LFUNV_OBJECT()
	{
		tail.m_seq = 0;
		tail.next = nullptr;

		for (auto& h : m_head) {
			h = &tail;
		}
	}

	void clear()
	{
		U_NODE* p = tail.next;

		while (nullptr != p) {
			U_NODE* old_p = p;
			p = p->next;
			delete old_p;
		}

		tail.next = nullptr;

		for (auto& h : m_head) {
			h = &tail;
		}
	}

	RESPONSE apply(INVOCATION& inv)
	{
		U_NODE* prefer = new U_NODE{ inv, 0, nullptr };
		int retry = 0;

		while (0 == prefer->m_seq) {
			if (retry++ > 100) {
				std::cerr << "Retry limit reached. \n";
				delete prefer;
				return { false };
			} // 만약 retry를 통해서 100번 넘게 무한루프시 해당 노드를 터치고 새로 구현
			// 더 좋은 방법이 있을텐데

			U_NODE* head = get_max_head();
			long long temp = 0;

			bool updated = std::atomic_compare_exchange_strong(
				reinterpret_cast<volatile std::atomic_llong*>(&head->next),
				&temp,
				reinterpret_cast<long long>(prefer)
			);

			if (updated) {
				U_NODE* after = head->next;
				after->m_seq = head->m_seq + 1;
				m_head[thread_id] = after;
			}
		}

		SEQOBJECT std_set;
		U_NODE* p = tail.next;

		while (p != prefer) {
			std_set.apply(p->m_inv);
			p = p->next;
		}

		return std_set.apply(inv);

	}
};

class STD_LF_SET {
	LFUNV_OBJECT m_set;

public:
	STD_LF_SET()
	{
	}

	void clear()
	{
		m_set.clear();
	}

	bool Add(int x)
	{
		INVOCATION inv{ M_ADD, x };
		return m_set.apply(inv).m_bool;
	}

	bool Remove(int x)
	{
		INVOCATION inv{ M_REMOVE, x };
		return m_set.apply(inv).m_bool;
	}

	bool Contains(int x)
	{
		INVOCATION inv{ M_CONTAINS, x };
		return m_set.apply(inv).m_bool;
	}

	void print20()
	{
		INVOCATION inv{ M_PRINT20, 0 };
		m_set.apply(inv);
	}
};

STD_LF_SET my_set;

const int NUM_TEST = 40000;
const int KEY_RANGE = 1000;

class HISTORY
{
public:
	int op;
	int i_value;
	bool o_value;

	HISTORY(int o, int i, bool re) : op(o), i_value(i), o_value(re) {

	}
};

std::array<std::vector<HISTORY>, 16> history;

void worker_check(int num_threads, int th_id)
{
	thread_id = th_id;

	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		int op = rand() % 3;

		switch (op) {
		case 0: {
			int v = rand() % KEY_RANGE;
			history[th_id].emplace_back(0, v, my_set.Add(v));
			break;
		}

		case 1: {
			int v = rand() % KEY_RANGE;
			history[th_id].emplace_back(1, v, my_set.Remove(v));
			break;
		}

		case 2: {
			int v = rand() % KEY_RANGE;
			history[th_id].emplace_back(2, v, my_set.Contains(v));
			break;
		}
		}
	}

	while (false == m_free_queue.empty()) {
		auto p = m_free_queue.front();
		m_free_queue.pop();
		delete p;
	}
}

void check_history(int num_threads)
{
	std::array <int, KEY_RANGE> survive = {};
	std::cout << "Checking Consistency : ";
	if (history[0].size() == 0) {
		std::cout << "No history.\n";
		return;
	}
	for (int i = 0; i < num_threads; ++i) {
		for (auto& op : history[i]) {
			if (false == op.o_value) continue;
			if (op.op == 3) continue;
			if (op.op == 0) survive[op.i_value]++;
			if (op.op == 1) survive[op.i_value]--;
		}
	}
	for (int i = 0; i < KEY_RANGE; ++i) {
		int val = survive[i];
		if (val < 0) {
			std::cout << "ERROR. The value " << i << " removed while it is not in the set.\n";
			exit(-1);
		}
		else if (val > 1) {
			std::cout << "ERROR. The value " << i << " is added while the set already have it.\n";
			exit(-1);
		}
		else if (val == 0) {
			if (my_set.Contains(i)) {
				std::cout << "ERROR. The value " << i << " should not exists.\n";
				exit(-1);
			}
		}
		else if (val == 1) {
			if (false == my_set.Contains(i)) {
				std::cout << "ERROR. The value " << i << " shoud exists.\n";
				exit(-1);
			}
		}
	}
	std::cout << " OK\n";
}

int main()
{
	using namespace std::chrono;

	for (int n = 1; n <= MAX_THREADS; n = n * 2) {
		my_set.clear();

		for (auto& v : history) {
			v.clear();
		}

		std::vector<std::thread> tv;
		auto start_t = high_resolution_clock::now();

		for (int i = 0; i < n; ++i) {
			tv.emplace_back(worker_check, n, i);
		}

		for (auto& th : tv) {
			th.join();
		}
		auto end_t = high_resolution_clock::now();
		auto exec_t = end_t - start_t;
		size_t ms = duration_cast<milliseconds>(exec_t).count();

		std::cout << n << " Threads,  " << ms << "ms.";

		check_history(n);
		my_set.print20();
	}
}