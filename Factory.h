//
// Created by Han on 2023/12/17.
//

#include <map>
#include <vector>
#include <deque>
#include <memory>

#ifndef OPENMP_FACTORY_H
#define OPENMP_FACTORY_H

template<typename T>
class FactoryBase;

template<typename connectType, class upperModule, class downModule>
class Connect;

class ModuleA;
class ModuleB;

//// Fake Interface Definition
struct ModuleAModuleBConnect{
    uint64_t m_data = 0;
};
struct ModuleBModuleAConnect{
    uint64_t m_data = 0;
};

/// All modules stored inside
struct ModuleContainer{
public:
    static ModuleContainer& GetModuleContainer(){
        static ModuleContainer container;
        return container;
    }
    /// Put all modules here, and initialise
    std::shared_ptr<ModuleA> moduleA = std::make_shared<ModuleA>();
    std::shared_ptr<ModuleB> moduleB = std::make_shared<ModuleB>();
};

struct ConnectContainer{
    std::shared_ptr<Connect<ModuleAModuleBConnect, ModuleA, ModuleB>> m_AtoBConnect = nullptr;
    std::shared_ptr<Connect<ModuleBModuleAConnect, ModuleB, ModuleA>> m_BtoAConnect = nullptr;
};

//// Connect Template Base
template<typename connectType, class upperModule, class downModule>
class Connect : FactoryBase<Connect<connectType, upperModule, downModule>>{
public:
    Connect() = delete;
    Connect(std::shared_ptr<upperModule> up, std::shared_ptr<downModule> down){
        m_upper_module = up;
        m_down_module = down;
    }
    void RunOneCycle(){
        latencyUpdate();
    }
    void Write(connectType& input){
        m_data_fifo.template emplace_back(input, 1);
    }
    bool Read(connectType& output){
        if(!m_data_fifo.empty() && m_data_fifo.front().second == 0){
            output = m_data_fifo.front().first;
            m_data_fifo.pop_front();
            return true;
        }
        return false;
    }
private:
    void latencyUpdate(){
        std::cout << "Connect update latency" << "\n";
        for(auto& entry: m_data_fifo){
            std::get<1>(entry) -= (std::get<1>(entry) > 0 ? 1 : 0);
        }
    }
    std::shared_ptr<upperModule> m_upper_module = nullptr;
    std::shared_ptr<downModule> m_down_module = nullptr;
    std::deque<std::pair<connectType, int>> m_data_fifo{};
};


//// Factory Template Base
template<typename T>
class FactoryBase{
public:
    void RunOneCycleTop(){
        std::cout << "RunOneCycleTop " << static_cast<T*>(this)->GetModuleID() << "\n";
        static_cast<T*>(this)->RunOneCycle();
        cycle_count++;
    }
    void RunOneCycle(){
        throw std::runtime_error("Module must implement RunOneCycle!");
    }

    std::string GetModuleID(){
        throw std::runtime_error("Module must implement GetModuleID!");
    }
    int cycle_count = 0;
    std::string module_id = "FactoryBase";
};

class ModuleA: public FactoryBase<ModuleA>{
public:
    void RunOneCycle(){
        std::cout << "ModuleA runs one cycle, cycle:" << cycle_count << "\n";
        ModuleBModuleAConnect input{};
        if(m_BtoAConnect->Read(input)){
            std::cout << "ModuleA receives ModuleBModuleAConnect data" << "\n";
        }
    }
    std::string GetModuleID(){
        return module_id;
    }
    std::shared_ptr<Connect<ModuleAModuleBConnect, ModuleA, ModuleB>> m_AtoBConnect = nullptr;
    std::shared_ptr<Connect<ModuleBModuleAConnect, ModuleB, ModuleA>> m_BtoAConnect = nullptr;
    std::string module_id = "ModuleA";
};

class ModuleB: public FactoryBase<ModuleB>{
public:
    void RunOneCycle(){
        std::cout << "ModuleB runs one cycle, cycle:" << cycle_count << "\n";
        ModuleBModuleAConnect input{};
        m_BtoAConnect->Write(input);
    }
    std::string GetModuleID(){
        return module_id;
    }
    std::shared_ptr<Connect<ModuleAModuleBConnect, ModuleA, ModuleB>> m_AtoBConnect = nullptr;
    std::shared_ptr<Connect<ModuleBModuleAConnect, ModuleB, ModuleA>> m_BtoAConnect = nullptr;
    std::string module_id = "ModuleB";
};

#endif //OPENMP_FACTORY_H
