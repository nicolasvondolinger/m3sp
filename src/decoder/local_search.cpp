#include "decoder/local_search.h"

static Channel removeFromChannel(Channel channel, int idToRemove){
    Channel result(channel.bandwidth);
    for(const auto& conn : channel.connections)
        if(conn.id != idToRemove) result = insertInChannel(result, conn.id);
    return result;
}

static bool relocateSweep(Solution& sol){
    bool anyMove = false;

    vector<int> ids;
    for(auto& slot : sol.slots) for(auto& spec : slot.spectrums)
        for(auto& ch : spec.channels) for(auto& conn : ch.connections) ids.pb(conn.id);

    for(int id : ids){
        int st = -1, ss = -1, sc = -1;
        for(int t = 0; t < (int)sol.slots.size() && st < 0; t++)
            for(int s = 0; s < (int)sol.slots[t].spectrums.size() && st < 0; s++)
                for(int c = 0; c < (int)sol.slots[t].spectrums[s].channels.size() && st < 0; c++)
                    for(const auto& conn : sol.slots[t].spectrums[s].channels[c].connections)
                        if(conn.id == id){ st = t; ss = s; sc = c; break; }

        if(st < 0) continue;

        Channel src = sol.slots[st].spectrums[ss].channels[sc];
        if(src.connections.size() <= 1) continue;
        Channel newSrc = removeFromChannel(src, id);

        double bestDelta = EPS;
        int bt = -1, bs = -1, bc = -1; Channel bestDst;

        for(int t = 0; t < (int)sol.slots.size(); t++)
            for(int s = 0; s < (int)sol.slots[t].spectrums.size(); s++)
                for(int c = 0; c < (int)sol.slots[t].spectrums[s].channels.size(); c++){
                    if(t == st && s == ss && c == sc) continue;
                    Channel dst = sol.slots[t].spectrums[s].channels[c];
                    Channel newDst = insertInChannel(dst, id);
                    double delta = (newSrc.throughput + newDst.throughput) - (src.throughput + dst.throughput);
                    if(delta > bestDelta){ bestDelta = delta; bt = t; bs = s; bc = c; bestDst = newDst; }
                }

        if(bt >= 0){
            sol.slots[st].spectrums[ss].channels[sc] = newSrc;
            sol.slots[bt].spectrums[bs].channels[bc] = bestDst;
            sol.throughput += bestDelta;
            anyMove = true;
        }
    }

    return anyMove;
}

static Solution localSearchType6(Solution sol){
    double prev;
    do {
        prev = sol.throughput;
        while(relocateSweep(sol)) {}
        sol = applyFusion(sol);
        sol = dp(sol);
    } while(sol.throughput > prev + EPS);
    return sol;
}

void runLocalSearch(Solution& sol, vector<vector<double>>& links, vector<double>& variables){
    double totalSpectrum = 500.0, totalUsed = 0.0;
    int i = 0;

    while(i < (int)links.size() && totalUsed < totalSpectrum){
        int connection = links[i][1]/2;
        insertInBestChannel(sol, connection, variables);
        i++;
    }

    // if(applyFinalLS) sol = localSearchType6(sol);
}
