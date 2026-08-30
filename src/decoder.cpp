#include "m3sp/decoder.h"
#include "m3sp/decoder/mic.h"
#include "m3sp/decoder/dp.h"
#include "m3sp/decoder/gcsid.h"
#include "m3sp/decoder/gcsid_plus.h"
#include "m3sp/decoder/gcsid_greedy.h"
#include "m3sp/decoder/local_search.h"

double Solution::decode(vector<double>& variables) const {
    // O fluxo aleatorio deriva do cromossomo, entao decode() e determinístico
    // qualquer que seja a thread que o execute. Tem de vir antes de qualquer
    // sorteio e antes de o mic reescrever genes de banda.
    seedDecoderRngForChromosome(variables);

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

    if(type == "mic") runMic(sol, links, variables);
    else if(type == "dp") runDp(sol, links, variables);
    else if(type == "gcsid") runGcsid(sol, links, variables);
    else if(type == "gcsid_plus") runGcsidPlus(sol, links, variables);
    else if(type == "gcsid_greedy") runGcsidGreedy(sol, links, variables);
    else if(type == "local_search") runLocalSearch(sol, links, variables);

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
