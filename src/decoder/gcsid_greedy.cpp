#include "m3sp/decoder/gcsid_greedy.h"

// GCSID guloso: igual ao GCSID+ em tudo (mesma varredura, mesma estrutura de
// dados incremental, divide-primeiro-insere-depois), exceto na regra que
// reparte os membros que ja estavam no canal:
//
//   GCSID+   embaralha e alterna -- membro par vai pra metade A, impar pra B.
//   este     embaralha e coloca um a um, cada membro na metade onde ganha mais.
//
// O link novo continua sendo colocado gulosamente nos dois, entao a unica
// variavel que muda em relacao ao GCSID+ e a particao dos existentes. Como a
// regra e diferente, este decoder NAO reproduz os numeros do GCSID/GCSID+.
//
// Este e o ponto de partida para o trabalho seguinte.

namespace {

// Canal em construcao: membros mais a interferencia que cada um ja recebe dos
// outros. Manter a soma corrente e o que torna cada insercao linear.
struct ChannelView {
    int bandwidth = 0;
    double throughput = 0.0;
    vector<int> ids;
    vector<double> interference;  // interference[k] e o que ids[k] recebe
};

// Quanto a view valeria com `extra` dentro, sem alterar nada.
double priceInsertion(const ChannelView& view, int extra) {
    double total = 0.0, extraInterference = 0.0;

    for (size_t k = 0; k < view.ids.size(); k++) {
        total += linkThroughput(view.ids[k],
                                view.interference[k] + affectance[view.ids[k]][extra],
                                view.bandwidth);
        extraInterference += affectance[extra][view.ids[k]];
    }

    return total + linkThroughput(extra, extraInterference, view.bandwidth);
}

// Efetiva uma insercao que priceInsertion ja avaliou: so as somas de
// interferencia precisam ser tocadas, o throughput ja foi calculado.
void commitInsertion(ChannelView& view, int extra, double pricedThroughput) {
    double extraInterference = 0.0;

    for (size_t k = 0; k < view.ids.size(); k++) {
        view.interference[k] += affectance[view.ids[k]][extra];
        extraInterference += affectance[extra][view.ids[k]];
    }

    view.ids.push_back(extra);
    view.interference.push_back(extraInterference);
    view.throughput = pricedThroughput;
}

// Quanto um canal da solucao valeria com `extra` dentro, sem copia-lo.
double priceInsertion(const Channel& channel, int extra) {
    double total = 0.0, extraInterference = 0.0;

    for (const auto& conn : channel.connections) {
        total += linkThroughput(conn.id, conn.interference + affectance[conn.id][extra],
                                channel.bandwidth);
        extraInterference += affectance[extra][conn.id];
    }

    return total + linkThroughput(extra, extraInterference, channel.bandwidth);
}

Channel buildChannel(const vector<int>& ids, int bandwidth) {
    Channel channel(bandwidth);
    for (int id : ids) channel = insertInChannel(channel, id);
    return channel;
}

enum class Placement { None, Create, Join, Split };

struct Split {
    vector<int> left, right;
};

void insertInBestChannelGreedy(Solution& sol, int idConnection) {
    const double currentThroughput = sol.throughput;
    double bestThroughput = sol.throughput;

    Placement best = Placement::None;
    int bestSlot = -1, bestSpectrum = -1, bestChannel = -1, createBand = 0;
    Split bestSplit;

    for (int t = 0; t < (int)sol.slots.size(); t++) {
        for (int s = 0; s < (int)sol.slots[t].spectrums.size(); s++) {
            const Spectrum& spectrum = sol.slots[t].spectrums[s];

            int freeSpec = spectrum.maxFrequency - spectrum.usedFrequency;
            if (freeSpec >= 160) freeSpec = 160;
            else if (freeSpec >= 80) freeSpec = 80;
            else if (freeSpec >= 40) freeSpec = 40;

            if (freeSpec != 0) {
                const double candidate =
                    currentThroughput + linkThroughput(idConnection, 0.0, freeSpec);
                if (candidate > bestThroughput) {
                    best = Placement::Create;
                    bestThroughput = candidate;
                    bestSlot = t; bestSpectrum = s; createBand = freeSpec;
                }
            }

            for (int c = 0; c < (int)spectrum.channels.size(); c++) {
                const Channel& channel = spectrum.channels[c];
                const double without = currentThroughput - channel.throughput;

                const double joined = without + priceInsertion(channel, idConnection);
                if (joined > bestThroughput) {
                    best = Placement::Join;
                    bestThroughput = joined;
                    bestSlot = t; bestSpectrum = s; bestChannel = c;
                }

                if (channel.bandwidth == 20) continue;

                // Divide primeiro: so os membros que JA estavam no canal.
                vector<int> ids;
                ids.reserve(channel.connections.size());
                for (const auto& conn : channel.connections) ids.push_back(conn.id);

                MTRand& generator = decoderRng();
                for (int k = (int)ids.size() - 1; k > 0; --k)
                    std::swap(ids[k], ids[generator.randInt(k)]);

                ChannelView a, b;
                a.bandwidth = b.bandwidth = channel.bandwidth / 2;

                // Cada membro vai para a metade onde o ganho e maior. Cada um e
                // colocado uma unica vez e nunca reconsiderado, entao cada
                // colocacao custa uma passada linear.
                for (int id : ids) {
                    const double pricedA = priceInsertion(a, id);
                    const double pricedB = priceInsertion(b, id);

                    if (pricedA - a.throughput >= pricedB - b.throughput)
                        commitInsertion(a, id, pricedA);
                    else
                        commitInsertion(b, id, pricedB);
                }

                // Só agora testa em qual das metades o link novo rende mais.
                const double opt1 = priceInsertion(a, idConnection) + b.throughput;
                const double opt2 = a.throughput + priceInsertion(b, idConnection);

                const double halved = without + (opt1 >= opt2 ? opt1 : opt2);
                if (halved > bestThroughput) {
                    best = Placement::Split;
                    bestThroughput = halved;
                    bestSlot = t; bestSpectrum = s; bestChannel = c;

                    bestSplit.left = a.ids;
                    bestSplit.right = b.ids;
                    if (opt1 >= opt2) bestSplit.left.push_back(idConnection);
                    else bestSplit.right.push_back(idConnection);
                }
            }
        }
    }

    if (best == Placement::None) return;

    Spectrum& spectrum = sol.slots[bestSlot].spectrums[bestSpectrum];
    sol.throughput = bestThroughput;

    if (best == Placement::Create) {
        spectrum.channels.emplace_back(insertInChannel(Channel(createBand), idConnection));
        spectrum.usedFrequency += createBand;
    } else if (best == Placement::Join) {
        spectrum.channels[bestChannel] =
            insertInChannel(spectrum.channels[bestChannel], idConnection);
    } else {
        const int half = spectrum.channels[bestChannel].bandwidth / 2;
        spectrum.channels.erase(spectrum.channels.begin() + bestChannel);
        spectrum.channels.emplace_back(buildChannel(bestSplit.left, half));
        spectrum.channels.emplace_back(buildChannel(bestSplit.right, half));
    }
}

}  // namespace

void runGcsidGreedy(Solution& sol, vector<vector<double>>& links, vector<double>& variables) {
    (void)variables;  // como nos outros GCSID, a segunda chave nao e lida

    for (const auto& link : links) insertInBestChannelGreedy(sol, (int)link[1] / 2);

    // Comentado para acompanhar o estado atual do GCSID (gcsid.cpp).
    // sol = applyFusion(sol);
}
