#include "m3sp/common.h"
#include "brkga.h"
#include "m3sp/decoder.h"

namespace fs = std::filesystem;

int numberVariables;
double maximumTime;

bool first = true;

const unsigned p = 100;
const double pe = 0.05; // Elite
const double pm = 0.25; // Bottom (Mutant)
const double rhoe = 0.70; // Probabilidade de Herdar da Elite
const unsigned K = 1;
const unsigned MAXT = omp_get_max_threads();
const unsigned X_INTVL = 100;
const unsigned X_NUMBER = 2;
const unsigned MAX_GENS = 1000;

// type "local_search": se true, aplica localSearchType6 nas pe elites a cada
// geracao (alem da LS final na melhor solucao).
const bool USE_ELITE_LS = true;

// Heuristicas aceitas em argv[1]; devem casar com o dispatch em decoder.cpp.
const vector<string> KNOWN_TYPES = {"mic", "dp", "gcsid", "gcsid_plus", "gcsid_greedy", "local_search"};

const fs::path INSTANCES_ROOT = "../instances";

// Conjunto de instancias escolhido em argv[2]: um subdiretorio de ../instances
// (ex.: "vrbsp", "md_vrbsp"). Entra no caminho de saida, senao rodadas de
// conjuntos diferentes sobrescreveriam os resultados umas das outras.
string instanceSet;

// Subdiretorios de ../instances, em ordem alfabetica. Descobertos em tempo de
// execucao: acrescentar um conjunto novo nao exige recompilar.
vector<string> availableInstanceSets() {
    vector<string> sets;
    if (!fs::exists(INSTANCES_ROOT)) return sets;
    for (const auto& entry : fs::directory_iterator(INSTANCES_ROOT))
        if (entry.is_directory()) sets.push_back(entry.path().filename().string());
    sort(sets.begin(), sets.end());
    return sets;
}

void usage(const vector<string>& sets) {
    cerr << "usage: m3sp <type> <instance-set>" << endl;

    cerr << "  type:         ";
    for (size_t i = 0; i < KNOWN_TYPES.size(); i++)
        cerr << KNOWN_TYPES[i] << (i + 1 < KNOWN_TYPES.size() ? " | " : "");
    cerr << endl;

    cerr << "  instance-set: ";
    if (sets.empty()) {
        cerr << "(no subdirectory found in " << INSTANCES_ROOT << ")";
    } else {
        for (size_t i = 0; i < sets.size(); i++)
            cerr << sets[i] << (i + 1 < sets.size() ? " | " : "");
    }
    cerr << endl;
}

