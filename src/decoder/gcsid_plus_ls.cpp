#include "m3sp/decoder/gcsid_plus_ls.h"

// Busca local do GCSID+ (VND com duas vizinhancas).
//
// N1 -- realocacao. Um link pode
//   - sair do canal em que esta e entrar em qualquer outro canal,
//   - sair da solucao, ou
//   - se esta fora da solucao, entrar em qualquer canal.
//   Para cada link, aplica o melhor desses movimentos se ele aumenta o
//   throughput.
//
// N2 -- troca com vizinhos. Para um link escalonado x e cada um dos seus
//   vizinhos u (os links com quem x mais interfere), avalia
//   - substituir x por u no canal de x, se u esta fora da solucao, ou
//   - trocar x e u de canal, se u esta em outro canal,
//   e aplica a melhor troca que aumenta o throughput. Vizinhos tem quase a
//   mesma "pegada" de interferencia, entao a troca reacomoda os dois sem
//   desarrumar o resto do canal -- algo que N1 nao alcanca, porque move um
//   link de cada vez.
//
// Ordem: N1 ate o otimo local, depois N2; se N2 melhorou, volta a N1. Os links
// sao visitados na ordem das chaves do cromossomo, a mesma da construcao.
//
// Tres coisas tornam isso barato o bastante para rodar em todo decode:
//
//   Avaliacao incremental. Mexer em um link so muda a interferencia dos
//   membros dos canais envolvidos, e a taxa de cada membro e uma funcao em
//   degraus da interferencia. Cada link guarda quanto de interferencia ainda
//   cabe antes de cair de degrau (slackDown) e quanto precisaria sair para
//   subir (slackUp); quase sempre a variacao fica dentro da folga e o membro
//   custa uma comparacao, sem busca de MCS.
//
//   Limitantes. Os membros de um canal so perdem quando alguem entra, entao o
//   ganho de uma entrada nunca passa da taxa do proprio link que entra -- e
//   essa taxa sai de J, a interferencia que cada link receberia em cada canal,
//   mantida incrementalmente. A maioria dos movimentos e descartada sem ser
//   avaliada.
//
//   Bits de "nao olhe" (como no 2-opt do TSP). Depois da primeira varredura,
//   so e reexaminado o link que esta perto de algo que mudou: o link movido,
//   o parceiro da troca e os vizinhos deles.
//
// Ao final, os canais sao reescritos com interferencia e throughput calculados
// na mesma ordem de soma que insertInChannel usa, entao a solucao devolvida tem
// exatamente os valores que checkFeasibility recalcula.

namespace {

// Ganho minimo para aceitar um movimento. As taxas da tabela sao multiplos de
// ~0.1 Mbps; abaixo disso so ha ruido de arredondamento. A mesma margem desfaz
// empates entre candidatos de forma deterministica.
constexpr double kEps = 1e-7;

// Vizinhos considerados por link em N2 (e marcados quando o link se move).
constexpr int kNeighbors = 32;

// Limites de seguranca; com os bits de "nao olhe" a busca para bem antes.
constexpr int kMaxRelocateSweeps = 50;
constexpr int kMaxSwapRounds = 10;

// ---------------------------------------------------------------------------
// Dados da instancia, compartilhados entre as threads
// ---------------------------------------------------------------------------
// A avaliacao precisa de affectance[y][x] para um x fixo e varios y, isto e,
// de uma COLUNA de affectance. A matriz e guardada por linhas, entao a coluna
// salta 16 KB por acesso; a transposta deixa esse acesso contiguo.
//
// main.cpp roda varias instancias no mesmo processo, recarregando affectance
// entre elas. Os dados sao refeitos quando a impressao digital da instancia
// muda; isso so acontece entre duas execucoes do BRKGA, nunca durante uma.
struct InstanceData {
    int n = -1;
    double fingerprint = 0.0;

    vector<double> causedOn;  // causedOn[x*n + y] = affectance[y][x]

    // limit[(x*4 + b)*16 + m]: maior interferencia com que x ainda alcanca
    // MCS >= m na banda b. Nao crescente em m; as posicoes 12..15 valem -1
    // (sentinelas), o que deixa levelAt sem testes de limite.
    vector<double> limit;

