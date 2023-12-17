#include <iostream>
#include <vector>
#include "/usr/local/opt/libomp/include/omp.h"

int main() {
    std::vector<uint32_t> num (8, 10);
    omp_set_num_threads(8);
    #pragma omp parallel for
    {
        for(int i = 0; i < num.size(); i++){
            std::cout << num[i] << "\n";
        }
    }
    return 0;
}
