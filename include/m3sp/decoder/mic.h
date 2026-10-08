#ifndef DECODER_MIC_H
#define DECODER_MIC_H

#include "m3sp/decoder/base.h"

void micConstruct(Solution& sol, vector<vector<double>>& links, vector<double>& variables);
void runMic(Solution& sol, vector<vector<double>>& links, vector<double>& variables);

// MIC seguido da mesma busca local do GCSID+ (gcsid_plus_ls.cpp).
void runMicLs(Solution& sol, vector<vector<double>>& links, vector<double>& variables);

#endif