    double rate[4][12];  // rate[b][m] = dataRates[m][b]

    // neighbors[x*kNeighbors + j]: os links com maior interferencia mutua com
    // x (affectance[x][y] + affectance[y][x]), do mais forte ao mais fraco.
    vector<int> neighbors;

    static double fingerprintOf() {
        double f = nConnections + noise;
        for (int i = 0; i < nConnections; i++)
            f += affectance[i][i] * (1.0 + 1e-3 * i) + affectance[i][(7 * i + 1) % nConnections];
        return f;
    }

    void build(double fp) {
        n = nConnections;
        fingerprint = fp;

        causedOn.assign((size_t)n * n, 0.0);
        for (int y = 0; y < n; y++)
            for (int x = 0; x < n; x++) causedOn[(size_t)x * n + y] = affectance[y][x];

        for (int b = 0; b < 4; b++)
            for (int m = 0; m < 12; m++) rate[b][m] = dataRates[m][b];

        // linkThroughput escolhe o MAIOR m com SINR >= SINR[m][b]. Como a
        // tabela de SINR nao e monotona (ver MCS 8 em 40 MHz), o degrau m vale
        // enquanto a interferencia nao passa do maximo dos limites de m..11.
        limit.assign((size_t)n * 4 * 16, -1.0);
        for (int x = 0; x < n; x++)
            for (int b = 0; b < 4; b++) {
                double* L = &limit[((size_t)x * 4 + b) * 16];
                double best = -1.0;
                for (int m = 11; m >= 0; m--) {
                    best = max(best, affectance[x][x] / SINR[m][b] - noise);
                    L[m] = best;
                }
            }

        const int K = min(kNeighbors, n - 1);
        neighbors.assign((size_t)n * kNeighbors, -1);
        vector<pair<double, int>> mutual(n);
        for (int x = 0; x < n; x++) {
            for (int y = 0; y < n; y++)
                mutual[y] = {y == x ? -1.0 : affectance[x][y] + affectance[y][x], y};
            partial_sort(mutual.begin(), mutual.begin() + K, mutual.end(),
                         greater<pair<double, int>>());
            for (int j = 0; j < K; j++) neighbors[(size_t)x * kNeighbors + j] = mutual[j].second;
        }
    }
};

const InstanceData& instanceData() {
    static mutex lock;
    static InstanceData data;

    const double fp = InstanceData::fingerprintOf();
    lock_guard<mutex> guard(lock);
    if (data.n != nConnections || data.fingerprint != fp) data.build(fp);
    return data;
}

int bandIndexOf(int bandwidth) {
    if (bandwidth == 40) return 1;
    if (bandwidth == 80) return 2;
    if (bandwidth == 160) return 3;
    return 0;
}

// ---------------------------------------------------------------------------
// Estado da busca
// ---------------------------------------------------------------------------
class LocalSearch {
  public:
    void run(Solution& sol, const vector<vector<double>>& links);

  private:
    struct ChannelState {
        int band;            // indice da banda (0..3)
        vector<int> ids;     // membros
        int version;         // muda a cada entrada ou saida
        int departures;      // muda a cada saida
    };

    const InstanceData* data = nullptr;
    int n = 0;
    vector<ChannelState> channels;

    // Por link. channelOf = -1: fora da solucao. level = MCS atual (-1: sem
    // taxa; -2: desconhecido, o link acabou de entrar ou sair). O degrau atual
    // vale para interferencias em (stepLo, stepHi].
    vector<int> channelOf, position, level;
    vector<double> interference, rate, slackDown, slackUp, stepLo, stepHi;

    // J[k*n + x]: interferencia que x receberia no canal k (sem contar a si
    // mesmo). Mantida a cada entrada e saida.
    vector<double> J;

    // Variacao de remover x do seu canal, valida enquanto a versao do canal
    // for removalStamp[x].
    vector<double> removalCache;
    vector<int> removalStamp;

