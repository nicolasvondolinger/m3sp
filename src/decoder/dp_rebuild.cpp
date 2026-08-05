#include "decoder/dp_rebuild.h"

static vector<double> rebuildChromossome(const Solution& sol, int nVariables, vector<double>& variables) {
    vector<double> newVariables = variables;

    int totalConnections = 0;
    for (const auto& slot : sol.slots)
        for (const auto& spec : slot.spectrums)
            for (const auto& channel : spec.channels)
                totalConnections += channel.connections.size();

    double priorityStep = 1.0 / (totalConnections + 1.0);
    double currentPriority = priorityStep;

    vector<double> channels_gene;
    vector<vector<Connection>> aux;

    for (const auto& slot : sol.slots) {
        for (const auto& spec : slot.spectrums) {
            for (const auto& channel : spec.channels) {

                double newBandGene = 0.0;
                if (channel.bandwidth == 20) newBandGene = 0.125;
                else if (channel.bandwidth == 40) newBandGene = 0.375;
                else if (channel.bandwidth == 80) newBandGene = 0.625;
                else newBandGene = 0.875;

                channels_gene.push_back(newBandGene);

                vector<pair<double, Connection>> temp_chan;
                for (const auto& conn : channel.connections) {
                    temp_chan.push_back({variables[conn.id * 2], conn});
                }
                sort(temp_chan.begin(), temp_chan.end());

                vector<Connection> sorted_conns;
                for(auto& p : temp_chan) sorted_conns.push_back(p.second);
                aux.push_back(sorted_conns);
            }
        }
    }

    int max_depth = 0;
    for(int j = 0; j < aux.size(); j++){
        max_depth = max(max_depth, int(aux[j].size()));
    }

    for(int i = 0; i < max_depth; i++){
        for(int j = 0; j < aux.size(); j++){

            if(i >= aux[j].size()) continue;

            Connection conn = aux[j][i];

            int id = conn.id;
            int bandIndex = id * 2 + 1;

            newVariables[bandIndex] = channels_gene[j];

            currentPriority += priorityStep;
        }
    }

    return newVariables;
}

void runDpRebuild(Solution& sol, vector<vector<double>>& links, vector<double>& variables){
    classicConstruct(sol, links, variables);
    applyDpWithCount(sol);
    variables = rebuildChromossome(sol, (int)variables.size(), variables);
}
