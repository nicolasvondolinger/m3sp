#include "../include/decoder.h"
#include "../include/MTRand.h" 

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

double insertLessInterference(Solution& sol, int idConnection, vector<double>& variables){
    const int band = 20; // Banda fixa em 20 MHz
    
    for(int i = 0; i < sol.slots.size(); i++){
        for(int j = 0; j < sol.slots[i].spectrums.size(); j++){
            int freeSpec = sol.slots[i].spectrums[j].maxFrequency - sol.slots[i].spectrums[j].usedFrequency;
            if(freeSpec >= band){
                sol.slots[i].spectrums[j].usedFrequency += band;
                Channel newChannel = insertInChannel(Channel(band), idConnection);
                sol.slots[i].spectrums[j].channels.emplace_back(newChannel);
                sol.throughput += newChannel.throughput;
                return band; 
            }
        }
    }

    return 0.0;
}

double calculateTotalChannelInterference(const Channel& channel) {
    double totalInterference = 0.0;
    for (const auto& connection : channel.connections) {
        totalInterference += connection.interference;
    }
    return totalInterference;
}

void insertBestInterference(Solution& sol, int idConnection, vector<double>& variables) {
    double minAddedInterference = std::numeric_limits<double>::max();
    Channel bestChannel;
    vector<int> tscBestPlacement = {-1, -1, -1};
    bool inserted = false;

    for (int t = 0; t < sol.slots.size(); t++) {
        for (int s = 0; s < sol.slots[t].spectrums.size(); s++) {
            for (int c = 0; c < sol.slots[t].spectrums[s].channels.size(); c++) {
                
                Channel originalChannel = sol.slots[t].spectrums[s].channels[c];
                double originalInterference = calculateTotalChannelInterference(originalChannel);

                Channel candidateChannel = insertInChannel(originalChannel, idConnection);

                double newInterference = calculateTotalChannelInterference(candidateChannel);

                double addedInterference = newInterference - originalInterference;

                if (addedInterference < minAddedInterference) {
                    minAddedInterference = addedInterference;
                    bestChannel = candidateChannel;
                    tscBestPlacement = {t, s, c};
                    inserted = true;
                }
            }
        }
    }

    if (inserted) {
        Channel oldChannel = sol.slots[tscBestPlacement[0]].spectrums[tscBestPlacement[1]].channels[tscBestPlacement[2]];
        sol.throughput -= oldChannel.throughput;
        sol.throughput += bestChannel.throughput;
        sol.slots[tscBestPlacement[0]].spectrums[tscBestPlacement[1]].channels[tscBestPlacement[2]] = bestChannel;
    }
}

Solution applyFusion(Solution sol) {
    // Solution newSol = sol;
    // for(auto& slot : newSol.slots) for(auto& spec : slot.spectrums) spec.channels.clear();
    while(true){
        bool done = false, restart = false;
        for(auto & slot: sol.slots){
            for(auto & spec: slot.spectrums){
                int n = spec.channels.size();
                for(int i = 0; i < n; i++){
                    Channel a = spec.channels[i], b;
                    for(int j = i+1; j < n; j++){
                        if(spec.channels[j].bandwidth == a.bandwidth && a.bandwidth < 160){
                            b = spec.channels[j];
                            Channel aux(a.bandwidth*2.0);
                            vector<int> connection_ids;
                            for(auto & conn : a.connections) connection_ids.pb(conn.id);
                            for(auto & conn : b.connections) connection_ids.pb(conn.id);
                            for(auto & id : connection_ids) aux = insertInChannel(aux, id);
                            if(aux.throughput > a.throughput + b.throughput){
                                spec.channels.erase(spec.channels.begin() + j);
                                spec.channels.erase(spec.channels.begin() + i);
                                spec.channels.push_back(aux);
                                sol.throughput -= (a.throughput); sol.throughput -= (b.throughput);
                                sol.throughput += (aux.throughput);
                                done = true; restart = true;
                                // cout << "aqui" << endl;
                                break; 
                            }
                        }
                    }
                    if(restart) break;
                }
                if(restart) break;
            }
            if(restart) break;
        }
        if(!done) break;
    }
    
    return sol;
}

