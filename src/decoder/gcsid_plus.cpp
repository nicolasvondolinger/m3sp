#include "m3sp/decoder/gcsid_plus.h"

// GCSID+ : mesma heuristica do GCSID, executada de outro jeito.
//
// A busca e identica linha a linha -- mesma ordem dos links, mesmas tres acoes
// avaliadas (abrir canal no espectro livre, entrar num canal existente, entrar
// e dividir), mesmo embaralhamento, mesma distribuicao alternada na divisao,
// mesmos criterios de desempate. A unica diferenca e COMO cada candidato e
// avaliado:
//
//   GCSID   copia o Channel inteiro (insertInChannel recebe por valor),
//           insere, e reconstroi o throughput -- uma alocacao por candidato.
//
//   GCSID+  le a interferencia ja acumulada em cada membro e faz uma unica
//           passada linear, sem copiar nada e sem alocar.
//
// Como as somas sao acumuladas na mesma ordem, o resultado e identico bit a
// bit: os dois decoders devem produzir o mesmo throughput para o mesmo
// cromossomo e a mesma semente. A diferenca aparece so no tempo.

namespace {

// ---------------------------------------------------------------------------
// Estrutura de dados: canal em construcao
// ---------------------------------------------------------------------------
// Membros do canal mais a interferencia que cada um ja recebe dos outros.
// Manter essa soma corrente e o que torna a insercao linear: os valores par a
// par ja estao em `affectance`, e o acumulado de cada membro e atualizado no
// lugar em vez de ser reconstruido do zero a cada candidato.
struct ChannelView {
    int bandwidth = 0;
    vector<int> ids;
    vector<double> interference;  // interference[k] e o que ids[k] recebe

