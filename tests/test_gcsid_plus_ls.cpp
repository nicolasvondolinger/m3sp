// Busca local do GCSID+: a solucao devolvida tem de ser viavel, so com canais
// de 20, 40, 80 ou 160 MHz, sem links repetidos nem links escalonados sem taxa,
// e nunca pior que a solucao de partida.
//
// A instancia e gerada aqui mesmo (mesmo formato de det_instance_generator.py)
// e carregada pelo loadData() de verdade, entao o teste nao depende de arquivos
// do repositorio nem do diretorio em que o CTest o executa.

#include "m3sp/common.h"
#include "m3sp/decoder.h"
#include "m3sp/decoder/gcsid_plus.h"
#include "m3sp/decoder/gcsid_plus_ls.h"
#include "m3sp/decoder/mic.h"

namespace {

int failures = 0;

void check(bool ok, const string& what) {
    if (!ok) {
        failures++;
        cerr << "FALHA: " << what << "\n";
    }
}

// Escreve uma instancia VRBSP com `n` links num plano `dim` x `dim`, com o
// transmissor a no maximo 6*sqrt(2) do receptor, e a carrega.
void loadRandomInstance(int n, double dim, unsigned seed) {
    const double rates[12][4] = {
        {8.6, 17.2, 36.0, 72.1},       {17.2, 34.4, 72.1, 144.1},     {25.8, 51.6, 108.1, 216.2},
        {34.4, 68.8, 144.1, 288.2},    {51.6, 103.2, 216.2, 432.4},   {68.8, 137.6, 288.2, 576.5},
        {77.4, 154.9, 324.3, 648.5},   {86.0, 172.1, 360.3, 720.6},   {103.2, 206.5, 432.4, 864.7},
        {114.7, 229.4, 480.4, 960.8},  {129.0, 258.1, 540.4, 1080.9}, {143.4, 286.8, 600.5, 1201.0}};
    const int sinr[12][4] = {{2, 5, 8, 11},    {5, 8, 11, 14},   {7, 10, 13, 16},  {10, 13, 16, 19},
                             {14, 17, 20, 23}, {18, 21, 24, 27}, {19, 22, 25, 28}, {20, 23, 26, 29},
                             {25, 19, 31, 34}, {27, 30, 33, 36}, {30, 33, 36, 39}, {32, 35, 38, 41}};

    MTRand gen(seed);
    vector<array<double, 2>> rx(n), tx(n);
    for (int i = 0; i < n; i++) {
        rx[i] = {gen.rand() * dim, gen.rand() * dim};
        do {
            tx[i] = {gen.rand() * dim, gen.rand() * dim};
        } while (hypot(rx[i][0] - tx[i][0], rx[i][1] - tx[i][1]) > 6 * sqrt(2.0));
    }

    char path[] = "/tmp/m3sp_test_instance_XXXXXX";
    const int fd = mkstemp(path);
    check(fd >= 0, "criar arquivo temporario");
    FILE* f = fdopen(fd, "w");
    fprintf(f, "%d 3.0 0.0 1000.0 3 160 240 100\n", n);
    for (auto& p : rx) fprintf(f, "%.17g %.17g\n", p[0], p[1]);
    for (auto& p : tx) fprintf(f, "%.17g %.17g\n", p[0], p[1]);
    for (int i = 0; i < n; i++) fprintf(f, "8.6\n");
    for (auto& row : rates) fprintf(f, "%g %g %g %g\n", row[0], row[1], row[2], row[3]);
    for (auto& row : sinr) fprintf(f, "%d %d %d %d\n", row[0], row[1], row[2], row[3]);
    fclose(f);

    check(freopen(path, "r", stdin) != nullptr, "abrir a instancia gerada");
    loadData();
    remove(path);
}

// O mesmo preparo que Solution::decode() faz antes de chamar o decoder.
vector<vector<double>> sortedLinks(const vector<double>& chromosome) {
    vector<vector<double>> links(chromosome.size() / 2);
    for (size_t i = 0; i < chromosome.size(); i += 2) links[i / 2] = {chromosome[i], (double)i};
    sort(links.begin(), links.end());
    return links;
}

Solution emptySolution() {
    vector<Channel> none;
    return Solution({Spectrum(160, 0, none), Spectrum(240, 0, none), Spectrum(100, 0, none)}, 0.0);
}

// Confere o que checkFeasibility nao confere (ele so avisa sobre links
// repetidos, aceita link com taxa zero e nao olha a largura dos canais -- e
// bandIndex() precifica qualquer largura desconhecida como 20 MHz, entao um
// canal de 60 ou 120 MHz passaria em silencio) e refaz a soma do throughput.
void checkStrict(const Solution& sol, const string& tag) {
    check(checkFeasibility(sol, nConnections), tag + ": checkFeasibility");

    vector<int> seen(nConnections, 0);
    double total = 0.0;
    for (const auto& slot : sol.slots)
        for (const auto& spectrum : slot.spectrums) {
            int used = 0;
            for (const auto& channel : spectrum.channels) {
                const int bw = channel.bandwidth;
                check(bw == 20 || bw == 40 || bw == 80 || bw == 160,
                      tag + ": canal de " + to_string(bw) + " MHz");
                used += bw;
                for (const auto& conn : channel.connections) {
                    check(++seen[conn.id] == 1, tag + ": link repetido");
                    check(conn.throughput > 0.0, tag + ": link escalonado sem taxa");
                    total += conn.throughput;
                }
            }
            check(used <= spectrum.maxFrequency, tag + ": espectro estourado");
        }
    check(fabs(total - sol.throughput) <= 1e-6 * max(1.0, total), tag + ": throughput total");
}

}  // namespace

int main() {
    loadRandomInstance(256, 120.0, 7);
    seedDecoderRng(3);
    MTRand gen(11);

    for (int trial = 0; trial < 12; trial++) {
        vector<double> chromosome(2 * nConnections);
        for (double& key : chromosome) key = gen.rand();
        const string tag = "cromossomo " + to_string(trial);

        // GCSID+ completo (construcao + busca local), como decode() o executa.
        type = "gcsid_plus";
        vector<double> keys = chromosome;
        seedDecoderRngForChromosome(keys);
        vector<vector<double>> links = sortedLinks(keys);
        Solution plus = emptySolution();
        runGcsidPlus(plus, links, keys);
        checkStrict(plus, tag + " gcsid_plus");

        // decode() e deterministico e devolve o mesmo valor.
        Solution decoder;
        vector<double> again = chromosome;
        check(fabs(-decoder.decode(again) - plus.throughput) < 1e-9, tag + ": decode deterministico");

        // A busca local partindo de outra solucao viavel (a do MIC) nunca piora.
        type = "mic";
        keys = chromosome;
        seedDecoderRngForChromosome(keys);
        links = sortedLinks(keys);
        Solution mic = emptySolution();
        runMic(mic, links, keys);
        const double before = mic.throughput;
        localSearchGcsidPlus(mic, links);
        check(mic.throughput >= before - 1e-6, tag + ": busca local piorou a solucao do MIC");
        checkStrict(mic, tag + " mic + busca local");
    }

    if (failures == 0) cout << "ok\n";
    return failures == 0 ? 0 : 1;
}