pair<double, vector<Channel>> dfs(Channel channel){

    if(channel.bandwidth <= 20) return {channel.throughput, {channel}}; 

    double newBand = channel.bandwidth / 2.0;
    Channel a(newBand), b(newBand);

    int n = channel.connections.size();
    if (n == 0) return {0.0, {}};
    else if (n == 1) return {channel.throughput, {channel}}; // Não é possível dividir um canal com 0 ou 1 conexão
    
    vector<int> connection_ids;
    for(const auto& conn : channel.connections) connection_ids.pb(conn.id);
    
    int n_conn = connection_ids.size();
    for (int i = n_conn - 1; i > 0; --i) {
        int j = rng.randInt(i);
        std::swap(connection_ids[i], connection_ids[j]);
    }

    for(int i = 0; i < n; i++){
        if(i%2==0) a = insertInChannel(a, connection_ids[i]);
        else b = insertInChannel(b, connection_ids[i]);
    }

    pair<double, vector<Channel>> result_a = dfs(a), result_b = dfs(b);

    double children_throughput = result_a.first + result_b.first; 

    if (children_throughput > channel.throughput) {
        vector<Channel> best_resultant_channels = result_a.second;
        best_resultant_channels.insert(best_resultant_channels.end(), result_b.second.begin(), result_b.second.end());
        return {children_throughput, best_resultant_channels};
    } else return {channel.throughput, {channel}}; 
    
}

Solution dp(Solution sol){
    Solution newSol = sol; 
    newSol.throughput = 0.0;

    for(auto& slot : newSol.slots) {
        for(auto& spec : slot.spectrums) {
            spec.channels.clear();
            spec.usedFrequency = 0;
        }
    }

    for(int i = 0; i < sol.slots.size(); i++){
        for(int j = 0; j < sol.slots[i].spectrums.size(); j++){
            
            vector<Channel> originalChannels = sol.slots[i].spectrums[j].channels;

            for(Channel& currentChannel : originalChannels){
                pair<double, vector<Channel>> result = dfs(currentChannel);
                
                for(Channel newChannel : result.second){
                    newSol.slots[i].spectrums[j].channels.push_back(newChannel);
                    newSol.slots[i].spectrums[j].usedFrequency += newChannel.bandwidth;
                }
                
                newSol.throughput += result.first;
            }
        }
    }

    return newSol;
}

vector<double> rebuildChromossome(const Solution& sol, int nVariables, vector<double>& variables) {
    vector<double> newVariables = variables; 
    
    // 1. Calcula o total de conexões para definir o passo da prioridade
    int totalConnections = 0;
    for (const auto& slot : sol.slots)
        for (const auto& spec : slot.spectrums)
            for (const auto& channel : spec.channels)
                totalConnections += channel.connections.size();

    double priorityStep = 1.0 / (totalConnections + 1.0);
    double currentPriority = priorityStep;

    vector<double> channels_gene;
    // aux[j] guarda as conexões do canal j
    vector<vector<Connection>> aux; 

    // 2. Preenche a estrutura auxiliar
    for (const auto& slot : sol.slots) {
        for (const auto& spec : slot.spectrums) {
            for (const auto& channel : spec.channels) {
                
                // Define o gene da banda para este canal
                double newBandGene = 0.0;
                if (channel.bandwidth == 20) newBandGene = 0.125;
                else if (channel.bandwidth == 40) newBandGene = 0.375;
                else if (channel.bandwidth == 80) newBandGene = 0.625;
                else newBandGene = 0.875;
                
                channels_gene.push_back(newBandGene);
                
                // Ordena conexões dentro do canal pela prioridade ANTIGA
                // para manter consistência interna
                vector<pair<double, Connection>> temp_chan;
                for (const auto& conn : channel.connections) {
                    temp_chan.push_back({variables[conn.id * 2], conn});
                }
                sort(temp_chan.begin(), temp_chan.end()); // Ordena pelo first (priority antiga)

                // Salva apenas as conexões ordenadas no aux
                vector<Connection> sorted_conns;
                for(auto& p : temp_chan) sorted_conns.push_back(p.second);
                aux.push_back(sorted_conns);
            }
        }
    }

    // 3. Descobre a profundidade máxima (canal com mais conexões)
    int max_depth = 0;
    for(int j = 0; j < aux.size(); j++){
        max_depth = max(max_depth, int(aux[j].size()));
    }

    // 4. Reconstrói intercalando (Round Robin)
    // i = índice da conexão dentro do canal (profundidade)
    // j = índice do canal
    for(int i = 0; i < max_depth; i++){
        for(int j = 0; j < aux.size(); j++){
            
            // Verifica se o canal 'j' possui uma conexão na posição 'i'
            if(i >= aux[j].size()) continue;

            // CORREÇÃO DO SEG_FAULT AQUI: aux[j][i] e não aux[i][j]
            Connection conn = aux[j][i]; 
            
            int id = conn.id;
            int priorityIndex = id * 2;
            int bandIndex = id * 2 + 1;

            // if (priorityIndex < nVariables && bandIndex < nVariables) {
                // Atribui a nova prioridade sequencial
                // newVariables[priorityIndex] = currentPriority;
                
                // Atribui o gene do canal 'j' correspondente
                // CORREÇÃO DE LÓGICA: channels_gene[j] e não [i]
                newVariables[bandIndex] = channels_gene[j]; 

                currentPriority += priorityStep;
           // }
        }
    }
    
    return newVariables;
}

