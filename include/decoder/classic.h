#ifndef DECODER_CLASSIC_H
#define DECODER_CLASSIC_H

#include "decoder/base.h"

void classicConstruct(Solution& sol, vector<vector<double>>& links, vector<double>& variables);
void applyDpWithCount(Solution& sol);
void runClassic(Solution& sol, vector<vector<double>>& links, vector<double>& variables);

#endif
