#include "decoder/dp.h"

void runDp(Solution& sol, vector<vector<double>>& links, vector<double>& variables){
    classicConstruct(sol, links, variables);
    applyDpWithCount(sol);
}
