//
// Created by José Joaquim on 03/03/20.
//

#include "HeuristicDecoder.h"

using namespace std;

static vector<Spectrum> init_conf;
static vector<int> spectrum_size;

int nConnections, nSpectrums;
vector<vector<double>> dataRates, SINR, beta;
double distanceMatrix[MAX_CONN][MAX_CONN], interferenceMatrix[MAX_CONN][MAX_CONN];
double senders[MAX_CONN][2], receivers[MAX_CONN][2];
double affectance[MAX_CONN][MAX_CONN];
double powerSender, alfa, noise, ttm;

random_device rd;
default_random_engine whatever = std::default_random_engine{rd()};
MTRand rng;

bool approximatelyEqual(double a, double b, double epsilon = EPS) {
    return fabs(a - b) <= ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

bool essentiallyEqual(double a, double b, double epsilon = EPS) {
    return fabs(a - b) <= ((fabs(a) > fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

bool definitelyGreaterThan(double a, double b, double epsilon = EPS) {
    return (a - b) > ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

bool definitelyLessThan(double a, double b, double epsilon = EPS) {
    return (b - a) > ((fabs(a) < fabs(b) ? fabs(b) : fabs(a)) * epsilon);
}

inline double distance(double X_si, double Y_si, double X_ri, double Y_ri) {
    return hypot((X_si - X_ri), (Y_si - Y_ri));
}

void distanceAndInterference() {
    for (int i = 0; i < nConnections; i++) {
        double X_si = receivers[i][X_c];
        double Y_si = receivers[i][Y_c];

        for (int j = 0; j < nConnections; j++) {

            double X_rj = senders[j][X_c];
            double Y_rj = senders[j][Y_c];

            double dist = distance(X_si, Y_si, X_rj, Y_rj);

            distanceMatrix[i][j] = dist;
        }
    }
}

double convertDBMToMW(double _value) {
    double result = 0.0, b;

    b = _value / 10.0;     // dBm dividido por 10
    result = pow(10.0, b); // Converte de DBm para mW

    return result;
}

void convertTableToMW(const vector<vector<double>> &_SINR, vector<vector<double>> &_SINR_Mw) {
    double result, b;
    for (int i = 0; i < _SINR_Mw.size(); i++) {
        for (int j = 0; j < _SINR_Mw[i].size(); j++) {

            if (_SINR[i][j] != 0) {
                b = _SINR[i][j] / 10.0; // dBm divided by 10
                result = pow(10.0, b);  // Convert DBM to mW

                _SINR_Mw[i][j] = result;
            } else {
                _SINR_Mw[i][j] = 0;
            }
        }
    }
}

void initTimeSlot() {
    for (Spectrum &sp : init_conf) {
        while (sp.maxFrequency - sp.usedFrequency > 0) {
            int bw = 160;
            while (bw > (sp.maxFrequency - sp.usedFrequency) && bw > 20) {
                bw /= 2;
            }

            if (bw <= (sp.maxFrequency - sp.usedFrequency)) {
                sp.usedFrequency += bw;
                sp.channels.emplace_back(0.0, 0.0, 0.0, bw, vector<Connection>());
            }
        }

        assert(sp.maxFrequency - sp.usedFrequency >= 0);
    }
}

void loadData() {
    scanf("%d %lf %lf %lf %d", &nConnections, &alfa, &noise, &powerSender, &nSpectrums);

    for (int i = 0; i < nSpectrums; i++) {
        int s;
        scanf("%d", &s);
        spectrum_size.emplace_back(s);
        init_conf.emplace_back(s, 0, vector<Channel>());
    }

    if (noise != 0) {
        noise = convertDBMToMW(noise);
    }

    for (int i = 0; i < nConnections; i++) {
        double a, b;
        scanf("%lf %lf", &a, &b);
        receivers[i][0] = a;
        receivers[i][1] = b;
    }

    for (int i = 0; i < nConnections; i++) {
        double a, b;
        scanf("%lf %lf", &a, &b);
        senders[i][0] = a;
        senders[i][1] = b;
    }

    for (int i = 0; i < nConnections; i++) {
        double bt;
        scanf("%lf", &bt);
#ifdef MDVRBSP
        gma.emplace_back(bt);
#endif
    }

    dataRates.assign(12, vector<double>(4, 0));
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 4; j++) {
            double a;
            scanf("%lf", &a);
            dataRates[i][j] = a;
        }
    }

    SINR.assign(12, vector<double>(4, 0));
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 4; j++) {
            double a;
            scanf("%lf", &a);
            SINR[i][j] = a;
        }
    }

    convertTableToMW(SINR, SINR);
    distanceAndInterference();
    initTimeSlot();

    for (int i = 0; i < nConnections; i++) {
        for (int j = 0; j < nConnections; j++) {
            double value = powerSender / pow(distanceMatrix[i][j], alfa);
            affectance[i][j] = value;
        }
    }

#ifdef MDVRBSP
    beta.assign(nConnections, vector<double>(4, 0));
    for (int i = 0; i < nConnections; i++) {

        for (int j = 0; j < 4; j++) {
            double value = gammaToBeta(gma[i], j);
            beta[i][j] = value;
        }
    }
#endif
}

int bwIdx(int bw) {
    if (bw == 40) {
        return 1;
    } else if (bw == 80) {
        return 2;
    } else if (bw == 160) {
        return 3;
    }
    return 0;
}

double computeConnectionThroughput(Connection &conn, int bandwidth) {
    if (bandwidth == 0)
        return 0.0;

    int mcs = -1;
    int maxDataRate = 11;

    if (approximatelyEqual(conn.interference, 0.0, EPS)) {
        mcs = maxDataRate;
        conn.throughput = dataRates[mcs][bwIdx(bandwidth)];
    } else {
        conn.SINR = affectance[conn.id][conn.id] / (conn.interference + noise);

        mcs = 11;
        while (mcs >= 0 && conn.SINR < SINR[mcs][bwIdx(bandwidth)])
            mcs--;

        if (mcs < 0)
            conn.throughput = 0.0;
        else
            conn.throughput = dataRates[mcs][bwIdx(bandwidth)];
    }
    return conn.throughput;
}

Channel insertInChannel(Channel newChannel, int idConn) {
    Connection conn(idConn, 0.0, 0.0, distanceMatrix[idConn][idConn]);

    for (Connection &connection : newChannel.connections) {
        connection.interference += affectance[connection.id][conn.id];
        conn.interference += affectance[conn.id][connection.id];
    }

    newChannel.connections.emplace_back(conn);
    newChannel.throughput = 0.0;
    newChannel.violation = 0.0;
    for (Connection &connection : newChannel.connections) {
        computeConnectionThroughput(connection, newChannel.bandwidth);
        newChannel.throughput += connection.throughput;
#ifdef MDVRBSP
#ifdef SUMVIO
        newChannel.violation += max(0.0, connection.thoughput, gma[connection.id]);
#else
        newChannel.violation =
            max(newChannel.violation, connection.throughput - gma[connection.id]);
#endif
#endif
    }

    return newChannel;
}

Channel deleteFromChannel(const Channel &channel, int idConn) {
    Channel newChannel(channel.bandwidth);

    for (const Connection &conn : channel.connections) {
        if (conn.id != idConn) {
            newChannel.connections.emplace_back(conn);
        }
    }

    newChannel.throughput = 0.0;
    newChannel.violation = 0.0;
    for (Connection &conn : newChannel.connections) {
        conn.interference -= affectance[conn.id][idConn];

        computeConnectionThroughput(conn, newChannel.bandwidth);
        newChannel.throughput += conn.throughput;
#ifdef MDVRBSP
#ifdef SUMVIO
        newChannel.violation += max(0.0, connection.thoughput, gma[connection.id]);
#else
        newChannel.violation = max(newChannel.violation, conn.throughput - gma[conn.id]);
#endif
#endif
    }

    return newChannel;
}

double computeThroughput(Solution &curr) {
    double OF = 0.0;

    for (int t = 0; t < curr.slots.size(); t++) {
        for (int s = 0; s < curr.slots[t].spectrums.size(); s++) {
            for (int c = 0; c < curr.slots[t].spectrums[s].channels.size(); c++) {

                double &chThroughput = curr.slots[t].spectrums[s].channels[c].throughput;
                int bw = curr.slots[t].spectrums[s].channels[c].bandwidth;
                chThroughput = 0.0;
                for (Connection &u : curr.slots[t].spectrums[s].channels[c].connections) {
                    u.interference = 0.0;
                    u.throughput = 0.0;
                    for (Connection &v : curr.slots[t].spectrums[s].channels[c].connections)
                        u.interference += u.id == v.id ? 0.0 : affectance[u.id][v.id];

                    chThroughput += computeConnectionThroughput(u, bw);
                }

                OF += chThroughput;
            }
        }
    }

    curr.throughput = OF;
    return OF;
}

void computeChannelsThroughput(vector<Channel> &channels) {
    for (Channel &channel : channels) {
        double of = 0.0;
        for (Connection &conn : channel.connections) {
            of += computeConnectionThroughput(conn, channel.bandwidth);
        }
        channel.throughput = of;
    }
}

vector<Channel> split(Channel toSplit, int spectrum) {
    int newBw = toSplit.bandwidth / 2;
    vector<Channel> ret;
    ret.emplace_back(Channel(0.0, 0.0, 0.0, newBw, vector<Connection>()));
    ret.emplace_back(Channel(0.0, 0.0, 0.0, newBw, vector<Connection>()));

    vector<Connection> connToInsert = toSplit.connections;
    sort(connToInsert.rbegin(), connToInsert.rend());

    double bestThroughputSoFar = 0.0;
    for (int i = 0; i < connToInsert.size(); i++) {
        double bestThroughputIteration = 0.0, testando = 0.0;
        Channel bestChannel(0.0, 0.0, 0.0, newBw, vector<Connection>());
        int bestIdx = -1;

        for (int c = 0; c < ret.size(); c++) {
            Channel inserted = insertInChannel(ret[c], connToInsert[i].id);
            double resultThroughput = inserted.throughput;

            double g_throughput = (bestThroughputSoFar - ret[c].throughput) + resultThroughput;
            testando = max(testando, g_throughput);
            bool one = g_throughput > bestThroughputIteration;
            bool two = approximatelyEqual(g_throughput, bestThroughputIteration) &&
                       (inserted.connections.size() < bestChannel.connections.size());

            if (one || two) {
                bestThroughputIteration = g_throughput;
                bestChannel = inserted;
                bestIdx = c;
            }
        }

        assert(bestIdx >= 0);
        ret[bestIdx] = bestChannel;
        bestThroughputSoFar = bestThroughputIteration;
    }
    computeChannelsThroughput(ret);
    assert(approximatelyEqual(ret[0].throughput + ret[1].throughput, bestThroughputSoFar));
    return ret;
}

int convertChromoBandwidth(double value) {
    if (value < .25)
        return 20;
    else if (value < .5)
        return 40;
    else if (value < .75)
        return 80;

    return 160;
}

// bool insertFreeChannel(Solution &sol, const int conn, const int bw, const vector<double> &vars,
//                        int &usedSpec) {
//     for (auto &slot : sol.slots) {
//         for (auto &spec : slot.spectrums) {
//             int free = spec.maxFrequency - spec.usedFrequency;
// 
//             if (free >= bw) {
//                 spec.usedFrequency += bw;
// 
//                 auto ch = insertInChannel(Channel(bw), conn);
//                 sol.throughput += ch.throughput;
//                 return true;
//             }
//         }
//     }
// 
//     return false;
// }

int insertFreeChannel(Solution &sol, int conn, int band, vector<double> &variables) {
    int freeSpec = 0, freeSpec2 = 0, idxSpectrum = -1, idxSlot = -1;
    int auxBw = -1;
    bool inserted = false;
    for (int t = 0; t < int(sol.slots.size()); t++) {
        for (int s = 0; s < sol.slots[t].spectrums.size(); s++) {
            freeSpec =
                sol.slots[t].spectrums[s].maxFrequency - sol.slots[t].spectrums[s].usedFrequency;

            if (freeSpec >= band) {
                sol.slots[t].spectrums[s].usedFrequency += band;

                Channel newChannel = insertInChannel(Channel(band), conn);
                sol.slots[t].spectrums[s].channels.emplace_back(newChannel);

                sol.throughput += newChannel.throughput;
                inserted = true;
                return band;
            } else if (freeSpec > freeSpec2) { // TODO: what is the condition?
                freeSpec2 = freeSpec;
                idxSpectrum = s;
                idxSlot = t;

                if (freeSpec2 >= 80) {
                    auxBw = 80;
                } else if (freeSpec2 >= 40) {
                    auxBw = 40;
                } else {
                    auxBw = 20;
                }
            }
        }
    }

    assert(auxBw != -1);
    if (!inserted && freeSpec2 > 0) {
        Channel newChannel = insertInChannel(Channel(auxBw), conn);
        sol.slots[idxSlot].spectrums[idxSpectrum].usedFrequency += auxBw;
        sol.slots[idxSlot].spectrums[idxSpectrum].channels.emplace_back(newChannel);
        sol.throughput += newChannel.throughput;

        if (auxBw != band) {
            switch (newChannel.bandwidth) {
            case 20:
                variables[(conn * 2) + 1] = (0 + 0.25) / 2.0;
                break;
            case 40:
                variables[(conn * 2) + 1] = (0.5 + 0.25) / 2.0;
                break;
            case 80:
                variables[(conn * 2) + 1] = (0.75 + 0.5) / 2.0;
                break;
            case 160:
                variables[(conn * 2) + 1] = (1.0 + 0.75) / 2.0;
                break;
            default:
                exit(77);
            }
        }
    }

    return auxBw;
}

void insertRemainingChannels(Solution &sol, const int conn, const int bw, vector<double> &vars) {
    double bestOF = sol.throughput;

    bool insert = false;
    Channel newCh(bw), *oldCh = nullptr;
    tuple<int, int, int> ch = {-1, -1, -1};

    // for (int t = 0; t < int(sol.slots.size()); ++t) {
    for (auto &slot : sol.slots) {
        // for (int s = 0; s < sols.slots[t].spectrums.size(); ++s)
        for (auto &spec : slot.spectrums) {
            for (auto &ch : spec.channels) {
                // for (int c = 0; c < sol.slots[t].spectrums[s].channels.size(); ++c) {
                // const auto &ch = sol.slots[t].spectrums[s].channels[c];
                auto auxCh = insertInChannel(ch, conn);

                double auxOF = sol.throughput - ch.throughput + auxCh.throughput;

                if (definitelyGreaterThan(auxOF, bestOF)) {
                    insert = true;
                    newCh = auxCh;
                    oldCh = &ch;
                    // ch = {t, s, c};
                }
            }
        }
    }

    if (insert) {
        sol.throughput = sol.throughput - oldCh->throughput + newCh.throughput;
        *oldCh = newCh;

        if (newCh.bandwidth != bw) {
            switch (newCh.bandwidth) {
            case 20:
                vars[(conn * 2) + 1] = (0 + 0.25) / 2.0;
                break;
            case 40:
                vars[(conn * 2) + 1] = (0.5 + 0.25) / 2.0;
                break;
            case 80:
                vars[(conn * 2) + 1] = (0.75 + 0.5) / 2.0;
                break;
            case 160:
                vars[(conn * 2) + 1] = (1.0 + 0.75) / 2.0;
                break;
            default:
                exit(77);
            }
        }
    }
}

bool insertBestFreeChannel(Solution &sol, const int conn, const int bw, vector<double> &vars,
                           int &totUsed) {

    Spectrum *spAdd = nullptr;
    int maxFreq = -1;

    for (auto &slot : sol.slots) {
        for (auto &spec : slot.spectrums) {
            int free = spec.maxFrequency - spec.usedFrequency;

            if (free > maxFreq) {
                spAdd = &spec;
                free = maxFreq;
            }
        }
    }

    if (spAdd != nullptr) {
        spAdd->channels.emplace_back(insertInChannel(Channel(maxFreq), conn));
        return true;
    }

    return false;
}

void insertBestChannel(Solution &sol, int conn, int band, vector<double> &variables) {
    // TODO: remind to check the output of this function.
    double currentThroughput = sol.throughput, bestThroughputIteration = sol.throughput;
    bool inserted = false;
    Channel newChannel(band);
    tuple<int, int, int> nCh = {-1, -1, -1};
    for (int t = 0; t < int(sol.slots.size()); t++) {
        for (int s = 0; s < sol.slots[t].spectrums.size(); s++) {
            for (int c = 0; c < sol.slots[t].spectrums[s].channels.size(); c++) {
                Channel channelInsert =
                    insertInChannel(sol.slots[t].spectrums[s].channels[c], conn);

                double auxThroughput = currentThroughput -
                                       sol.slots[t].spectrums[s].channels[c].throughput +
                                       channelInsert.throughput;

                if (auxThroughput > bestThroughputIteration) {
                    bestThroughputIteration = auxThroughput;
                    newChannel = channelInsert;
                    inserted = true;
                    nCh = {t, s, c};
                }
            }
        }
    }

    if (inserted) {
        sol.throughput =
            sol.throughput -
            sol.slots[get<0>(nCh)].spectrums[get<1>(nCh)].channels[get<2>(nCh)].throughput +
            newChannel.throughput;
        sol.slots[get<0>(nCh)].spectrums[get<1>(nCh)].channels[get<2>(nCh)] = newChannel;

        if (newChannel.bandwidth != band) {
            switch (newChannel.bandwidth) {
            case 20:
                variables[(conn * 2) + 1] = (0 + 0.25) / 2.0;
                break;
            case 40:
                variables[(conn * 2) + 1] = (0.5 + 0.25) / 2.0;
                break;
            case 80:
                variables[(conn * 2) + 1] = (0.75 + 0.5) / 2.0;
                break;
            case 160:
                variables[(conn * 2) + 1] = (1.0 + 0.75) / 2.0;
                break;
            default:
                exit(77);
            }
        }
    }
}

#ifdef MDVRBSP
double buildMDVRBSPSolution(vector<double> variables, vector<int> permutation) { return -1; }
#else
double buildVRBSPSolution(vector<double> variables, vector<int> permutation) {
    int totalSpectrum = 160 + 240 + 100, totalUsedSpectrum = 0.0;

    vector<Channel> auxCh;
    Spectrum spec1(160, 0, auxCh);
    Spectrum spec2(240, 0, auxCh);
    Spectrum spec3(100, 0, auxCh);
    Solution sol({spec1, spec2, spec3}, 0.0);

    // First, insert in free channels
    int idx = 0;
    while (idx < permutation.size() && totalUsedSpectrum < totalSpectrum) {
        int connection = permutation[idx] / 2;
        int bandWidth = convertChromoBandwidth(variables[permutation[idx] + 1]);
        totalUsedSpectrum += insertFreeChannel(sol, connection, bandWidth, variables);
        idx++;
    }

    // Second, insert in the best channels
    while (idx < permutation.size()) {
        int connection = permutation[idx] / 2;
        int bandWidth = convertChromoBandwidth(variables[permutation[idx] + 1]);
        insertBestChannel(sol, connection, bandWidth, variables);
        idx++;
    }

    double throughput = sol.throughput;
    return -1.0 * throughput;
}

// double buildVRBSPSolution(vector<double> vars, vector<int> perm) {
//     int totSpec = 500, totUsed = 0;
// 
//     vector<Channel> auxCh;
//     Spectrum spec1(160, 0, auxCh);
//     Spectrum spec2(240, 0, auxCh);
//     Spectrum spec3(100, 0, auxCh);
//     Solution sol({spec1, spec2, spec3}, 0.0);
// 
//     vector<int> leftOut;
// 
//     for (int i = 0; i < int(perm.size()) && totUsed < totSpec; ++i) {
//         int conn = perm[i] / 2;
//         int bw = convertChromoBandwidth(vars[perm[i] + 1]);
// 
//         if (!insertFreeChannel(sol, conn, bw, vars, totUsed))
//             leftOut.emplace_back(perm[i]);
//     }
// 
//     vector<int> leftOutAgain;
//     if (totUsed < totSpec) {
//         for (int i = 0; i < int(leftOut.size()) && totUsed < totSpec; ++i) {
//             int conn = leftOut[i] / 2;
//             int bw = convertChromoBandwidth(vars[leftOut[i] + 1]);
// 
//             if (!insertBestFreeChannel(sol, conn, bw, vars, totUsed))
//                 leftOutAgain.emplace_back(leftOut[i]);
//         }
//     } else
//         leftOutAgain = leftOut;
// 
//     for (int i = 0; i < int(leftOutAgain.size()); ++i) {
//         int conn = leftOutAgain[i] / 2;
//         int bw = convertChromoBandwidth(vars[leftOut[i] + 1]);
// 
//         insertRemainingChannels(sol, conn, bw, vars);
//     }
//     return -1.0 * sol.throughput;
// }
#endif

double Solution::decode(std::vector<double> variables) const {
    double fitness = 0.0;

    vector<pair<double, int>> ranking;
    for (int i = 0; i < variables.size(); i += 2)
        ranking.emplace_back(variables[i], i);

    sort(ranking.begin(), ranking.end());

    vector<int> permutation;
    for (int i = 0; i < ranking.size(); i++)
        permutation.emplace_back(ranking[i].second);

#ifdef MDVRBSP
    fitness = buildMDVRBSPSolution(variables, permutation);
#else
    fitness = buildVRBSPSolution(variables, permutation);
#endif
    return fitness;
}
