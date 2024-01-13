#include <iostream>
#include <vector>
#include <future>
#include <thread>
#include <array>
#include "/usr/local/opt/libomp/include/omp.h"
#include "Factory.h"
#include "ThreadedDump.h"

//// OPENMP
void ParallelPrint(){
    std::vector<uint32_t> num (8, 10);
    omp_set_num_threads(10);
    #pragma omp parallel
    {
        int thx = omp_get_thread_num();
        #pragma omp for
        for(int i = 0; i < num.size(); i++){
            std::string temp = "thread " + std::to_string(thx) + " " + std::to_string(num[i]) + "\n";
            std::cout << temp;
        }
    }
}

void ParallelPrintwithLock(){
    std::vector<uint32_t> num (8, 10);
    omp_set_num_threads(10);
    omp_lock_t writelock;
    omp_init_lock(&writelock);
    #pragma omp parallel
    {
        int thx = omp_get_thread_num();
        #pragma omp for
        for(int i = 0; i < num.size(); i++){
            omp_set_lock(&writelock);
            std::cout << "thread " << thx << " " << num[i] << "\n";
            omp_unset_lock(&writelock);
        }
    }
    omp_destroy_lock(&writelock);
}

void compactMemoryAllocation(){
    const int count = 10000;
    std::array<ModuleA, count> stack_array{};
    std::array<ModuleA*, count> discreet_pointer_array{nullptr};
    std::array<ModuleA*, count> pointer_array{nullptr};

    int stride_discreet = 0;
    int prev_addr_discreet = 0;
    int stride_compact = 0;
    int prev_addr_compact = 0;

    for(int i = 0; i < count; i++){
        discreet_pointer_array[i] = new ModuleA;
        std::cout << "discreet mem addr: " << discreet_pointer_array[i] << "\n";
    }
    for(int i = 0; i < stack_array.size(); i++){
        pointer_array[i] = &stack_array[i];
        std::cout << "compact mem addr: " << pointer_array[i] << "\n";
    }
    for(auto& entry: pointer_array){
        std::cout << entry->GetModuleID() << "\n";
    }
}

//// CRTP
void CRTPExample(){
    auto container = ModuleContainer::GetModuleContainer();

    /// Connect Initialisation
    using AtoBConnectType = Connect<ModuleAModuleBConnect, ModuleA, ModuleB>;
    using BtoAConnectType = Connect<ModuleBModuleAConnect, ModuleB, ModuleA>;
    std::shared_ptr<AtoBConnectType> AtoBConnect = std::make_shared<AtoBConnectType>(container.moduleA, container.moduleB);
    std::shared_ptr<BtoAConnectType> BtoAConnect = std::make_shared<BtoAConnectType>(container.moduleB, container.moduleA);

    /// Connect the modules
    container.moduleA->m_AtoBConnect = AtoBConnect;
    container.moduleB->m_AtoBConnect = AtoBConnect;
    container.moduleA->m_BtoAConnect = BtoAConnect;
    container.moduleB->m_BtoAConnect = BtoAConnect;

    for(int i = 0; i < 10; i ++){
        container.moduleA->RunOneCycleTop();
        container.moduleB->RunOneCycleTop();
        AtoBConnect->RunOneCycle();
        BtoAConnect->RunOneCycle();
    }

}

TD::TDQueue TD::m_dump_queue; // des, data
std::mutex TD::m_dump_mutex;
std::condition_variable TD::m_dump_cv;
bool TD::m_dump_terminate = false;

void threaded_log(){

    std::vector<std::thread> threads;
    auto check_dump_config = std::promise<bool>();
    auto is_dump_enabled = check_dump_config.get_future();
    threads.emplace_back(&TD::ThreadedDump::init, TD::ThreadedDump(), std::move(check_dump_config));

    if(is_dump_enabled.get()){
        for(int j = 0; j < 20; j++){
            for(int i = 0; i < 200; i++){
                TD::m_dump_queue.safe_emplace_back({"tmp1.txt", std::to_string(i) + " xxxxxxxxxxxxxxx" +"\n"});
                TD::m_dump_queue.safe_emplace_back({"tmp2.txt", std::to_string(i) + " xxxxxxxxxxxxxxx" +"\n"});
                TD::m_dump_queue.safe_emplace_back({"tmp3.txt", std::to_string(i) + " xxxxxxxxxxxxxxx" +"\n"});
                TD::m_dump_queue.safe_emplace_back({"tmp4.txt", std::to_string(i) + " xxxxxxxxxxxxxxx" +"\n"});
            }
            std::cout << TD::m_dump_queue.size() << "\n";
        }
    }
    else{
        std::cout << "Dump not enabled.." << "\n";
    }
    std::cout << "Main thread done, waiting for termination.." << "\n";
    TD::m_dump_terminate = true;
    threads[0].join();
}

int main() {
    //CRTPExample();
    //compactMemoryAllocation();
    threaded_log();
    return 0;
}
