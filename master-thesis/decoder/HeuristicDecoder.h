//
// Created by José Joaquim on 03/03/20.
//

#ifndef BRKGA_FF_BEST_HEURISTICDECODER_H
#define BRKGA_FF_BEST_HEURISTICDECODER_H

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <numeric>
#include <random>
#include <vector>

#include <chrono>
#include <thread>

#include "MTRand.h"

using namespace std;

const int MAX_CONN = 2048;

extern int nConnections, nSpectrums;
extern double senders[MAX_CONN][2], receivers[MAX_CONN][2];
extern pair<int, int> zeroChannel;
extern double powerSender, alfa, noise, ttm;
extern MTRand rng;
extern const int MAX_SPECTRUM, MAX_CHANNELS;

const int X_c = 0;
const int Y_c = 1;
const double EPS = 1e-7;

extern int nConnections, nSpectrums;
extern vector<vector<double>> dataRates, SINR, beta;
extern double distanceMatrix[MAX_CONN][MAX_CONN], interferenceMatrix[MAX_CONN][MAX_CONN];
extern double senders[MAX_CONN][2], receivers[MAX_CONN][2];
extern double affectance[MAX_CONN][MAX_CONN];
extern double powerSender, alfa, noise, ttm;

extern random_device rd;

extern default_random_engine whatever;// = std::default_random_engine{rd()};
extern MTRand rng;

using ii = pair<int, int>;

// struct Connection {
//     int id;
//     double throughput;
//     double interference;
//     double SINR;
//     double distanceSR;
// 
//     Connection(int id, double throughput, double interference, double distanceSR);
// 
//     Connection(int id);
// 
//     bool operator<(const Connection &other) const { return distanceSR < other.distanceSR; }
// 
//     bool operator>(const Connection &other) const { return !operator<(other); }
// };
// 
// struct Channel {
//     double throughput; // TODO: So far, I do not have any use for this variable.
//     double interference;
//     int bandwidth;
//     vector<Connection> connections;
// 
//     bool operator<(const Channel &other) const { return bandwidth < other.bandwidth; }
// 
//     bool operator>(const Channel &other) const { return !operator<(other); }
// 
//     Channel(double throughput, double interference, int bandwidth,
//             const vector<Connection> &connections);
// 
//     Channel(int bandwidth);
// };
// 
// struct Spectrum {
//     int maxFrequency;
//     int usedFrequency;
//     vector<Channel> channels;
// 
//     Spectrum(int maxFrequency, int usedFrequency, const vector<Channel> &channels);
// };
// 
// class Solution {
//   public:
//     vector<Spectrum> spectrums;
//     double totalThroughput;
// 
//     bool throughputFlag; // FIXME: conflict with the constructors
// 
//     Solution(const vector<Spectrum> &spectrums, double totalThroughput, bool throughputFlag);
// 
//     Solution();
// 
//     void printSolution(FILE *file = nullptr);
// 
//     // AVAILABLE ONLY IN THE META-HEURISTICS FILES
//     double decode(vector<double> variables) const;
// 
//     friend bool operator>(const Solution &o1, const Solution &o2);
// 
//     friend bool operator<(const Solution &o1, const Solution &o2);
// };

struct Connection {
    int id;
    double throughput;
    double interference;
    double SINR;
    double distanceSR;

    Connection(int id, double throughput, double interference, double distanceSR)
        : id(id), throughput(throughput), interference(interference), distanceSR(distanceSR) {
        SINR = 0.0;
    }

    Connection(int id) : id(id) {
        throughput = 0.0;
        interference = 0.0;
        SINR = 0.0;
        distanceSR = distanceMatrix[id][id];
    }

    bool operator<(const Connection &other) const { return distanceSR < other.distanceSR; }

    bool operator>(const Connection &other) const { return !operator<(other); }
};

struct Channel {
    double throughput;
    double interference;
    double violation;
    int bandwidth;
    vector<Connection> connections;

    bool operator<(const Channel &other) const { return bandwidth < other.bandwidth; }

    bool operator>(const Channel &other) const { return !operator<(other); }

    Channel(double throughput, double interference, double violation, int bandwidth,
            const vector<Connection> connections)
        : throughput(throughput), interference(interference), violation(violation),
          bandwidth(bandwidth), connections(connections) {}

    Channel(int bandwidth) : Channel(0.0, 0.0, 0.0, bandwidth, vector<Connection>()) {}

    Channel() : Channel(0.0) {}
};

struct Spectrum {
    int maxFrequency;
    int usedFrequency;
    double interference;
    vector<Channel> channels;

    Spectrum(int maxFrequency, int usedFrequency, const vector<Channel> channels)
        : maxFrequency(maxFrequency), usedFrequency(usedFrequency), channels(channels) {
        interference = 0.0;
    }

    Spectrum() {
        maxFrequency = 0;
        usedFrequency = 0;
        interference = 0.0;
        channels = vector<Channel>();
    }
};

struct TimeSlot {
    vector<Spectrum> spectrums;
    double interference;
    double throughput;

    TimeSlot(const vector<Spectrum> sp) : spectrums(sp) {}
    TimeSlot() {
        interference = 0.0;
        throughput = 0.0;
        spectrums = vector<Spectrum>();
    }
};

class Solution {
  public:
    vector<TimeSlot> slots;
    double throughput;
    double violation;

    Solution(const vector<Spectrum> sp, double tot) : throughput(tot) {
        slots.emplace_back(sp);
        violation = 0.0;
    }

    Solution(const vector<TimeSlot> &ts) : slots(ts) {
        throughput = 0.0;
        violation = 0.0;
    }

    Solution() {
        slots = vector<TimeSlot>();
        throughput = 0.0;
        violation = 0.0;
    }

    double decode(vector<double> variables) const;

#ifdef MDVRBSP
    bool operator<(const Solution &o1) const { return violation < o1.violation; }

    bool operator>(const Solution &o1) const { return !operator<(o1); }
#else
    bool operator<(const Solution &o1) const { return throughput < o1.throughput; }

    bool operator>(const Solution &o1) const { return !operator<(o1); }
#endif
};


bool checkOne(const Solution &s);

bool checkTwo(const Solution &s);

int bwIdx(int bw);

void loadData();

void rawInsert(Solution &sol, int conn, ii where);

void rawRemove(Solution &sol, int conn, ii where);

Channel insertInChannel(Channel newChannel, int conn);

Channel deleteFromChannel(const Channel &channel, int conn);

double computeThroughput(Solution &curr);

bool double_equals(double a, double b, double epsilon = 0.000000001);

void computeChannelsThroughput(vector<Channel> &channels);

double computeConnectionThroughput(Connection &conn, int bandWidth);

Solution createSolution();

int computeConnectionMCS(const Connection &conn, int bandwidth);

#endif // BRKGA_FF_BEST_HEURISTICDECODER_H
