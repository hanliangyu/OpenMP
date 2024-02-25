//
// Created by Han on 2024/1/11.
//

#ifndef OPENMP_THREADEDDUMP_H
#define OPENMP_THREADEDDUMP_H

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

#define CONFIG_FILE_PATH "dump_config.txt"

namespace TD{
    struct TDQueue;
    class ThreadedDumpPool;
    extern TDQueue m_dump_queue;
    extern std::mutex m_dump_mutex;
    extern std::condition_variable m_dump_cv;
    extern bool m_dump_terminate;

    struct TDQueue: public std::deque<std::pair<std::string, std::string>>{
    public:
        void safe_emplace_back(const std::pair<std::string, std::string>& data){
            while(true){
                std::lock_guard<std::mutex>lk(m_q_mutex);
                if(this->size() <= 500){
                    // If the dump thread is slow, stall the main thread
                    this->emplace_back(data);
                    return;
                }
            }
        }
        void safe_pop_front(){
            std::lock_guard<std::mutex>lk(m_q_mutex);
            this->pop_front();
        }
    private:
        std::mutex m_q_mutex;
    };

    class ThreadedDump{
    public:
        void init();
    private:
        static bool loadConfigFile(std::unordered_set<std::string> &active_files){
            std::ifstream config_reader(CONFIG_FILE_PATH);
            if(config_reader.fail()){
                // no config file
                std::cout << "No config file found, thread exits!" << "\n";
                return false;
            }
            else{
                std::istringstream ss;
                std::string line, file_name, status;
                while(std::getline(config_reader, line)){
                    ss.clear();
                    ss.str(line);
                    if(ss >> file_name >> status && active_files.count(file_name) == 0){
                        std::cout << file_name << " " << status << "\n";
                        active_files.insert(file_name);
                    }
                    else{
                        std::cout << "Unmatched line" << "\n";
                    }
                }
            }
            return true;
        }
        std::map<std::string, std::ofstream> m_stream_map;
        uint64_t m_id = 0;
    };

    class ThreadedDumpPool{
    public:
        ThreadedDumpPool(){
            for(int i = 0; i< thread_num; i++){
                m_pool.emplace_back(&TD::ThreadedDump::init, TD::ThreadedDump());
            }
        }
        static ThreadedDumpPool* get(){
            std::call_once(m_flag, ThreadedDumpPool::init);
            return m_instance.get();
        }
        void registerThreadID(uint64_t id){
            std::lock_guard<std::mutex>lk(m_pool_mutex);
            m_thread_id.push_back(id);
            std::cout << "Register thread id: " << id << std::endl;
        }
        void submit(const std::string& dir, const std::string& text){
            if(m_DtoT_mapping.count(dir) == 0){
                // new file
                m_file_count++;
                m_DtoT_mapping.insert({dir, m_file_count % thread_num}); // round-robin allocate
            }

        }
        using threadJobType = std::pair<std::string, std::string>;
        threadJobType queryJob(uint64_t thread_id){ // each thread query for jobs

        }
    private:
        static void init(){
            m_instance.reset(new ThreadedDumpPool());
        }
        static const int thread_num = 4;
        static std::once_flag m_flag;
        static std::unique_ptr<ThreadedDumpPool> m_instance;
        std::vector<std::thread>m_pool;
        std::vector<uint64_t>m_thread_id;
        std::unordered_map<std::string, int> m_DtoT_mapping;
        std::mutex m_pool_mutex;
        int m_file_count = 0;
    };

}

/// Multiple writers from multiple threads to multiple readers on multiple threads
namespace PTD{ // parallel thread dump
    const int thread_num = 3;

    struct DumpQueue{
    public:
        void dump(){
            std::lock_guard<std::mutex>lg(m_q_mutex);
            while(!m_q.empty()){

                m_q.pop_front();
            }
        }
        bool empty(){
            std::lock_guard<std::mutex>lg(m_q_mutex);
            return m_q.empty();
        }
    private:
        std::deque<std::pair<std::string, std::string>> m_q;
        std::mutex m_q_mutex;
        std::condition_variable m_q_cv;
    };

    class ThreadedDumpPool{
    public:
        ThreadedDumpPool(): m_running(true){
            for(int i = 0; i < PTD::thread_num; i++){
                m_threads.emplace_back(&ThreadedDumpPool::threadProcess, i);
            }
        }

    private:
        void threadProcess(int threadID){
            while(true){
                std::unique_lock<std::mutex> lk(m_mutex);
                m_cv.wait(lk, !m_qs[threadID].empty());
                if(!m_running) return;
                lk.unlock();

                m_qs[threadID].dump(); // threadID access is unique to each thread

            }
        }
        std::vector<std::thread> m_threads;
        std::vector<DumpQueue> m_qs;
        std::mutex m_mutex;
        std::condition_variable m_cv;
        bool m_running;
    };
}

#endif //OPENMP_THREADEDDUMP_H
