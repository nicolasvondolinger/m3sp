#include "../include/common.h"
#include "../include/brkga.h"
#include "../include/decoder.h"
#include "../include/MTRand.h"

namespace fs = std::filesystem;

int populationSize;
int numberVariables;
double maximumTime;

bool first = true;

const unsigned p = 100;
const double pe = 0.25;
const double pm = 0.05;
const double rhoe = 0.70;
const unsigned K = 1;
const unsigned MAXT = omp_get_max_threads();
const unsigned X_INTVL = 100;
const unsigned X_NUMBER = 2;
const unsigned MAX_GENS = 1000;

void init(const fs::path& instancePath, FILE **solutionFile, FILE **objectivesFile, FILE **timeFile, FILE **populationFile) {
    if (!instancePath.empty()) {
        fprintf(stderr, "trying to open input file %s\n", instancePath.c_str());
        freopen(instancePath.c_str(), "r", stdin);
    }

    if (stdin == nullptr) {
        fprintf(stderr, "error opening input file (stdin)\n");
        exit(13);
    }

    loadData();
    populationSize = 100;
    numberVariables = 2 * nConnections;

    string outputDir;

    ostringstream oss; oss << (pe * 100); 
    string aux = oss.str();
    if(type == 0) outputDir = "../output/output_" + aux + '/' + to_string(nConnections);
    else if(type == 1) outputDir = "../output/output_dp_" + aux + '/' + to_string(nConnections);
    else if(type == 2) outputDir = "../output/output_fixed_dp_" + aux + '/' + to_string(nConnections);
    else if (type == 3) outputDir = "../output/output_random_dp_" + aux + '/' + to_string(nConnections);

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
}

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << stderr << "Choose type: Classic - 0 | Classic DP - 1 | Fixed DP - 2" << endl;
        exit(1);
    }

    type = stod(argv[1]);

    const fs::path instancesDir = "../instances";

    vector<int> prime_numbers = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29};
    int i = 0;
    for (const auto& entry : fs::directory_iterator(instancesDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            FILE *solutionFile = nullptr,  *objectivesFile = nullptr, *timeFile = nullptr, *populationFile = nullptr;

            init(entry.path(), &solutionFile, &objectivesFile, &timeFile, &populationFile);
            evaluations = 0;
            const unsigned n = numberVariables;

            Solution decoder;
            MTRand rng(prime_numbers[i]); i++;
            BRKGA<Solution, MTRand> algorithm(n, p, pe, pm, rhoe, decoder, rng, K, MAXT);
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
                refine = (rng.randInt(1) == 1);
                
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

            if (objectivesFile != nullptr) fprintf(objectivesFile, "%lf %lld\n", -1.0 * algorithm.getBestFitness(), evaluations);
            else {
                cout << stderr << "objectivesFiles is null!" << endl;  
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
        }

    }

    exit(0);
}
