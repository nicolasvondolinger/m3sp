#include "m3sp/decoder/mic.h"
#include "m3sp/decoder/gcsid_plus_ls.h"

static int convertBand(double value){
    if(value < 0.25) return 20;
    else if(value < 0.50) return 40;
    else if(value < 0.75) return 80;

    return 160;
}

static double insertNextFree(Solution& sol, int idConnection, int band, vector<double>& variables){
    int freeSpec = 0, maxSpec = 0, indexMaxSpec = 0, indexMaxSlotSpec = 0, auxBand = -1;
    bool inserted = false;

    for(int i = 0; i < sol.slots.size(); i++){
        for(int j = 0; j < sol.slots[i].spectrums.size(); j++){
            freeSpec = sol.slots[i].spectrums[j].maxFrequency - sol.slots[i].spectrums[j].usedFrequency;
            if(freeSpec >= band){
                sol.slots[i].spectrums[j].usedFrequency += band;
                Channel newChannel = insertInChannel(Channel(band), idConnection);
                sol.slots[i].spectrums[j].channels.emplace_back(newChannel);
                sol.throughput += newChannel.throughput;
                inserted = true;
                return band;
            } else if(freeSpec > maxSpec) {
                maxSpec = freeSpec;
                indexMaxSpec = j;
                indexMaxSlotSpec = i;

                if(maxSpec>=80) auxBand = 80;
                else if(maxSpec>=40) auxBand = 40;
                else auxBand = 20;
            }
        }
    }

    if(!inserted && maxSpec > 0){
        Channel newChannel = insertInChannel(Channel(auxBand), idConnection);
        sol.slots[indexMaxSlotSpec].spectrums[indexMaxSpec].usedFrequency += auxBand;
        sol.slots[indexMaxSlotSpec].spectrums[indexMaxSpec].channels.emplace_back(newChannel);

        sol.throughput += newChannel.throughput;

        if(auxBand != band){
            if(newChannel.bandwidth == 20) variables[(idConnection * 2) + 1] = (0+0.25)/2.0;
            else if(newChannel.bandwidth == 40) variables[(idConnection * 2) + 1] = (0.25+0.5)/2.0;
            else if(newChannel.bandwidth == 80) variables[(idConnection * 2) + 1] = (0.5+0.75)/2.0;
            else if(newChannel.bandwidth == 160) variables[(idConnection * 2) + 1] = (0.75+1.0)/2.0;
            else cout << "ERRO" << endl;
        }
    }

    return auxBand;
}

static void insertBestFree(Solution& sol, int idConnection, int band, vector<double>& variables){
    double currentThroughput = sol.throughput, bestThroughput = sol.throughput;
    bool inserted = false;
    Channel newChannel(band);
    vector<int> tscBestThroughput(3, -1);

    for(int t = 0; t < sol.slots.size(); t++){
        for(int s = 0; s < sol.slots[t].spectrums.size(); s++){
            for(int c = 0; c < sol.slots[t].spectrums[s].channels.size(); c++){
                Channel toInsert = insertInChannel(sol.slots[t].spectrums[s].channels[c], idConnection);
                double aux = (currentThroughput - sol.slots[t].spectrums[s].channels[c].throughput) + toInsert.throughput;
                if(aux > bestThroughput){
                    bestThroughput = aux;
                    newChannel = toInsert;
                    inserted = true;
                    tscBestThroughput = {t, s, c};
                }
            }
        }
    }

    if(inserted){
        sol.throughput = bestThroughput;
        sol.slots[tscBestThroughput[0]].spectrums[tscBestThroughput[1]].channels[tscBestThroughput[2]] = newChannel;

        if(newChannel.bandwidth != band){
            if(newChannel.bandwidth==20)variables[(idConnection*2)+1] = (0+0.25)/2.0;
            else if(newChannel.bandwidth==40)variables[(idConnection*2)+1] = (0.25+0.50)/2.0;
            else if(newChannel.bandwidth==80)variables[(idConnection*2)+1] = (0.5+0.75)/2.0;
            else if(newChannel.bandwidth==160)variables[(idConnection*2)+1] = (0.75+1.0)/2.0;
            else cout << "ERRO 2" << endl;
        }
    }
}

void micConstruct(Solution& sol, vector<vector<double>>& links, vector<double>& variables){
    double totalSpectrum = 500.0, totalUsed = 0.0;
    int i = 0;

    while(i < (int)links.size() && totalUsed < totalSpectrum){
        int connection = links[i][1]/2;
        int band = convertBand(variables[links[i][1]+1]);
        totalUsed += insertNextFree(sol, connection, band, variables);
        i++;
    }

    while(i < (int)links.size()){
        int connection = links[i][1]/2;
        int band = convertBand(variables[links[i][1]+1]);
        insertBestFree(sol, connection, band, variables);
        i++;
    }
}

void runMic(Solution& sol, vector<vector<double>>& links, vector<double>& variables){
    micConstruct(sol, links, variables);
}

void runMicLs(Solution& sol, vector<vector<double>>& links, vector<double>& variables){
    micConstruct(sol, links, variables);
    localSearchGcsidPlus(sol, links);
}
