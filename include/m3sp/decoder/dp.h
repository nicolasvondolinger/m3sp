#ifndef DECODER_DP_H
#define DECODER_DP_H

#include "m3sp/decoder/mic.h"

void applyDpWithCount(Solution& sol);
void runDp(Solution& sol, vector<vector<double>>& links, vector<double>& variables);

#endif