Solution verify(vector<double>& variables){
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

    return sol;
}

bool checkFeasibility(const Solution& sol, int totalLinksExpected) {
    double totalSolThroughput = 0.0;
    vector<int> linkCounts(totalLinksExpected, 0);

    for (int t = 0; t < sol.slots.size(); t++) {
        for (int s = 0; s < sol.slots[t].spectrums.size(); s++) {
            const auto& spec = sol.slots[t].spectrums[s];
            double usedFreq = 0.0;

            for (const auto& channel : spec.channels) {
                usedFreq += channel.bandwidth;

                double channelThroughputCheck = 0.0;

                for (size_t i = 0; i < channel.connections.size(); i++) {
                    const auto& connA = channel.connections[i];
                    
                    if (connA.id < 0 || connA.id >= totalLinksExpected) {
                        cout << "[ERRO] Link ID invalido: " << connA.id << endl;
                        return false;
                    }
                    linkCounts[connA.id]++;

                    double calcInterference = 0.0;
                    for (size_t j = 0; j < channel.connections.size(); j++) {
                        if (i == j) continue;
                        const auto& connB = channel.connections[j];
                        calcInterference += affectance[connA.id][connB.id];
                    }

                    if (!approximatelyEqual(calcInterference, connA.interference, 1e-4)) {
                        cout << "[ERRO] Interferencia incompativel no Slot " << t << " Spec " << s 
                             << " Link " << connA.id << ". Real: " << calcInterference 
                             << " vs Salvo: " << connA.interference << endl;
                        return false;
                    }

                    Connection tempConn = connA;
                    tempConn.interference = calcInterference; 
                    computeConnectionThroughput(tempConn, channel.bandwidth);
                    
                    if (!approximatelyEqual(tempConn.throughput, connA.throughput, 1e-4)) {
                         cout << "[ERRO] Throughput incompativel no Link " << connA.id 
                              << ". Real: " << tempConn.throughput << " vs Salvo: " << connA.throughput << endl;
                         return false;
                    }

                    channelThroughputCheck += tempConn.throughput;
                }

                if (!approximatelyEqual(channelThroughputCheck, channel.throughput, 1e-4)) {
                    cout << "[ERRO] Soma do Throughput do Canal errada. Real: " << channelThroughputCheck 
                         << " vs Salvo: " << channel.throughput << endl;
                    return false;
                }
                
                totalSolThroughput += channelThroughputCheck;
            }

            if (usedFreq > spec.maxFrequency + EPS) {
                cout << "[ERRO] Espectro estourado no Slot " << t << " Spec " << s 
                     << ". Usado: " << usedFreq << " / Max: " << spec.maxFrequency << endl;
                return false;
            }
        }
    }

    if (!approximatelyEqual(totalSolThroughput, sol.throughput, 1e-3)) {
         cout << "[ERRO] Throughput Total da Solucao errado. Calculado: " << totalSolThroughput 
              << " vs Armazenado: " << sol.throughput << endl;
         return false;
    }

    for(int i = 0; i < totalLinksExpected; i++){
        if(linkCounts[i] > 1){
             cout << "[AVISO] Link " << i << " alocado " << linkCounts[i] << " vezes." << endl;
        }
    }

    return true;
}

