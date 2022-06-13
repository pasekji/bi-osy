#ifndef __PROGTEST__
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <climits>
#include <cfloat>
#include <cassert>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <string>
#include <vector>
#include <array>
#include <iterator>
#include <set>
#include <list>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <stack>
#include <deque>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <pthread.h>
#include <semaphore.h>
#include "progtest_solver.h"
#include "sample_tester.h"
using namespace std;
#endif /* __PROGTEST__ */

template <class T> class threadQ
{
public:
	threadQ() = default;
	~threadQ() = default;

	void enqueue(T t);
	T dequeue();

private:
	queue<T> q;
	mutable mutex m;
	condition_variable c;
};

template <class T> void threadQ<T>::enqueue(T t)
{
	lock_guard<mutex> lock(m);
	q.push(t);
	c.notify_one();
}

template <class T> T threadQ<T>::dequeue()
{
	unique_lock<mutex> lock(m);
	while (q.empty())
	{
		c.wait(lock);
	}
	T val = q.front();
	q.pop();
	return val;
}

class Order
{
	public:
		Order();
		const int getCustomersServed() const;
		void incCustomersServed();
		const int getCustomers() const;
		void setCustomers(int n);
		const int getOrderNumber() const;
		void setOrderNumber(int n);
		AShip& getShip();
		void setShip(AShip s);
		void addCargo(CCargo c);
		vector<CCargo>& getCargo();
		mutable mutex m;

	private:
		vector<CCargo> cargo;
		AShip ship;
		int orderNumber;
		int nCustomers;
		int nCustomersServed;
};

Order::Order()
{
	nCustomersServed = 0;
}

const int Order::getCustomersServed() const
{
	return nCustomersServed;
}

void Order::incCustomersServed() 
{
	nCustomersServed++;
}

const int Order::getCustomers() const
{
	return nCustomers;
}

void Order::setCustomers(int n)
{
	nCustomers = n;
}

const int Order::getOrderNumber() const
{
	return orderNumber;
}

void Order::setOrderNumber(int n)
{
	orderNumber = n;
}

AShip& Order::getShip()
{
	return ship;
}

void Order::setShip(AShip s)
{
	ship = s;
}

void Order::addCargo(CCargo c)
{
	cargo.push_back(c);
}

vector<CCargo>& Order::getCargo()
{
	return cargo;
}

class CCargoPlanner
{
private:
	vector<ACustomer> customers;
	vector<thread*> sellerThreads;
	vector<thread*> workerThreads;

public:
	threadQ<Order*> workQueue;
	threadQ<pair<Order*,ACustomer>> ordersQueue;

	static int SeqSolver(const vector<CCargo>& cargo, int maxWeight, int maxVolume, vector<CCargo>& load);
	void Start(int sales, int workers);
	void Stop(void);
	void Customer(ACustomer customer);
	void Ship(AShip ship);

};

void sellerThread(CCargoPlanner* planner);
void workThread(CCargoPlanner* planner);

int CCargoPlanner::SeqSolver(const vector<CCargo>& cargo, int maxWeight, int maxVolume, vector<CCargo>& load)
{
	int res = ProgtestSolver(cargo, maxWeight, maxVolume, load);
	return res;
}

void CCargoPlanner::Start(int sales, int workers)
{
	for (int i = 0; i < sales; i++)
		sellerThreads.push_back(new thread(sellerThread, this));
	for (int i = 0; i < workers; i++)
		workerThreads.push_back(new thread(workThread, this));
}

void CCargoPlanner::Stop()
{
	for (unsigned int i = 0; i < sellerThreads.size(); i++)
	{
		Order* nullOrder = nullOrder;
		ordersQueue.enqueue(make_pair(nullptr,nullptr));
	}
	for (auto& seller : sellerThreads)
	{
		seller->join();
		delete seller;
	}
	for (unsigned int i = 0; i < workerThreads.size(); i++)
		workQueue.enqueue(nullptr);
	for (auto& worker : workerThreads)
	{
		worker->join();
		delete worker;
	}
}

void CCargoPlanner::Customer(ACustomer customer)
{
	customers.push_back(customer);
}

void CCargoPlanner::Ship(AShip ship)
{
	Order* order = new Order();
	order->setShip(ship);
	order->setCustomers(customers.size());
	for (ACustomer customer : customers)
		ordersQueue.enqueue(make_pair(order,customer));
}

void sellerThread(CCargoPlanner* planner)
{
	pair<Order*,ACustomer> orderCustomer;
	while ((orderCustomer = planner->ordersQueue.dequeue()).first)
	{
		vector<CCargo> tmpCargo;
		orderCustomer.second->Quote(orderCustomer.first->getShip()->Destination(), tmpCargo);
		bool isLast = false;
		{
			lock_guard<mutex> lock(orderCustomer.first->m);
			for (CCargo& cargo : tmpCargo)
				orderCustomer.first->addCargo(cargo);
			orderCustomer.first->incCustomersServed();
			isLast = orderCustomer.first->getCustomersServed() == orderCustomer.first->getCustomers();
		}
		if (isLast)
			planner->workQueue.enqueue(orderCustomer.first);
	}
}

void workThread(CCargoPlanner* planner)
{
	Order* order;
	while ((order = planner->workQueue.dequeue()))
	{
		vector<CCargo> load;
		CCargoPlanner::SeqSolver(order->getCargo(), order->getShip()->MaxWeight(), order->getShip()->MaxVolume(), load);
		order->getShip()->Load(load);
		delete order;
	}
}

// TODO: CCargoPlanner implementation goes here
//-------------------------------------------------------------------------------------------------
#ifndef __PROGTEST__
int                main                                    ( void )
{
  CCargoPlanner  test;
  vector<AShipTest> ships;
  vector<ACustomerTest> customers { make_shared<CCustomerTest> (), make_shared<CCustomerTest> () };
  
  ships . push_back ( g_TestExtra[0] . PrepareTest ( "New York", customers ) );
  ships . push_back ( g_TestExtra[1] . PrepareTest ( "Barcelona", customers ) );
  ships . push_back ( g_TestExtra[2] . PrepareTest ( "Kobe", customers ) );
  ships . push_back ( g_TestExtra[8] . PrepareTest ( "Perth", customers ) );
  // add more ships here
  
  for ( auto x : customers )
    test . Customer ( x );
  
  test . Start ( 3, 2 );
  
  for ( auto x : ships )
    test . Ship ( x );

  test . Stop  ();

  for ( auto x : ships )
    cout << x -> Destination () << ": " << ( x -> Validate () ? "ok" : "fail" ) << endl;
  return 0;  
}
#endif /* __PROGTEST__ */ 
