#ifndef DECODER_GCSID_PLUS_LS_H
#define DECODER_GCSID_PLUS_LS_H

#include "m3sp/decoder/base.h"

// Busca local aplicada a solucao que o GCSID+ acabou de construir (e tambem a
// do MIC, no tipo mic_ls). Nao mexe na construcao: recebe a solucao pronta e so
// devolve uma solucao viavel com throughput maior ou igual. `links` e a lista ja
// ordenada pela primeira chave, a mesma que guiou a construcao; ela fixa a ordem
// em que os links sao revisitados.
void localSearchGcsidPlus(Solution& sol, const vector<vector<double>>& links);

#endif