    // failStamp[k*n + x]: valor de departures[k] quando a entrada de x (fora da
    // solucao) em k foi avaliada e nao compensou. Entradas de outros links so
    // tendem a piorar a entrada de x; o que pode torna-la boa e alguem sair de k.
    vector<int> failStamp;

    // Bits de "olhe de novo" de cada vizinhanca.
    vector<char> lookRelocate, lookSwap;

    const double* limitsOf(int x, int b) const { return &data->limit[((size_t)x * 4 + b) * 16]; }

    // MCS de x com interferencia `interf` na banda b; -1 = sem taxa. Busca
    // binaria pelo maior m com interf <= L[m] (L e nao crescente): quatro
    // comparacoes, sem desvios.
    int levelAt(int x, int b, double interf) const {
        if (interf == 0.0) return 11;  // como linkThroughput: canal so seu
        const double* L = limitsOf(x, b);
        int m = -1;
        if (interf <= L[m + 8]) m += 8;
        if (interf <= L[m + 4]) m += 4;
        if (interf <= L[m + 2]) m += 2;
        if (interf <= L[m + 1]) m += 1;
        return m;
    }

    double rateAt(int b, int m) const { return m < 0 ? 0.0 : data->rate[b][m]; }

    // Taxa de x com uma interferencia tirada de J. A margem cobre o erro de
    // arredondamento das somas incrementais.
    double rateFromJ(int x, int b, double interf) const {
        return rateAt(b, levelAt(x, b, max(0.0, interf * (1.0 - 1e-9))));
    }

    // Atualiza a interferencia de x no canal de banda b. Quase sempre o MCS
    // nao muda; nesse caso so as folgas sao refeitas, sem tocar na tabela.
    void setInterference(int x, int b, double interf) {
        interference[x] = interf;
        const bool sameStep = level[x] >= -1 && interf != 0.0 && interf > stepLo[x] && interf <= stepHi[x];
        if (!sameStep) {
            const double* L = limitsOf(x, b);
            const int m = levelAt(x, b, interf);
            level[x] = m;
            rate[x] = rateAt(b, m);
            stepHi[x] = m < 0 ? numeric_limits<double>::infinity() : L[m];
            stepLo[x] = m >= 11 ? -numeric_limits<double>::infinity() : L[m + 1];
        }
        slackDown[x] = stepHi[x] - interf;
        slackUp[x] = interf - stepLo[x];
    }

    // Variacao de taxa de y quando sua interferencia muda de `shift`.
    double shiftDelta(int y, int b, double shift) const {
        if (shift > 0.0 ? shift <= slackDown[y] : -shift < slackUp[y]) return 0.0;
        return rateAt(b, levelAt(y, b, interference[y] + shift)) - rate[y];
    }

    // Marca x e seus vizinhos para serem reexaminados pelas duas vizinhancas.
    void touch(int x) {
        lookRelocate[x] = lookSwap[x] = 1;
        const int* near = &data->neighbors[(size_t)x * kNeighbors];
        for (int j = 0; j < kNeighbors && near[j] >= 0; j++)
            lookRelocate[near[j]] = lookSwap[near[j]] = 1;
    }

