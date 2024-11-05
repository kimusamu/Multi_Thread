#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <random>
#include <array>

class NODE
{
public:
	int key;
	NODE* volatile next;
	bool marked;
	std::mutex n_lock;

	NODE(int x) : key(x), next(nullptr){}

	void lock() {
		n_lock.lock();
	}

	void unlock() {
		n_lock.unlock();
	}
};

class DUMMYMUTEX
{
public:
	void lock() {
	}

	void unlock() {
	}
};

class C_SET
{
	NODE head{ (int)(0x80000000) }, tail{ (int)(0x7FFFFFFF) };
	std::mutex set_lock;

public:
	C_SET() {
		head.next = &tail;
	}

	void clear() {
		while (head.next != &tail) {
			auto p = head.next;
			head.next = head.next->next;
			delete p;
		}
	}

	bool Add(int x) {
		auto p = new NODE{ x };
		auto prev = &head;

		set_lock.lock();

		auto curr = prev->next;
		
		while (curr->key < x) {
			prev = curr;
			curr = curr->next;
		}

		if (curr->key == x) {
			set_lock.unlock();
			delete p;

			return false;
		}

		else {
			// auto p = new NODE{ x };
			p->next = curr;
			prev->next = p;
			set_lock.unlock();

			return true;
		}
	}

	bool Remove(int x) {
		auto prev = &head;

		set_lock.lock();

		auto curr = prev->next;

		while (curr->key < x) {
			prev = curr;
			curr = curr->next;
		}

		if (curr->key == x) {
			prev->next = curr->next;
			delete curr;
			set_lock.unlock();

			return true;
		}

		else {
			set_lock.unlock();

			return false;
		}
	}

	bool Contains(int x) {
		auto prev = &head;
		
		set_lock.lock();

		auto curr = prev->next;

		while (curr->key < x) {
			prev = curr;
			curr = curr->next;
		}

		if (curr->key == x) {
			set_lock.unlock();

			return true;
		}
		
		else {
			set_lock.unlock();

			return false;
		}
	}

	void print20() {
		auto p = head.next;

		for (int i = 0; i < 20; ++i) {
			if (p == &tail) {
				break;
			}

			std::cout << p->key << ", ";
			p = p->next;
		}

		std::cout << std::endl;
	}
};

class F_SET
{
	NODE head{ (int)(0x80000000) }, tail{ (int)(0x7FFFFFFF) };

public:
	F_SET() {
		head.next = &tail;
	}

	void clear() {
		while (head.next != &tail) {
			auto p = head.next;
			head.next = head.next->next;
			delete p;
		}
	}

	bool Add(int x) {
		auto p = new NODE{ x };
		// head.lock();
		
		auto prev = &head;
		prev->lock();

		auto curr = prev->next;
		curr->lock();

		auto curr = prev->next;

		while (curr->key < x) {
			prev->unlock();

			prev = curr;
			curr = curr->next;

			curr->unlock();
		}

		if (curr->key == x) {
			prev->unlock();
			curr->unlock();

			delete p;

			return false;
		}

		else {
			p->next = curr;
			prev->next = p;

			prev->unlock();
			curr->unlock();

			return true;
		}
	}

	bool Remove(int x) {
		// head.lock();
		auto prev = &head;
		prev->lock();

		auto curr = prev->next;
		curr->lock();

		while (curr->key < x) {
			prev->unlock();

			prev = curr;
			curr = curr->next;

			curr->unlock();
		}

		if (curr->key == x) {
			prev->next = curr->next;
			delete curr;

			prev->unlock();

			return true;
		}

		else {
			prev->unlock();
			curr->unlock();

			return false;
		}
	}

	bool Contains(int x) {
		// head.lock();
		auto prev = &head;
		prev->lock();

		auto curr = prev->next;
		curr->lock();

		while (curr->key < x) {
			prev->lock();

			prev = curr;
			curr = curr->next;

			curr->lock();
		}

		if (curr->key == x) {
			prev->unlock();
			curr->unlock();

			return true;
		}

		else {
			prev->unlock();
			curr->unlock();

			return false;
		}
	}

	void print20() {
		auto p = head.next;

		for (int i = 0; i < 20; ++i) {
			if (p == &tail) {
				break;
			}

			std::cout << p->key << ", ";
			p = p->next;
		}

		std::cout << std::endl;
	}
};

class O_SET
{
	NODE head{ (int)(0x80000000) }, tail{ (int)(0x7FFFFFFF) };

public:
	O_SET() {
		head.next = &tail;
	}

	void clear() {
		while (head.next != &tail) {
			auto p = head.next;
			head.next = head.next->next;
			delete p;
		}
	}

	bool validate(const int x, NODE* prev, NODE* curr) {
		auto p = &head;
		auto c = p->next;

		while (c->key < x) {
			p = c;
			c = c->next;
		}

		return ((prev == p) && (curr == c));
	}