void init(const fs::path& instancePath, FILE **solutionFile, FILE **objectivesFile, FILE **timeFile, FILE **populationFile, FILE **channelsFile) {
    if (!instancePath.empty()) {
        fprintf(stderr, "trying to open input file %s\n", instancePath.c_str());
        freopen(instancePath.c_str(), "r", stdin);
    }

    if (stdin == nullptr) {
        fprintf(stderr, "error opening input file (stdin)\n");
        exit(13);
    }

    loadData();
    numberVariables = 2 * nConnections;

    string outputDir;

    ostringstream oss; oss << (pe * 100); 
    string aux = oss.str();
    // ../output/<instance-set>/output_<type>_<pe>/<nConnections>
    const string base = "../output/" + instanceSet + "/output_";
    if(type == "mic") outputDir = base + "mic_" + aux + '/' + to_string(nConnections);
    else if(type == "dp") outputDir = base + "dp_" + aux + '/' + to_string(nConnections);
    else if(type == "gcsid") outputDir = base + "gcsid_" + aux + '/' + to_string(nConnections);
    else if(type == "gcsid_plus") outputDir = base + "gcsid_plus_" + aux + '/' + to_string(nConnections);
    else if(type == "gcsid_greedy") outputDir = base + "gcsid_greedy_" + aux + '/' + to_string(nConnections);
    else if(type == "local_search") outputDir = base + "local_search_" + aux + '/' + to_string(nConnections);

    try {
        if (fs::exists(outputDir) && first){
            fs::remove_all(outputDir);
            first = false;
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error cleaning up output directories: " << e.what() << endl;
        exit(1);
    }
    
    try {
        fs::create_directories(outputDir);
    } catch (const fs::filesystem_error& e) {
        fprintf(stderr, "Error creating directory %s: %s\n", outputDir.c_str(), e.what());
        exit(1);
    }

    string solFile = outputDir +  "/solution.txt";
    *solutionFile = fopen(solFile.c_str(), "a");

    string objFile = outputDir + "/objectives.txt";
    *objectivesFile = fopen(objFile.c_str(), "a");

    string tFile = outputDir + "/time.txt";
    *timeFile = fopen(tFile.c_str(), "a");

    string popFile = outputDir + "/population.txt";
    *populationFile = fopen(popFile.c_str(), "a");

    // Composicao de canais da solucao final: uma linha por instancia com "c20 c40 c80 c160".
    string chanFile = outputDir + "/channels.txt";
    *channelsFile = fopen(chanFile.c_str(), "a");
}

int main(int argc, char **argv) {
    const vector<string> instanceSets = availableInstanceSets();

    if (argc < 3) {
        usage(instanceSets);
        exit(1);
    }

    type = argv[1];

    if (find(KNOWN_TYPES.begin(), KNOWN_TYPES.end(), type) == KNOWN_TYPES.end()) {
        cerr << "unknown type '" << type << "'" << endl;
        usage(instanceSets);
        exit(1);
    }

    instanceSet = argv[2];

    if (find(instanceSets.begin(), instanceSets.end(), instanceSet) == instanceSets.end()) {
        cerr << "unknown instance set '" << instanceSet << "'" << endl;
        usage(instanceSets);
        exit(1);
    }

    // So faz sentido para o type "local_search" (unico decoder com localSearchType6).
    applyEliteLS = (type == "local_search") && USE_ELITE_LS;

    const fs::path instancesDir = INSTANCES_ROOT / instanceSet;

    vector<fs::path> instanceFiles;

    for (const auto& entry : fs::directory_iterator(instancesDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            instanceFiles.push_back(entry.path());
        }
    }

    if (instanceFiles.empty()) {
        cerr << "no .txt instance found in " << instancesDir << endl;
        exit(1);
    }

    sort(instanceFiles.begin(), instanceFiles.end());

    vector<int> prime_numbers = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29};
    int i = 0;

    for (const auto& entry : instanceFiles) {

        if (i >= prime_numbers.size()) {
            cout << "Aviso: Mais instâncias do que números primos definidos. Parando." << endl;
            break; 
        }
        
        FILE *solutionFile = nullptr,  *objectivesFile = nullptr, *timeFile = nullptr, *populationFile = nullptr, *channelsFile = nullptr;
        init(entry, &solutionFile, &objectivesFile, &timeFile, &populationFile, &channelsFile);
        evaluations = 0;
        const unsigned n = numberVariables;
        
        rng.seed(prime_numbers[i]);
        seedDecoderRng(prime_numbers[i]);  // um gerador por thread, mesma semente base
        i++;

        count_improvements = 0;
        Solution decoder;
        BRKGA<Solution, MTRand> algorithm(n, p, pe, pm, rhoe, decoder, K, MAXT);
        double TempoExecTotal = 0.0, TempoFO_Star = 0.0, FO_Star = 1000000007, FO_Min = -1000000007;
        int bestGeneration = 0, minGeneration = 0;
        int iterSemMelhora, iterMax = 10, quantIteracoes = 0, bestIteration = 0;

        clock_t TempoFO_StarInic;

        TempoFO_StarInic = clock();

        bestGeneration = 0;
        minGeneration = 0;

        iterSemMelhora = 0;
        iterMax = 1;

        quantIteracoes = 0;
        bestIteration = 0;

        unsigned generation = 0;
        const auto DURATION_LIMIT = std::chrono::minutes(5);

        // while (std::chrono::steady_clock::now() - startTime < DURATION_LIMIT) {
        for(int i = 0; i < 1000; i++){
            cout << "AQUI: " << i << endl;

            algorithm.evolve();

            if (populationFile != nullptr) {
                // Assume K=0 (apenas uma população ou a principal)
                // Itera sobre todos os indivíduos (tamanho p)
                for(unsigned k = 0; k < p; k++) {
                    // Obtém o fitness. Multiplica por -1.0 para obter o Throughput real
                    double val = -1.0 * algorithm.getPopulationFitness(0, k);
                    fprintf(populationFile, "%lf ", val);
                }
                fprintf(populationFile, "\n"); // Nova linha = Nova geração
            }

            if ((++generation) % X_INTVL == 0) {
                algorithm.exchangeElite(X_NUMBER);
            }

            if (algorithm.getBestFitness() < FO_Star) {
                TempoFO_Star = (((double)(clock() - TempoFO_StarInic)) / CLOCKS_PER_SEC);
                FO_Star = algorithm.getBestFitness();
                bestGeneration = generation;
                bestIteration = quantIteracoes; 
            }
            
        }
        // }

        TempoExecTotal = (((double)(clock() - TempoFO_StarInic)) / CLOCKS_PER_SEC);

        if (solutionFile != nullptr) {
            vector<double> best = algorithm.getBestChromosome();
            for (int i = 0; i < best.size(); i++) fprintf(solutionFile, "%lf ", best[i]);
            fprintf(solutionFile, "\n");
        } else {
            cout << stderr << "solutionFile is null!" << endl; 
            exit(13);
        }

        // type "local_search" (Pacote 1): aplica a busca local SÓ na melhor solução, ao final.
        // Aproveita esse decode da melhor solução para coletar a composição de canais
        // (channels.txt). Para os demais tipos, decodifica a melhor uma vez só p/ os canais.
        double finalFitness = algorithm.getBestFitness();
        {
            vector<double> bestChromo = algorithm.getBestChromosome();
            collectChannels = true;
            if (type == "local_search") applyFinalLS = true;
            double f = decoder.decode(bestChromo);   // preenche channelBandCount
            if (type == "local_search") { finalFitness = f; applyFinalLS = false; }
            collectChannels = false;
        }

        if (objectivesFile != nullptr) fprintf(objectivesFile, "%lf %lld %lld\n", -1.0 * finalFitness, evaluations, count_improvements);
        else {
            cout << stderr << "objectivesFiles is null!" << endl;
            exit(13);
        }

        // Uma linha por instancia: contagem de canais de 20/40/80/160 MHz na solucao final.
        if (channelsFile != nullptr) {
            fprintf(channelsFile, "%lld %lld %lld %lld\n",
                    channelBandCount[20], channelBandCount[40],
                    channelBandCount[80], channelBandCount[160]);
        } else {
            cout << stderr << "channelsFile is null!" << endl;
            exit(13);
        }

        if(timeFile != nullptr) fprintf(timeFile, "%lf\n", TempoExecTotal);
        else {
            cout << stderr << "timeFile is null!" << endl;
            exit(13);
        }

        fclose(solutionFile);
        fclose(objectivesFile);
        fclose(timeFile);
        fclose(populationFile);
        fclose(channelsFile);
    }
        
    exit(0);
}
