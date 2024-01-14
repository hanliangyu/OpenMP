//
// Created by Han on 2024/1/11.
//

#ifndef OPENMP_THREADEDDUMP_H
#define OPENMP_THREADEDDUMP_H

#include <iostream>
#include <deque>
#include <chrono>
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
    extern TDQueue m_dump_queue;
    extern std::mutex m_dump_mutex;
    extern std::condition_variable m_dump_cv;
    extern bool m_dump_terminate;

    struct TDQueue: public std::deque<std::pair<std::string, std::string>>{
    public:
        void safe_emplace_back(const std::pair<std::string, std::string>& data){
            std::lock_guard<std::mutex>lk(m_q_mutex);
            this->emplace_back(data);
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
        void init(std::promise<std::unordered_set<std::string>>&& check_dump_config){
            std::unordered_set<std::string> active_files;
            if(!loadConfigFile(active_files)){
                check_dump_config.set_value(active_files);
                return;
            }
            check_dump_config.set_value(active_files);
            m_stream_map.reserve(100);
            while(true){
                std::unique_lock<std::mutex> lk(m_dump_mutex);
                // wakes up every 20ms
                if(m_dump_cv.wait_for(lk, std::chrono::milliseconds (50),
                                      []{return (!m_dump_queue.empty() || m_dump_terminate);}))
                {
                    auto size = m_dump_queue.size(); // size might change while we process
                    while(size--){
                        const auto dir = m_dump_queue.front().first;
                        if(m_stream_map.count(dir) == 0){
                            m_stream_map[dir] = std::ofstream();
                            m_stream_map[dir].open(dir, std::ios::trunc);
                            m_stream_map[dir].close();
                            m_stream_map[dir].open(dir, std::ios::app);
                            m_stream_map[dir] << m_dump_queue.front().second;
                        }
                        else{
                            m_stream_map[dir] << m_dump_queue.front().second;
                        }
                        m_dump_queue.safe_pop_front();
                    }
                    if(m_dump_terminate && m_dump_queue.empty()){
                        std::cout << "Dump thread exit..." << "\n";
                        return;
                    }
                }
            }
        }
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
        std::unordered_map<std::string, std::ofstream> m_stream_map;
    };
}


#endif //OPENMP_THREADEDDUMP_H
