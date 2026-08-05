#ifndef DECODER_NEW_HEURISTIC_H
#define DECODER_NEW_HEURISTIC_H

#include "decoder/base.h"

void insertInBestChannel(Solution& sol, int idConnection, vector<double>& variables);
void runNewHeuristic(Solution& sol, vector<vector<double>>& links, vector<double>& variables);

#endif