	bool Add(int x) {
		auto p = new NODE{ x };

		while (true) {
			auto prev = &head;
			auto curr = prev->next;
			
			while (curr->key < x) {
				prev = curr;
				curr = curr->next;
			}

			prev->lock();
			curr->lock();
			
			if (false == validate(x, prev, curr)) {
				prev->unlock();
				curr->unlock();

				continue;
			}

			if (curr->key == x) {
				prev->unlock();
				curr->unlock();

				delete p;

				return false;
			}

			else {
				p->next = curr;
				prev->next = p;

				prev->unlock();
				curr->unlock();

				return true;
			}
		}
	}

	bool Remove(int x) {
		auto p = new NODE{ x };

		while (true) {
			auto prev = &head;
			auto curr = prev->next;

			while (curr->key < x) {
				prev = curr;
				curr = curr->next;
			}

			prev->lock();
			curr->lock();

			if (false == validate(x, prev, curr)) {
				prev->unlock();
				curr->unlock();

				continue;
			}

			if (curr->key == x) {
				prev->next = curr->next;

				prev->unlock();
				curr->unlock();

				return true;
			}

			else {
				prev->unlock();
				curr->unlock();

				return false;
			}
		}
	}

	bool Contains(int x) {
		auto p = new NODE{ x };

		while (true) {
			auto prev = &head;
			auto curr = prev->next;

			while (curr->key < x) {
				prev = curr;
				curr = curr->next;
			}

			prev->lock();
			curr->lock();

			if (false == validate(x, prev, curr)) {
				prev->unlock();
				curr->unlock();

				continue;
			}

			if (curr->key == x) {
				prev->unlock();
				curr->unlock();

				return true;
			}

			else {
				prev->unlock();
				curr->unlock();

				return false;
			}
		}
	}

	void print20() {
		auto p = head.next;

		for (int i = 0; i < 20; ++i) {
			if (p == &tail) {
				break;
			}

			std::cout << p->key << ", ";
			p = p->next;
		}

		std::cout << std::endl;
	}
};


const int NUM_TEST = 4000000;
const int KEY_RANGE = 1000;

C_SET my_set;
// F_SET my_set;
// O_SET my_set;

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

void worker_check(int num_threads, int thread_id)
{
	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		int op = rand() % 3;

		switch (op) {
		case 0: {
			int v = rand() % KEY_RANGE;
			history[thread_id].emplace_back(0, v, my_set.Add(v));
			break;
		}

		case 1: {
			int v = rand() % KEY_RANGE;
			history[thread_id].emplace_back(1, v, my_set.Remove(v));
			break;
		}

		case 2: {
			int v = rand() % KEY_RANGE;
			history[thread_id].emplace_back(2, v, my_set.Contains(v));
			break;
		}

		}
	}
}

void check_history(int num_threads)
{
	std::array<int, KEY_RANGE> survive = {};
	std::cout << "Checking Consistency : ";

	if (history[0].size() == 0) {
		std::cout << "No History.\n";
		return;
	}

	for (int i = 0; i < num_threads; ++i) {
		for (auto& op : history[i]) {
			if (false == op.o_value) {
				continue;
			}

			if (op.op == 3) {
				continue;
			}

			if (op.op == 0) {
				survive[op.i_value]++;
			}

			if (op.op == 1) {
				survive[op.i_value]--;
			}
		}
	}

	for (int i = 0; i < KEY_RANGE; ++i) {
		int val = survive[i];

		if (val < 0) {
			std::cout << "ERROR. The value " << i << " removed while it is not in the set.\n";
			exit(-1);
		}

		else if (val > 1) {
			std::cout << "ERROR. The value " << i << " removed while it is not in the set.\n";
			exit(-1);
		}

		else if (val == 0) {
			if (my_set.Contains(i)) {
				std::cout << "ERROR. The value " << i << " removed while it is not in the set.\n";
				exit(-1);
			}
		}

		else if (val == 1) {
			if (false == my_set.Contains(i)) {
				std::cout << "ERROR. The value " << i << " removed while it is not in the set.\n";
				exit(-1);
			}
		}
	}

	std::cout << " OK\n";
}

void benchmark(const int num_thread) 
{
	int key;

	for (int i = 0; i < NUM_TEST / num_thread; ++i) {
		switch (rand() % 3) {
		case 0:
			key = rand() % KEY_RANGE;
			my_set.Add(key);
			break;

		case 1:
			key = rand() % KEY_RANGE;
			my_set.Remove(key);
			break;

		case 2:
			key = rand() % KEY_RANGE;
			my_set.Contains(key);
			break;

		default:
			std::cout << "Error\n";
			exit(-1);
		}
	}
}

int main()
{
	using namespace std::chrono;

	for (int n = 1; n <= 16; n = n * 2) {
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
		
		auto ent_t = high_resolution_clock::now();
		auto exec_t = ent_t - start_t;

		size_t ms = duration_cast<milliseconds>(exec_t).count();

		std::cout << n << " Threads, " << ms << "ms. ";

		my_set.print20();
		check_history(n);
	}
}