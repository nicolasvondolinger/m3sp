#ifndef COMMON_H
#define COMMON_H

#include <bits/stdc++.h>         

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
extern vector<vector<double>> dataRates, SINR, beta;

extern double distanceMatrix[MAX_CONN][MAX_CONN];
extern double interferenceMatrix[MAX_CONN][MAX_CONN];
extern double senders[MAX_CONN][2];
extern double receivers[MAX_CONN][2];
extern double affectance[MAX_CONN][MAX_CONN];
extern double powerSender, alfa, noise, ttm;


extern vector<Spectrum> init_conf;
extern vector<int> spectrum_size;

inline double distance(double X_si, double Y_si, double X_ri, double Y_ri);
void distanceAndInterference();
double convertDBMToMW(double value);
void convertTableToMW(const vector<vector<double>> &_SINR, vector<vector<double>> &_SINR_Mw);
void initTimeSlot();
void loadData();

#endif 