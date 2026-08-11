#include "decoder/new_heuristic.h"

void insertInBestChannel(Solution& sol, int idConnection, vector<double>& variables){
    double currentThroughput = sol.throughput, bestThroughput = sol.throughput;
    bool inserted = false, splited = false, created = false;
    Channel top, left, right, toBeCreated;
    vector<int> tsc(3, -1);

    for(int t = 0; t < sol.slots.size(); t++){
        for(int s = 0; s < sol.slots[t].spectrums.size(); s++){
            int freeSpec = sol.slots[t].spectrums[s].maxFrequency - sol.slots[t].spectrums[s].usedFrequency;
            if(freeSpec >= 160) freeSpec = 160;
            else if (freeSpec >= 80) freeSpec = 80;
            else if(freeSpec >= 40) freeSpec = 40;

            if(freeSpec != 0){
                Channel temp(freeSpec);
                temp = insertInChannel(temp, idConnection);
                double aux_temp = currentThroughput + temp.throughput;

                if(aux_temp > bestThroughput){
                    created = true; splited = false; inserted = false;
                    bestThroughput = aux_temp;
                    toBeCreated = temp;
                    tsc = {t, s, -1};
                }
            }

            for(int c = 0; c < sol.slots[t].spectrums[s].channels.size(); c++){
                Channel toInsert = insertInChannel(sol.slots[t].spectrums[s].channels[c], idConnection);
                double aux = (currentThroughput - sol.slots[t].spectrums[s].channels[c].throughput) + toInsert.throughput;
                if(aux > bestThroughput){
                    splited = false;
                    bestThroughput = aux;
                    top = toInsert;
                    inserted = true;
                    tsc = {t, s, c};
                }

                if(toInsert.bandwidth == 20) continue;

                int n = toInsert.connections.size();
                double newBand = toInsert.bandwidth / 2.0;
                Channel temp1(newBand), temp2(newBand);

                vector<int> connection_ids;
                for(const auto& conn : toInsert.connections) connection_ids.pb(conn.id);

                int n_conn = connection_ids.size();
                for (int i = n_conn - 1; i > 0; --i) {
                    int j = rng.randInt(i);
                    std::swap(connection_ids[i], connection_ids[j]);
                }

                for(int i = 0; i < n; i++){
                    if(i%2==0) temp1 = insertInChannel(temp1, connection_ids[i]);
                    else temp2 = insertInChannel(temp2, connection_ids[i]);
                }

                double newAux = (currentThroughput - sol.slots[t].spectrums[s].channels[c].throughput) + temp1.throughput + temp2.throughput;
                if(newAux > bestThroughput){
                    splited = true; inserted = true;
                    bestThroughput = newAux;
                    left = temp1; right = temp2;
                    tsc = {t, s, c};
                }
            }
        }
    }

    if(inserted && !splited){
        sol.throughput = bestThroughput;
        sol.slots[tsc[0]].spectrums[tsc[1]].channels[tsc[2]] = top;
    } else if(inserted && splited){
        sol.throughput = bestThroughput;
        sol.slots[tsc[0]].spectrums[tsc[1]].channels.erase(sol.slots[tsc[0]].spectrums[tsc[1]].channels.begin() + tsc[2]);
        sol.slots[tsc[0]].spectrums[tsc[1]].channels.push_back(left);
        sol.slots[tsc[0]].spectrums[tsc[1]].channels.push_back(right);
    } else if(created){
        sol.throughput = bestThroughput;
        sol.slots[tsc[0]].spectrums[tsc[1]].channels.push_back(toBeCreated);
        sol.slots[tsc[0]].spectrums[tsc[1]].usedFrequency += toBeCreated.bandwidth;
    }
}

void runNewHeuristic(Solution& sol, vector<vector<double>>& links, vector<double>& variables){
    double totalSpectrum = 500.0, totalUsed = 0.0;
    int i = 0;

    while(i < (int)links.size() && totalUsed < totalSpectrum){
        int connection = links[i][1]/2;
        insertInBestChannel(sol, connection, variables);
        i++;
    }

    sol = applyFusion(sol);
}