    void load(const Solution& sol);
    void recomputeInterference();
    double removal(int x);
    double insertionDelta(int x, int k, double room) const;
    double replaceDelta(int k, int out, int in, double inInterference) const;
    void detach(int x);
    void attach(int x, int k);
    int relocateSweep(const vector<int>& order);
    int swapSweep(const vector<int>& order);
    void dropSilentLinks();
    void store(Solution& sol) const;
};

void LocalSearch::load(const Solution& sol) {
    n = nConnections;
    channels.clear();
    channelOf.assign(n, -1);
    position.assign(n, -1);
    level.assign(n, -2);
    interference.assign(n, 0.0);
    rate.assign(n, 0.0);
    slackDown.assign(n, 0.0);
    slackUp.assign(n, 0.0);
    stepLo.assign(n, 0.0);
    stepHi.assign(n, 0.0);
    removalCache.assign(n, 0.0);
    removalStamp.assign(n, -1);
    lookRelocate.assign(n, 1);
    lookSwap.assign(n, 1);

    for (const auto& slot : sol.slots)
        for (const auto& spectrum : slot.spectrums)
            for (const auto& channel : spectrum.channels) {
                ChannelState state;
                state.band = bandIndexOf(channel.bandwidth);
                state.version = state.departures = 0;
                for (const auto& conn : channel.connections) {
                    channelOf[conn.id] = channels.size();
                    position[conn.id] = state.ids.size();
                    state.ids.push_back(conn.id);
                }
                channels.push_back(std::move(state));
            }

    const size_t cells = channels.size() * (size_t)n;
    J.assign(cells, 0.0);
    failStamp.assign(cells, -1);

    for (int k = 0; k < (int)channels.size(); k++) {
        double* Jk = &J[(size_t)k * n];
        for (int y : channels[k].ids) {
            const double* caused = &data->causedOn[(size_t)y * n];
            for (int x = 0; x < n; x++) Jk[x] += caused[x];
        }
    }
    recomputeInterference();
}

// Refaz as interferencias do zero, descartando o erro de arredondamento que as
// somas e subtracoes incrementais acumulam. Le affectance espalhada (uma linha
// por membro), entao e chamada so na carga e antes da fase de trocas; a solucao
// devolvida e recalculada do zero em store() de qualquer forma.
void LocalSearch::recomputeInterference() {
    for (const auto& ch : channels)
        for (int y : ch.ids) {
            double sum = 0.0;
            for (int z : ch.ids) if (z != y) sum += affectance[y][z];
            setInterference(y, ch.band, sum);
        }
    fill(removalStamp.begin(), removalStamp.end(), -1);
}

// Variacao do throughput se x sair do seu canal (com cache).
double LocalSearch::removal(int x) {
    const ChannelState& ch = channels[channelOf[x]];
    if (removalStamp[x] == ch.version) return removalCache[x];

    const double* caused = &data->causedOn[(size_t)x * n];
    double delta = -rate[x];
    for (int y : ch.ids) {
        if (y == x || caused[y] < slackUp[y]) continue;
        delta += rateAt(ch.band, levelAt(y, ch.band, interference[y] - caused[y])) - rate[y];
    }

    removalCache[x] = delta;
    removalStamp[x] = ch.version;
    return delta;
}

// Variacao do throughput se x (fora do canal k) entrar em k. Os membros so
// perdem com a chegada de x; quando a perda acumulada passa de `room` o
// movimento ja nao tem como compensar e a avaliacao para.
double LocalSearch::insertionDelta(int x, int k, double room) const {
    const ChannelState& ch = channels[k];
    const double* caused = &data->causedOn[(size_t)x * n];
    const double* received = affectance[x];
    double delta = 0.0, own = 0.0;

    for (int y : ch.ids) {
        own += received[y];
        if (caused[y] <= slackDown[y]) continue;
        delta += rateAt(ch.band, levelAt(y, ch.band, interference[y] + caused[y])) - rate[y];
        if (-delta >= room) return -numeric_limits<double>::infinity();
    }
    return delta + rateAt(ch.band, levelAt(x, ch.band, own));
}

// Variacao do throughput do canal k se `out` (membro) sair e `in` (de fora do
// canal) entrar no lugar. `inInterference` e o que `in` recebera no canal, ja
// sem `out`; vem de J, o que poupa percorrer a linha de affectance de `in`.
double LocalSearch::replaceDelta(int k, int out, int in, double inInterference) const {
    const ChannelState& ch = channels[k];
    const double* outCaused = &data->causedOn[(size_t)out * n];
    const double* inCaused = &data->causedOn[(size_t)in * n];
    double delta = -rate[out];

    for (int y : ch.ids)
        if (y != out) delta += shiftDelta(y, ch.band, inCaused[y] - outCaused[y]);

    return delta + rateAt(ch.band, levelAt(in, ch.band, max(0.0, inInterference)));
}

void LocalSearch::detach(int x) {
    const int k = channelOf[x];
    ChannelState& ch = channels[k];
    const double* caused = &data->causedOn[(size_t)x * n];

    const int p = position[x];
    ch.ids[p] = ch.ids.back();
    position[ch.ids[p]] = p;
    ch.ids.pop_back();
    ch.version++;
    ch.departures++;

    for (int y : ch.ids) setInterference(y, ch.band, interference[y] - caused[y]);

    double* Jk = &J[(size_t)k * n];
    for (int z = 0; z < n; z++) Jk[z] -= caused[z];

    channelOf[x] = position[x] = removalStamp[x] = -1;
    level[x] = -2;
    interference[x] = rate[x] = 0.0;
}

void LocalSearch::attach(int x, int k) {
    ChannelState& ch = channels[k];
    const double* caused = &data->causedOn[(size_t)x * n];
    const double* received = affectance[x];

    double own = 0.0;
    for (int y : ch.ids) {
        setInterference(y, ch.band, interference[y] + caused[y]);
        own += received[y];
    }

    channelOf[x] = k;
    position[x] = ch.ids.size();
    ch.ids.push_back(x);
    ch.version++;
    removalStamp[x] = -1;
    level[x] = -2;
    setInterference(x, ch.band, own);

    double* Jk = &J[(size_t)k * n];
    for (int z = 0; z < n; z++) Jk[z] += caused[z];
}

// N1: uma varredura de realocacao. Devolve o numero de movimentos feitos.
int LocalSearch::relocateSweep(const vector<int>& order) {
    const int K = channels.size();
    int moves = 0;

    for (int x : order) {
        if (!lookRelocate[x]) continue;
        const int from = channelOf[x];

        double base = 0.0, best = kEps;
        int target = -2;  // -2: fica onde esta; -1: sai da solucao; k: vai para k

        if (from >= 0) {
            base = removal(x);
            if (base > best) { best = base; target = -1; }
        }

        for (int k = 0; k < K; k++) {
            if (k == from) continue;

            int& stamp = failStamp[(size_t)k * n + x];
            if (from < 0 && stamp == channels[k].departures) continue;

            // Os membros de k so perdem com a chegada de x, entao o ganho nunca
            // passa da taxa do proprio x em k.
            const int b = channels[k].band;
            const double bound = base + rateFromJ(x, b, J[(size_t)k * n + x]);
            if (bound <= best + kEps) {
                if (from < 0) stamp = channels[k].departures;
                continue;
            }

            const double gain = base + insertionDelta(x, k, bound - best - kEps);
            if (gain > best + kEps) { best = gain; target = k; }
            else if (from < 0) stamp = channels[k].departures;
        }

        if (target == -2) {
            lookRelocate[x] = 0;
            continue;
        }
        if (from >= 0) detach(x);
        if (target >= 0) attach(x, target);
        touch(x);
        moves++;
    }
    return moves;
}

// N2: uma varredura de trocas com vizinhos. Devolve o numero de trocas feitas.
int LocalSearch::swapSweep(const vector<int>& order) {
    int moves = 0;

    for (int x : order) {
        if (!lookSwap[x]) continue;
        const int A = channelOf[x];
        if (A < 0) {
            lookSwap[x] = 0;
            continue;
        }

        const int bandA = channels[A].band;
        const double leaveA = removal(x);
        const double* xCaused = &data->causedOn[(size_t)x * n];  // xCaused[u] = affectance[u][x]
        double best = kEps;
        int partner = -1;

        for (int j = 0; j < kNeighbors; j++) {
            const int u = data->neighbors[(size_t)x * kNeighbors + j];
            if (u < 0) break;
            const int B = channelOf[u];
            if (B == A) continue;

            // Quem entra no lugar de x ve a interferencia do canal sem x; os
            // demais membros de A nunca ganham mais do que com a simples saida
            // de x. Isso limita o ganho do lado de A.
            const double uInA = J[(size_t)A * n + u] - xCaused[u];
            const double intoA = leaveA + rateFromJ(u, bandA, uInA);

            if (B < 0) {
                if (intoA <= best + kEps) continue;
                const double gain = replaceDelta(A, x, u, uInA);
                if (gain > best + kEps) { best = gain; partner = u; }
                continue;
            }

            // Do lado de B a estimativa ignora o que os membros de B ganham com
            // a saida de u (calcular isso custaria uma passada por B a cada
            // vizinho). E um filtro, nao um limitante: pode descartar uma troca
            // boa, mas raramente -- os vizinhos sao quase intercambiaveis.
            const int bandB = channels[B].band;
            const double xInB = J[(size_t)B * n + x] - affectance[x][u];
            const double intoB = rateFromJ(x, bandB, xInB) - rate[u];
            if (intoA + intoB <= best + kEps) continue;

            const double gainA = replaceDelta(A, x, u, uInA);
            if (gainA + intoB <= best + kEps) continue;

            const double gain = gainA + replaceDelta(B, u, x, xInB);
            if (gain > best + kEps) { best = gain; partner = u; }
        }

        if (partner < 0) {
            lookSwap[x] = 0;
            continue;
        }

        const int B = channelOf[partner];
        detach(x);
        if (B >= 0) {
            detach(partner);
            attach(x, B);
        }
        attach(partner, A);
        touch(x);
        touch(partner);
        moves++;
    }
    return moves;
}

// Um link sem taxa nenhuma (SINR abaixo do MCS 0) nao transmite, mas continua
// interferindo nos vizinhos de canal. Tira-lo nunca reduz o throughput: ele
// nao contribuia e os demais so perdem interferencia.
void LocalSearch::dropSilentLinks() {
    bool dropped = true;
    while (dropped) {
        dropped = false;
        for (int x = 0; x < n; x++)
            if (channelOf[x] >= 0 && rate[x] <= 0.0) { detach(x); dropped = true; }
    }
}

// Reescreve os canais da solucao. Os canais mantem largura e espectro; so os
// membros mudam. A interferencia de cada membro e somada sobre os demais na
// ordem do canal, a mesma ordem que insertInChannel acumula e que
// checkFeasibility recalcula, e a taxa sai de linkThroughput.
void LocalSearch::store(Solution& sol) const {
    int k = 0;
    sol.throughput = 0.0;
    for (auto& slot : sol.slots)
        for (auto& spectrum : slot.spectrums)
            for (auto& channel : spectrum.channels) {
                const vector<int>& ids = channels[k++].ids;
                channel.connections.clear();
                channel.throughput = channel.violation = 0.0;

                for (size_t i = 0; i < ids.size(); i++) {
                    Connection conn(ids[i], 0.0, 0.0, distanceMatrix[ids[i]][ids[i]]);
                    for (size_t j = 0; j < ids.size(); j++)
                        if (j != i) conn.interference += affectance[ids[i]][ids[j]];
                    if (conn.interference != 0.0)
                        conn.SINR = affectance[conn.id][conn.id] / (conn.interference + noise);
                    conn.throughput = linkThroughput(conn.id, conn.interference, channel.bandwidth);
                    channel.throughput += conn.throughput;
                    channel.connections.push_back(conn);
                }
                sol.throughput += channel.throughput;
            }
}

void LocalSearch::run(Solution& sol, const vector<vector<double>>& links) {
    data = &instanceData();
    load(sol);

    thread_local vector<int> order;
    order.clear();
    for (const auto& link : links) order.push_back((int)link[1] / 2);

    auto descend = [&]() {
        for (int s = 0; s < kMaxRelocateSweeps; s++)
            if (relocateSweep(order) == 0) break;
    };

    descend();
    recomputeInterference();
    for (int round = 0; round < kMaxSwapRounds; round++) {
        if (swapSweep(order) == 0) break;
        descend();
    }
    dropSilentLinks();

    Solution improved = sol;
    store(improved);

    // Rede de seguranca: a busca decide com somas incrementais; se o valor
    // exato reconstruido nao superar o da construcao, fica a solucao
    // construida, so sem os links mudos.
    if (improved.throughput >= sol.throughput) {
        sol = std::move(improved);
        return;
    }
    load(sol);
    dropSilentLinks();
    store(sol);
}

}  // namespace

void localSearchGcsidPlus(Solution& sol, const vector<vector<double>>& links) {
    thread_local LocalSearch search;
    search.run(sol, links);
}
