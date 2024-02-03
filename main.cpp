
#include "/usr/local/opt/libomp/include/omp.h"
#include "Factory.h"
//#include "ThreadedDump.h"
#include "ParallelLib.h"

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

int main() {
    //CRTPExample();
    //compactMemoryAllocation();
    //threaded_log();

    constexpr int numThreads = 5; // Specify the desired number of threads

    Module m1, m2, m3, m4, m5, m6, m7, m8, m9, m10, m11, m12, m13, m14;
    ThreadPoolOp threadPool;
    threadPool.registerModule(&m1);
    threadPool.registerModule(&m2);
    threadPool.registerModule(&m3);
    threadPool.registerModule(&m4);
    threadPool.registerModule(&m5);
    threadPool.registerModule(&m6);
    threadPool.registerModule(&m7);
    threadPool.registerModule(&m8);
    threadPool.registerModule(&m9);
    threadPool.registerModule(&m10);
    threadPool.registerModule(&m11);
    threadPool.registerModule(&m12);
    threadPool.registerModule(&m13);
    threadPool.registerModule(&m14);

    int count = 0;
    double time = 0;
    int total_cycle = 500;

    // Send the signal multiple times
    for (int i = 0; i < total_cycle; ++i) {

        auto t1 = std::chrono::high_resolution_clock::now();
        threadPool.run();
        auto t2 = std::chrono::high_resolution_clock::now();
        auto ms_int = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1);

        time += ms_int.count();
        count++;

        int tmp = static_cast<int>(rand() / double(RAND_MAX) * 2);
        std::this_thread::sleep_for(std::chrono::milliseconds (tmp));

        if(count % 100 == 0){
            std::cout << "CYCLE: " << count << "\n";
        }
        if(count % total_cycle == 0){
            std::cout << "AVERAGE TIME: " << time / total_cycle << "\n";
            time = 0;
        }
    }

    return 0;
}
