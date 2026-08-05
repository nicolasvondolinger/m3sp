#include "decoder/random_dp.h"

void runRandomDp(Solution& sol, vector<vector<double>>& links, vector<double>& variables){
    classicConstruct(sol, links, variables);
    if(refine) applyDpWithCount(sol);
}
