#include "m3sp/decoder/dp.h"

void applyDpWithCount(Solution& sol){
    Solution temp = dp(sol);

    if (temp.throughput > sol.throughput) {
        #pragma omp atomic
        count_improvements++;
    }

    sol = temp;
}

void runDp(Solution& sol, vector<vector<double>>& links, vector<double>& variables){
    micConstruct(sol, links, variables);
    applyDpWithCount(sol);
}
