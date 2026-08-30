#include "m3sp/decoder/base.h"

static bool approximatelyEqual(double a, double b, double epsilon = EPS) {
    return fabs(a - b) <= ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

static int bandIndex(int band){
    if(band == 40) return 1;
    else if(band == 80) return 2;
    else if(band == 160) return 3;
    return 0;
}

// Pure form of the MCS lookup: the rate link `id` sustains on a `band` MHz
// channel while seeing `interference`. Exposed so a decoder can price a
// candidate placement without materialising a Channel first;
// computeConnectionThroughput below remains the only writer of Connection state.
double linkThroughput(int id, double interference, int band){
    if(band == 0) return 0.0;

    if(approximatelyEqual(interference, 0.0, EPS)) return dataRates[11][bandIndex(band)];

    const double sinr = affectance[id][id] / (interference + noise);
    int MCS = 11;
    while(MCS>=0 && sinr < SINR[MCS][bandIndex(band)]) MCS--;

    return MCS < 0 ? 0.0 : dataRates[MCS][bandIndex(band)];
}

static double computeConnectionThroughput(Connection& connection, int band){
    if(band == 0) return 0;

    if(!approximatelyEqual(connection.interference, 0.0, EPS))
        connection.SINR = affectance[connection.id][connection.id] / (connection.interference + noise);

    connection.throughput = linkThroughput(connection.id, connection.interference, band);
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

Solution applyFusion(Solution sol) {
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

static pair<double, vector<Channel>> dfs(Channel channel){
    if(channel.bandwidth <= 20) return {channel.throughput, {channel}};

    double newBand = channel.bandwidth / 2.0;
    Channel a(newBand), b(newBand);

    int n = channel.connections.size();
    if (n == 0) return {0.0, {}};
    else if (n == 1) return {channel.throughput, {channel}};

    vector<int> connection_ids;
    for(const auto& conn : channel.connections) connection_ids.pb(conn.id);

    int n_conn = connection_ids.size();
    MTRand& generator = decoderRng();
    for (int i = n_conn - 1; i > 0; --i) {
        int j = generator.randInt(i);
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
