#include "decoder.h"
#include "decoder/classic.h"
#include "decoder/dp.h"
#include "decoder/random_dp.h"
#include "decoder/dp_rebuild.h"
#include "decoder/new_heuristic.h"
#include "decoder/local_search.h"

double Solution::decode(vector<double>& variables) const {
    int n = variables.size();

    vector<vector<double>> links(n/2);

    for(int i = 0; i < n; i+=2){
        double k1 = variables[i];
        links[i/2].push_back(k1); links[i/2].push_back(i);
    }

    sort(links.begin(), links.end());

    vector<Channel> aux;
    Spectrum spec1(160, 0, aux);
    Spectrum spec2(240, 0, aux);
    Spectrum spec3(100, 0, aux);

    Solution sol = Solution({spec1, spec2, spec3}, 0.0);

    if(type == 0) runClassic(sol, links, variables);
    else if(type == 1) runDp(sol, links, variables);
    else if(type == 3) runRandomDp(sol, links, variables);
    else if(type == 4) runDpRebuild(sol, links, variables);
    else if(type == 5) runNewHeuristic(sol, links, variables);
    else if(type == 6) runLocalSearch(sol, links, variables);

    if (!checkFeasibility(sol, n/2)) {
        cout << "Solucao Inviável Detectada!" << endl;
        exit(1);
    }

    if (collectChannels) {
        channelBandCount.clear();
        for (const auto& slot : sol.slots)
            for (const auto& spec : slot.spectrums)
                for (const auto& ch : spec.channels)
                    if (!ch.connections.empty())
                        channelBandCount[ch.bandwidth]++;
    }

    return -1.0 * sol.throughput;
}