void insertInBestChannel(Solution& sol, int idConnection, vector<double>& variables){
    double currentThroughput = sol.throughput, bestThroughput = sol.throughput;
    bool inserted = false, splited = false, created = false;
    Channel top, left, right, toBeCreated;
    vector<int> tsc(3, -1);

    for(int t = 0; t < sol.slots.size(); t++){
        for(int s = 0; s < sol.slots[t].spectrums.size(); s++){
            int freeSpec = sol.slots[t].spectrums[s].maxFrequency - sol.slots[t].spectrums[s].usedFrequency;
            if(freeSpec > 160) freeSpec = 160;
            else if (freeSpec > 80) freeSpec = 80;
            else if(freeSpec > 40) freeSpec = 40;
            
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
                // Calcular o novo throughput baseado no canal atual da iteração (t, s, c)
                Channel toInsert = insertInChannel(sol.slots[t].spectrums[s].channels[c], idConnection);
                double aux = (currentThroughput - sol.slots[t].spectrums[s].channels[c].throughput) + toInsert.throughput;
                if(aux > bestThroughput){
                    splited = false;
                    bestThroughput = aux;
                    top = toInsert;
                    inserted = true;
                    tsc = {t, s, c};
                }

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
        // sol.slots[tsc[0]].spectrums[tsc[1]].usedFrequency += top.bandwidth;
    } else if(inserted && splited){
        sol.throughput = bestThroughput;
        sol.slots[tsc[0]].spectrums[tsc[1]].channels.erase(sol.slots[tsc[0]].spectrums[tsc[1]].channels.begin() + tsc[2]);
        sol.slots[tsc[0]].spectrums[tsc[1]].channels.push_back(left);
        sol.slots[tsc[0]].spectrums[tsc[1]].channels.push_back(right);
        // sol.slots[tsc[0]].spectrums[tsc[1]].usedFrequency += left.bandwidth + right.bandwidth;
    } else if(created){
        sol.throughput = bestThroughput;
        sol.slots[tsc[0]].spectrums[tsc[1]].channels.push_back(toBeCreated);
        sol.slots[tsc[0]].spectrums[tsc[1]].usedFrequency += toBeCreated.bandwidth;
    }

}

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

    Solution sol ({spec1, spec2, spec3}, 0.0);
    double totalSpectrum = 500.0, totalUsed = 0.0;

    int i = 0;
    if(type == 0 || type == 1 || type == 3 || type == 4){

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

        if(type == 1 || (type == 3 && refine) || type == 4){
            Solution temp = dp(sol);
            
            if (temp.throughput > sol.throughput) { 
                #pragma omp atomic
                count_improvements++;
            }
            // cout << temp.throughput << endl;
            sol = temp;
            if(type == 4) variables = rebuildChromossome(sol, n, variables);            
        }
        
    } else if(type == 2) {
        while(i < links.size() && totalUsed < totalSpectrum){
            int connection = links[i][1]/2;
            totalUsed += insertLessInterference(sol, connection, variables);
            i++;
        }   
        
        while(i < links.size()){
            int connection = links[i][1]/2;
            insertBestInterference(sol, connection, variables);
            i++;
        } 
        
        sol = applyFusion(sol);
    } else if (type == 5){
        while(i < links.size() && totalUsed < totalSpectrum){
            int connection = links[i][1]/2;
            insertInBestChannel(sol, connection, variables);
            i++;
        }
    }

    if (!checkFeasibility(sol, n/2)) {
        cout << "Solucao Inviável Detectada!" << endl;
        exit(1); 
    }

    return -1.0 * sol.throughput;
}