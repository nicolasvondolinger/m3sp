#include "../include/decoder.h"

bool approximatelyEqual(double a, double b, double epsilon = EPS) {
    return fabs(a - b) <= ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

int convertBand(double value){
    if(value < 0.25) return 20;
    else if(value < 0.50) return 40;
    else if(value < 0.75) return 80;
    
    return 160;
}

int bandIndex(int band){
    if(band == 40) return 1;
    else if(band == 80) return 2;
    else if(band == 160) return 3;
    return 0;
}

double computeConnectionThroughput(Connection& connection, int band){
    if(band == 0) return 0;
    int MCS = -1;

    if(approximatelyEqual(connection.interference, 0.0, EPS)){
        MCS = 11;
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

    newChannel.connections.emplace_back(connectionToInsert);
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

    // assert(auxBand != -1);
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

void insertBestFree(Solution& sol, int idConnection, int band, vector<double>& variables){
    double currentThroughput = sol.throughput, bestThroughput = sol.throughput;
    bool inserted = false;
    Channel newChannel(band);
    vector<int> tscBestThroughput(3, -1);

    for(int t = 0; t < sol.slots.size(); t++){
        for(int s = 0; s < sol.slots[t].spectrums.size(); s++){
            for(int c = 0; c < sol.slots[t].spectrums[s].channels.size(); c++){
                // Calcular o novo throughput baseado no canal atual da iteração (t, s, c)
                Channel toInsert = insertInChannel(sol.slots[t].spectrums[s].channels[c], idConnection);
                double aux = currentThroughput - sol.slots[t].spectrums[s].channels[c].throughput + toInsert.throughput;
                
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

pair<double, vector<Channel>> dfs(Channel channel){

    if(channel.bandwidth <= 20) return {channel.throughput, {channel}}; 

    double newBand = channel.bandwidth / 2.0;
    Channel a(newBand), b(newBand);

    int n = channel.connections.size();
    if (n < 2) return {channel.throughput, {channel}}; // Não é possível dividir um canal com 0 ou 1 conexão
    
    vector<int> connection_ids;
    for(const auto& conn : channel.connections) connection_ids.pb(conn.id);
    
    random_shuffle(connection_ids.begin(), connection_ids.end()); 

    for(int i = 0; i < n; i++){
        if(i < n / 2) a = insertInChannel(a, connection_ids[i]);
        else b = insertInChannel(b, connection_ids[i]);
    }

    pair<double, vector<Channel>> result_a = dfs(a), result_b = dfs(b);

    double children_throughput = result_a.first + result_b.first; 

    if (children_throughput >= channel.throughput) {
        vector<Channel> best_resultant_channels = result_a.second;
        best_resultant_channels.insert(best_resultant_channels.end(), result_b.second.begin(), result_b.second.end());
        return {children_throughput, best_resultant_channels};
    } else return {channel.throughput, {channel}}; 
    
}

Solution dp(Solution sol){
    Solution ans = sol;
    ans.throughput = 0.0;
    for(int i = 0; i < sol.slots.size(); i++){
        for(int j = 0; j < sol.slots[i].spectrums.size(); j++){
            vector<Channel> originalChannels = ans.slots[i].spectrums[j].channels;
            ans.slots[i].spectrums[j].channels.clear(); // Limpa para adicionar os canais refinados

            for(Channel& currentChannel : originalChannels){
                // Chama a função recursiva corrigida
                pair<double, vector<Channel>> result = dfs(currentChannel);
                
                // Adiciona os canais resultantes (divididos ou o original)
                for(Channel newChannel : result.second){
                    ans.slots[i].spectrums[j].channels.push_back(newChannel);
                }
                ans.throughput += result.first; // Adiciona o throughput obtido
            }
        }
    }

    return ans;
}

double Solution::decode(vector<double> variables) const {
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

    Solution sol ({spec1, spec2, spec3}, 0.0);
    double totalSpectrum = 500.0, totalUsed = 0.0;

    int i = 0;
    while(i < links.size() && totalUsed < totalSpectrum){
        int connection = links[i][1]/2;
        int band = convertBand(variables[links[i][1]+1]);
        totalUsed += insertNextFree(sol, connection, band, variables);
        i++;
    }

    while(i < links.size()){
        int connection = links[i][1]/2;
        int band = convertBand(variables[links[i][1]+1]);
        insertBestFree(sol, connection, band, variables);
        i++;
    } 

    Solution temp;
    if(type) temp = dp(sol);

    return (type) ? 1.0 * temp.throughput : -1.0 * sol.throughput;
}