    // clear() preserva a capacidade: depois das primeiras chamadas nenhuma
    // destas views aloca de novo.
    void reset(int bw) {
        bandwidth = bw;
        ids.clear();
        interference.clear();
    }
};

// Acrescenta `extra` atualizando SO a interferencia. Na particao alternada o
// lado de cada membro e decidido pela paridade do indice, entao o throughput
// intermediario nunca e consultado -- calcula-lo a cada insercao seria O(k^2)
// varreduras de MCS jogadas fora.
void addMember(ChannelView& view, int extra) {
    double extraInterference = 0.0;

    for (size_t k = 0; k < view.ids.size(); k++) {
        view.interference[k] += affectance[view.ids[k]][extra];
        extraInterference += affectance[extra][view.ids[k]];
    }

    view.ids.push_back(extra);
    view.interference.push_back(extraInterference);
}

// Throughput da view fechada. A soma percorre os membros na ordem de insercao,
// que e a mesma ordem em que insertInChannel acumula -- por isso o resultado
// bate bit a bit com o do GCSID.
double totalThroughput(const ChannelView& view) {
    double total = 0.0;

    for (size_t k = 0; k < view.ids.size(); k++)
        total += linkThroughput(view.ids[k], view.interference[k], view.bandwidth);

    return total;
}

// Quanto a view valeria com `extra` dentro, sem alterar nada. Mesma ordem de
// acumulacao de commitInsertion, entao os dois concordam bit a bit.
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

// Quanto um canal ja existente na solucao valeria com `extra` dentro. Le a
// interferencia armazenada em cada Connection, entao nao copia o Channel nem
// altera coisa alguma -- este e o caminho quente do decoder.
double priceInsertion(const Channel& channel, int extra) {
    double total = 0.0, extraInterference = 0.0;

    for (const auto& conn : channel.connections) {
        total += linkThroughput(conn.id, conn.interference + affectance[conn.id][extra],
                                channel.bandwidth);
        extraInterference += affectance[extra][conn.id];
    }

    return total + linkThroughput(extra, extraInterference, channel.bandwidth);
}

// Materializa um subcanal a partir dos ids escolhidos. A interferencia de um
// membro e uma soma sobre os outros, entao repetir as insercoes nessa ordem
// reproduz exatamente o estado que foi avaliado.
Channel buildChannel(const vector<int>& ids, int bandwidth) {
    Channel channel(bandwidth);
    for (int id : ids) channel = insertInChannel(channel, id);
    return channel;
}

// ---------------------------------------------------------------------------
// Um passo da construcao
// ---------------------------------------------------------------------------
enum class Placement { None, Create, Join, Split };

struct Split {
    vector<int> left, right;
};

void insertInBestChannelPlus(Solution& sol, int idConnection) {
    const double currentThroughput = sol.throughput;
    double bestThroughput = sol.throughput;

    Placement best = Placement::None;
    int bestSlot = -1, bestSpectrum = -1, bestChannel = -1, createBand = 0;
    Split bestSplit;

    // Buffers reaproveitados entre candidatos e entre links. thread_local
    // porque o BRKGA decodifica a populacao em paralelo (OpenMP).
    thread_local vector<int> ids;
    thread_local ChannelView a, b;

    for (int t = 0; t < (int)sol.slots.size(); t++) {
        for (int s = 0; s < (int)sol.slots[t].spectrums.size(); s++) {
            const Spectrum& spectrum = sol.slots[t].spectrums[s];

            int freeSpec = spectrum.maxFrequency - spectrum.usedFrequency;
            if (freeSpec >= 160) freeSpec = 160;
            else if (freeSpec >= 80) freeSpec = 80;
            else if (freeSpec >= 40) freeSpec = 40;

            // Acao A: abrir um canal novo. Sozinho nele, o link nao ve
            // interferencia nenhuma.
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

                // Acao B: entrar no canal como ele esta.
                const double joined = without + priceInsertion(channel, idConnection);
                if (joined > bestThroughput) {
                    best = Placement::Join;
                    bestThroughput = joined;
                    bestSlot = t; bestSpectrum = s; bestChannel = c;
                }

                // Acao C: entrar e dividir. 20 MHz e o canal mais estreito
                // que existe, entao nao ha o que dividir.
                if (channel.bandwidth == 20) continue;

                // Divide primeiro: so os membros que JA estavam no canal.
                ids.clear();
                for (const auto& conn : channel.connections) ids.push_back(conn.id);

                // Mesmo Fisher-Yates do GCSID: consome a mesma sequencia do rng.
                MTRand& generator = decoderRng();
                for (int k = (int)ids.size() - 1; k > 0; --k)
                    std::swap(ids[k], ids[generator.randInt(k)]);

                a.reset(channel.bandwidth / 2);
                b.reset(channel.bandwidth / 2);

                for (size_t k = 0; k < ids.size(); k++) {
                    if (k % 2 == 0) addMember(a, ids[k]);
                    else addMember(b, ids[k]);
                }

                // Uma unica varredura de MCS por metade, no fim.
                const double thrA = totalThroughput(a);
                const double thrB = totalThroughput(b);

                // Só agora testa em qual das metades o link novo rende mais.
                const double opt1 = priceInsertion(a, idConnection) + thrB;
                const double opt2 = thrA + priceInsertion(b, idConnection);

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

    // Nenhuma acao melhora a solucao: o link fica sem alocacao, como no GCSID.
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
        // As duas metades substituem o original, entao a ocupacao do espectro
        // nao muda e usedFrequency fica como esta.
        const int half = spectrum.channels[bestChannel].bandwidth / 2;
        spectrum.channels.erase(spectrum.channels.begin() + bestChannel);
        spectrum.channels.emplace_back(buildChannel(bestSplit.left, half));
        spectrum.channels.emplace_back(buildChannel(bestSplit.right, half));
    }
}

}  // namespace

void runGcsidPlus(Solution& sol, vector<vector<double>>& links, vector<double>& variables) {
    (void)variables;  // como no GCSID, a segunda chave do cromossomo nao e lida

    for (const auto& link : links) insertInBestChannelPlus(sol, (int)link[1] / 2);

    // Comentado para acompanhar o estado atual do GCSID (gcsid.cpp:96).
    // sol = applyFusion(sol);
}
