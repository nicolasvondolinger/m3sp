#ifndef COMMON_H
#define COMMON_H

#include <bits/stdc++.h>      
#include <omp.h>          // Necessário para omp_get_thread_num()
#include "MTRand.h"

#define _ ios_base::sync_with_stdio(0); cin.tie(0);
#define endl '\n'
#define ff first
#define ss second
#define pb push_back

using namespace std;

typedef long long ll;

const int INF = 0x3f3f3f3f;
const ll LINF = 0x3f3f3f3f3f3f3f3fll;
const ll MAX_CONN = 2048;
const int X_c = 0;
const int Y_c = 1;
const double EPS = 1e-7;
extern bool applyFinalLS;
extern bool applyEliteLS;

// Coleta da composicao de canais da solucao final (para o grafico de larguras de banda).
// Quando collectChannels==true, o decode() preenche channelBandCount com a contagem de
// canais por largura de banda (ex.: channelBandCount[20], [40], [80], [160]).
extern bool collectChannels;
extern map<int, long long> channelBandCount;

// Gerador serial: usado pelo BRKGA (crossover/mutacao) e por main.cpp, sempre
// fora de regiao paralela.
extern MTRand rng;

// Gerador dos decoders. decode() roda dentro de um `omp parallel for`, e MTRand
// nao e thread-safe: compartilhar um unico gerador global entre as threads e
// uma corrida de dados, e o resultado passa a depender do escalonamento.
//
// Aqui cada thread tem o seu gerador, e ele e re-semeado no inicio de cada
// decode() a partir do PROPRIO cromossomo (mais a semente da instancia). Com
// isso o fluxo aleatorio e uma funcao da entrada, nao da thread: o mesmo
// cromossomo produz o mesmo resultado independentemente de qual thread o
// decodificou, de quantas threads existem e da maquina.
MTRand& decoderRng();

// Semente base da instancia; chamar uma vez por instancia, fora de regiao
// paralela.
void seedDecoderRng(unsigned long baseSeed);

// Re-semeia o gerador da thread para este cromossomo. Chamado por
// Solution::decode() antes de qualquer sorteio.
void seedDecoderRngForChromosome(const vector<double>& chromosome);

struct Connection {
    int id;
    double throughput;
    double interference;
    double SINR;
    double distanceSR;

    Connection(int id, double throughput, double interference, double distanceSR);
    Connection(int id);

    bool operator<(const Connection &other) const;
    bool operator>(const Connection &other) const;
};

struct Channel {
    double throughput;
    double interference;
    double violation;
    int bandwidth;
    vector<Connection> connections; 

    Channel(double throughput, double interference, double violation, int bandwidth,
            const vector<Connection> connections);
    Channel(int bandwidth);
    Channel();
    
    bool operator<(const Channel &other) const;
    bool operator>(const Channel &other) const;
};

struct Spectrum {
    int maxFrequency;
    int usedFrequency;
    double interference;
    vector<Channel> channels; 

    Spectrum(int maxFrequency, int usedFrequency, const vector<Channel> channels);
    Spectrum();
};

struct TimeSlot {
    vector<Spectrum> spectrums; 
    double interference;
    double throughput;

    TimeSlot(const vector<Spectrum> sp);
    TimeSlot();
};


extern int nConnections, nSpectrums;

// Heuristica escolhida na linha de comando (ex.: "mic", "dp", "gcsid",
// "local_search"). Ver KNOWN_TYPES em main.cpp.
extern string type;
extern vector<vector<double>> dataRates, SINR, beta;

extern double distanceMatrix[MAX_CONN][MAX_CONN];
extern double interferenceMatrix[MAX_CONN][MAX_CONN];
extern double senders[MAX_CONN][2];
extern double receivers[MAX_CONN][2];
extern double affectance[MAX_CONN][MAX_CONN];
extern double powerSender, alfa, noise, ttm;

extern long long count_improvements;

extern vector<Spectrum> init_conf;
extern vector<int> spectrum_size;

inline double distance(double X_si, double Y_si, double X_ri, double Y_ri);
void distanceAndInterference();
double convertDBMToMW(double value);
void convertTableToMW(const vector<vector<double>> &_SINR, vector<vector<double>> &_SINR_Mw);
void initTimeSlot();
void loadData();

#endif 