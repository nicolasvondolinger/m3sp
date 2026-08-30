#ifndef DECODER_GCSID_H
#define DECODER_GCSID_H

#include "m3sp/decoder/base.h"

void insertInBestChannel(Solution& sol, int idConnection, vector<double>& variables);
void runGcsid(Solution& sol, vector<vector<double>>& links, vector<double>& variables);

#endif
