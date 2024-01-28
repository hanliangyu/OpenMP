//
// Created by Han on 2024/1/28.
//

#ifndef OPENMP_PARALLELLIB_H
#define OPENMP_PARALLELLIB_H

#include <iostream>
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

template<typename T>
class ThreadSafeQueue{
public:
    ThreadSafeQueue()= default;;
    ThreadSafeQueue(ThreadSafeQueue const& other){
        std::lock_guard<std::mutex>lk(m_mutex);
        m_q = other.m_q;
    }
    void push_back(T data){
        std::lock_guard<std::mutex>lk(m_mutex);
        m_q.push_back(data);
        m_cv.notify_one();
    }
    void wait_and_pop(T& value){
        std::unique_lock<std::mutex>lk(m_mutex);
        m_cv.wait(lk, [this]{return !m_q.empty();});
        value = m_q.front();
        m_q.pop_front();
    }
    bool try_pop(T& value){
        std::lock_guard<std::mutex>lk(m_mutex);
        if(m_q.empty()){
            return false;
        }
        value = m_q.front();
        m_q.pop_front();
        return true;
    }
    bool empty(){
        std::lock_guard<std::mutex>lk(m_mutex);
        return m_q.empty();
    }

private:
    mutable std::mutex m_mutex;
    std::deque<T> m_q;
    std::condition_variable m_cv;
};



#endif //OPENMP_PARALLELLIB_H
