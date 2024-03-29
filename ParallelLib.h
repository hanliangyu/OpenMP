//
// Created by Han on 2024/1/28.
//

#ifndef OPENMP_PARALLELLIB_H
#define OPENMP_PARALLELLIB_H

#include <deque>
#include <chrono>
#include <memory>
#include <mutex>
#include <future>
#include <string>
#include <condition_variable>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <numeric>

// Simple thread safe queue with lock and conditional variable
template<typename T>
class ThreadSafeQueue{
public:
    ThreadSafeQueue()= default;;
    ThreadSafeQueue(ThreadSafeQueue const& other){
        std::lock_guard<std::mutex>lk(m_mutex);
        m_q = other.m_q;
    }
    // locks the queue and push
    void push_back(T data){
        std::lock_guard<std::mutex>lk(m_mutex);
        m_q.push_back(data);
        m_cv.notify_one();
    }
    // the thread calling this will be blocked until !queue.empty()
    void wait_and_pop(T& value){
        std::unique_lock<std::mutex>lk(m_mutex);
        m_cv.wait(lk, [this]{return !m_q.empty();});
        value = m_q.front();
        m_q.pop_front();
    }
    // returns immediately, value only valid when TRUE is returned
    bool try_pop(T& value){
        std::lock_guard<std::mutex>lk(m_mutex);
        if(m_q.empty()){
            return false;
        }
        value = m_q.front();
        m_q.pop_front();
        return true;
    }
    // locks the queue and check empty
    bool empty(){
        std::lock_guard<std::mutex>lk(m_mutex);
        return m_q.empty();
    }

private:
    mutable std::mutex m_mutex;
    std::deque<T> m_q;
    std::condition_variable m_cv;
};

class Module{
public:
    void Run(int threadIndex){
        int time = static_cast<int>(rand() / double(RAND_MAX) * 500);
        std::this_thread::sleep_for(std::chrono::microseconds (time));
        //std::string tmp = "Thread id " + std::to_string(threadIndex) + " sleeps for " + std::to_string(time) + " ms";
        //std::cout << tmp << std::endl;
    }
};

class ThreadPool {
public:
    explicit ThreadPool() : running(true){}

    void registerModule(Module* module){
        threads.emplace_back(&ThreadPool::threadProcess, this, module, count);
        signals.emplace_back(false);
        count++;
    }

    void run() {
        // initiate threads
        for(auto& state: signals){
            state.store(true, std::memory_order_release);
        }
        barrier();
    }

    ~ThreadPool() {
        running = false;
        barrier();
        for (std::thread& thread : threads) {
            thread.join(); // Wait for all threads to exit
        }
    }

private:
    void barrier(){
        // Sets a barrier here to sync all threads
        std::unique_lock<std::mutex>lk(mutex);
        cv.wait(lk, [this]{
            return !std::any_of(signals.begin(),
                               signals.end(),
                               [](std::atomic<bool> &m_state){return m_state.load(std::memory_order_acquire);});
        });
    }

    void threadProcess(Module* module, int threadIndex) {
        while(true) {
            if (!running) {
                return;
            }
            if(signals[threadIndex].load(std::memory_order_acquire)){
                module->Run(threadIndex);
                signals[threadIndex].store(false, std::memory_order_release);
                cv.notify_all();
            }
        }
    }

private:
    std::vector<std::thread> threads;
    std::deque<std::atomic<bool>> signals;
    std::atomic<bool> running{true};
    std::condition_variable cv;
    std::mutex mutex;
    int count = 0;
};

struct SpinLock {
    int retries{0};
    std::atomic<bool> lock_ = {false};
    void lock() {
        for (;;) {
            retries = 0;
            if (!lock_.exchange(true, std::memory_order_acquire)) {
                break;
            }
            while (lock_.load(std::memory_order_relaxed)) {
                //std::this_thread::yield();
                backoff();
            }
        }
    }
    void backoff() const {
        const int max_retries = 50;
        if (retries < max_retries) {
            std::this_thread::yield();
        } else {
            auto delay = std::chrono::microseconds(1 << (retries - max_retries));
            std::this_thread::sleep_for(delay);
        }
    }

    void unlock() { lock_.store(false, std::memory_order_release); }
};

class ThreadPoolOp {
public:
    explicit ThreadPoolOp() : running(true){}

    void registerModule(Module* module){
        signals.emplace_back(0);
        threads.emplace_back(&ThreadPoolOp::threadProcess, this, module, count);
        count++;
    }

    void run() {
        // initiate threads
        assert(sum.load() == 0);
        sum.store(count, std::memory_order_release);

        std::unique_lock<std::mutex>lk2(mutex);
        std::fill(signals.begin(), signals.begin()+count, 1);
        flag = true;
        cv.notify_all();

        while(flag){
            cv.wait(lk2);
        }
        //std::cout << "=================" << std::endl;
    }

    ~ThreadPoolOp() {
        running = false;
        std::fill(signals.begin(), signals.begin()+count, 1);
        cv.notify_all();
        for (std::thread& thread : threads) {
            thread.join(); // Wait for all threads to exit
        }
    }

private:
    void threadProcess(Module* module, int threadIndex) {
        while(true) {
            std::unique_lock<std::mutex> lk2(mutex);
            while(!signals[threadIndex]){
                cv.wait(lk2);
            }
            signals[threadIndex] = false;
            lk2.unlock();

            if (!running) {
                return;
            }

            module->Run(threadIndex);
            int before = sum.fetch_sub(1, std::memory_order_release);
            if (before == 1) {
                {
                    lk2.lock();
                    flag = false;
                    cv.notify_all();
                }
            }
        }
    }

private:
    std::vector<std::thread> threads;
    std::vector<int> signals; // 0 --> pause, 1 --> start
    std::atomic<bool> running{true};
    SpinLock lk;
    int count = 0;
    std::atomic<int> sum{0};
    std::condition_variable cv;
    std::mutex mutex;
    bool flag = false;
};

#endif //OPENMP_PARALLELLIB_H
