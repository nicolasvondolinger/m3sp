#ifndef DECODER_BASE_H
#define DECODER_BASE_H

#include "m3sp/decoder.h"

double linkThroughput(int id, double interference, int band);
Channel insertInChannel(Channel newChannel, int id);
Solution applyFusion(Solution sol);
Solution dp(Solution sol);
bool checkFeasibility(const Solution& sol, int totalLinksExpected);

#endif
