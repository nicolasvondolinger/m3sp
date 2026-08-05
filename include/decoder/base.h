#ifndef DECODER_BASE_H
#define DECODER_BASE_H

#include "decoder.h"

Channel insertInChannel(Channel newChannel, int id);
Solution applyFusion(Solution sol);
Solution dp(Solution sol);
bool checkFeasibility(const Solution& sol, int totalLinksExpected);

#endif
