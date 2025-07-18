#include "decoder.h"

bool approximatelyEqual(double a, double b, double epsilon = EPS) {
    return fabs(a - b) <= ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

int convertBand(double value){
    if(value >=0 && value < 25) return 20;
    if(value >= 25 && value < 50) return 40;
    if(value >= 50 && value < 75) return 80;
    else return 160;
}

int bandIndex(int band){
    if(band == 40) return 1;
    else if(band == 80) return 2;
    else if(band == 160) return 3;
    return 0;
}

double computeConnectionThroughput(Connection& connection, int band){
    if(band == 0) return 0;
    int MCS = -1, maxMCS = 11;

    if(approximatelyEqual(connection.interference, 0.0, EPS)){
        MCS = maxMCS;
        connection.throughput = dataRates[MCS][bandIndex(band)];
    } else {
        connection.SINR = affectance[connection.id][connection.id] / (connection.interference + noise);
        MCS = 11;
        while(MCS>=0 && connection.SINR < SINR[MCS][bandIndex(band)]) MCS--;

        if(MCS < 0) connection.throughput = 0.0;
        else connection.throughput = dataRates[MCS][bandIndex(band)];
    }

    return connection.throughput;
}

Channel insertInChannel(Channel newChannel, int id){
    Connection connectionToInsert(id, 0.0, 0.0, distanceMatrix[id][id]);

    for(auto& connection: newChannel.connections){
        connection.interference += affectance[connection.id][connectionToInsert.id];
        connectionToInsert.interference += affectance[connectionToInsert.id][connection.id];
    }

    newChannel.connections.push_back(connectionToInsert);
    newChannel.violation = newChannel.throughput = 0.0;

    for(auto& connection: newChannel.connections){
        computeConnectionThroughput(connection, newChannel.bandwidth);
        newChannel.throughput += connection.throughput;
    }

    return newChannel;
}

double insertNextFree(Solution& sol, int idConnection, int band, vector<double>& variables){
    int freeSpec = 0, maxSpec = 0, indexMaxSpec = 0, indexMaxSlotSpec = 0, auxBand = -1;
    bool inserted = false;
    
    for(int i = 0; i < sol.slots.size(); i++){
        for(int j = 0; j < sol.slots[i].spectrums.size(); j++){
            freeSpec = sol.slots[i].spectrums[j].maxFrequency - sol.slots[i].spectrums[j].usedFrequency;
            if(freeSpec >= band){
                sol.slots[i].spectrums[j].usedFrequency += band;
                Channel newChannel = insertInChannel(Channel(band), idConnection);
            }
        }
    }
}

double Solution::decode(vector<double>& variables) const {
    double solution = 0.0; int n = variables.size();

    vector<vector<double>> links(n/2);

    for(int i = 0; i < n; i+=2){
        double k1 = variables[i];
        links[i].push_back(k1); links[i].push_back(i);
    }

    sort(links.begin(), links.end());

    vector<Channel> aux;
    Spectrum spec1(160, 0, aux);
    Spectrum spec2(240, 0, aux);
    Spectrum spec3(100, 0, aux);

    // Um expectrum tem vários canais (20,...,160) e a solução possui vários spectrums, 3 nesse caso

    Solution sol ({spec1, spec2, spec3}, 0.0);
    double totalSpectrum = 500.0, totalUsed = 0.0;

    for(int i = 0; i < links.size(); i++){
        if(totalUsed >= totalSpectrum) break;
        int connection = links[i][1]/2;
        int band = convertBand(variables[links[i][1]+1]);
        totalUsed += insertNextFree(sol, connection, band, variables);
    }


    return solution;
}