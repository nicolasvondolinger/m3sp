#include "../include/common.h"
#include "../include/brkga.h"
#include "../include/decoder.h"
#include "../include/MTRand.h"

namespace fs = std::filesystem;

int populationSize;
int numberVariables;
double maximumTime;

bool first = true;

void init(const fs::path& instancePath, FILE **solutionFile, FILE **objectivesFile, FILE **timeFile) {
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
    if(type) outputDir = "output_dp/" + to_string(nConnections);
    else outputDir = "output/" + to_string(nConnections);

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
}

int main(int argc, char **argv) {
    if (argc < 2) {
        cout << stderr << "Choose type: Normal - 0 | DP - 1" << endl;
        exit(1);
    }

    type = stod(argv[1]);

    const unsigned p = 100;
    const double pe = 0.25;
    const double pm = 0.05;
    const double rhoe = 0.70;
    const unsigned K = 1;
    const unsigned MAXT = 1;
    const unsigned X_INTVL = 100;
    const unsigned X_NUMBER = 2;
    // const unsigned MAX_GENS = 1000;

    const fs::path instancesDir = "instances";

    for (const auto& entry : fs::directory_iterator(instancesDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".txt") {
            FILE *solutionFile = nullptr,  *objectivesFile = nullptr, *timeFile = nullptr;

            init(entry.path(), &solutionFile, &objectivesFile, &timeFile);
            evaluations = 0;
            const unsigned n = numberVariables;

            Solution decoder;
            MTRand rng;
            BRKGA<Solution, MTRand> algorithm(n, p, pe, pm, rhoe, decoder, rng, K, MAXT);
            // cout << "AQUI: " << -algorithm.getBestFitness() << endl;
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

            auto startTime = std::chrono::steady_clock::now();

            // while (std::chrono::steady_clock::now() - startTime < DURATION_LIMIT) {
                for(int i = 0; i < 10; i++){
                    algorithm.evolve();

                    if ((++generation) % X_INTVL == 0) {
                        algorithm.exchangeElite(X_NUMBER);
                    }

                    if (algorithm.getBestFitness() < FO_Star) {
                        TempoFO_Star = (((double)(clock() - TempoFO_StarInic)) / CLOCKS_PER_SEC);
                        FO_Star = algorithm.getBestFitness();
                        bestGeneration = generation;
                        bestIteration = quantIteracoes; 
		   }
		    cout << i << endl;
                }
            // }

            TempoExecTotal = (((double)(clock() - TempoFO_StarInic)) / CLOCKS_PER_SEC);

            if (solutionFile != nullptr) {
                vector<double> best = algorithm.getBestChromosome();
                for (int i = 0; i < best.size(); i++) {
                    fprintf(solutionFile, "%lf ", best[i]);
                }
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
        }
    }

    exit(0);
}
