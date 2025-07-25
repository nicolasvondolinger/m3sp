#include "../include/common.h"

double distanceMatrix[MAX_CONN][MAX_CONN];
double interferenceMatrix[MAX_CONN][MAX_CONN];
double senders[MAX_CONN][2];
double receivers[MAX_CONN][2];
double affectance[MAX_CONN][MAX_CONN];

using namespace std;

int nConnections, nSpectrums;
vector<vector<double>> dataRates, SINR, beta;
double powerSender, alfa, noise, ttm;

vector<Spectrum> init_conf;
vector<int> spectrum_size;

Connection::Connection(int id, double throughput, double interference, double distanceSR)
    : id(id), throughput(throughput), interference(interference), distanceSR(distanceSR) {
    SINR = 0.0;
}

Connection::Connection(int id) : id(id) {
    throughput = 0.0;
    interference = 0.0;
    SINR = 0.0;
    distanceSR = distanceMatrix[id][id];
}

bool Connection::operator<(const Connection &other) const { return distanceSR < other.distanceSR; }
bool Connection::operator>(const Connection &other) const { return !operator<(other); }


Channel::Channel(double throughput, double interference, double violation, int bandwidth,
        const vector<Connection> connections)
    : throughput(throughput), interference(interference), violation(violation),
      bandwidth(bandwidth), connections(connections) {}

Channel::Channel(int bandwidth) : Channel(0.0, 0.0, 0.0, bandwidth, vector<Connection>()) {}

Channel::Channel() : Channel(0.0) {}

bool Channel::operator<(const Channel &other) const { return bandwidth < other.bandwidth; }
bool Channel::operator>(const Channel &other) const { return !operator<(other); }


Spectrum::Spectrum(int maxFrequency, int usedFrequency, const ::vector<Channel> channels)
    : maxFrequency(maxFrequency), usedFrequency(usedFrequency), channels(channels) {
    interference = 0.0;
}

Spectrum::Spectrum() {
    maxFrequency = 0;
    usedFrequency = 0;
    interference = 0.0;
    channels = ::vector<Channel>();
}

TimeSlot::TimeSlot(const ::vector<Spectrum> sp) : spectrums(sp) {}
TimeSlot::TimeSlot() {
    interference = 0.0;
    throughput = 0.0;
    spectrums = ::vector<Spectrum>();
}

inline double distance(double X_si, double Y_si, double X_ri, double Y_ri) {
    return hypot((X_si - X_ri), (Y_si - Y_ri));
}

void distanceAndInterference() {
    for (int i = 0; i < nConnections; i++) {
        double X_si = receivers[i][X_c];
        double Y_si = receivers[i][Y_c];

        for (int j = 0; j < nConnections; j++) {
            double X_rj = senders[j][X_c];
            double Y_rj = senders[j][Y_c];

            double dist = distance(X_si, Y_si, X_rj, Y_rj);
            distanceMatrix[i][j] = dist;
        }
    }
}

double convertDBMToMW(double value) {
    return pow(10.0, value / 10.0);
}

void convertTableToMW(const vector<vector<double>> &_SINR, vector<vector<double>> &_SINR_Mw) {
    for (int i = 0; i < _SINR_Mw.size(); i++) { 
        for (int j = 0; j < _SINR_Mw[i].size(); j++) {
            if (_SINR[i][j] != 0) _SINR_Mw[i][j] = convertDBMToMW(_SINR[i][j]);
            else _SINR_Mw[i][j] = 0;
        }
    }
}

void initTimeSlot() {
    for (Spectrum &sp : init_conf) {
        while (sp.maxFrequency - sp.usedFrequency > 0) {
            int bw = 160;
            while (bw > (sp.maxFrequency - sp.usedFrequency) && bw > 20) bw /= 2;
            
            if (bw <= (sp.maxFrequency - sp.usedFrequency)) {
                sp.usedFrequency += bw;
                sp.channels.emplace_back(0.0, 0.0, 0.0, bw, vector<Connection>());
            }
        }
        assert(sp.maxFrequency - sp.usedFrequency >= 0);
    }
}

void loadData(){
    cin >> nConnections >> alfa >> noise >> powerSender >> nSpectrums;
    for (int i = 0; i < nSpectrums; i++) {
        int s; cin >> s;
        spectrum_size.emplace_back(s);
        init_conf.emplace_back(s, 0, vector<Channel>());
    }

    if (noise != 0) noise = convertDBMToMW(noise);
    

    for (int i = 0; i < nConnections; i++) {
        cin >> receivers[i][0] >> receivers[i][1];
    }

    for (int i = 0; i < nConnections; i++) {
        cin >> senders[i][0] >> senders[i][1];
    }

    for (int i = 0; i < nConnections; i++) {
        double bt; cin >> bt;
    }

    dataRates.assign(12, vector<double>(4, 0));
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 4; j++) cin >> dataRates[i][j];
    }

    SINR.assign(12, vector<double>(4, 0));
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 4; j++) cin >> SINR[i][j];
    }

    convertTableToMW(SINR, SINR);
    distanceAndInterference();
    initTimeSlot();

    for (int i = 0; i < nConnections; i++) {
        for (int j = 0; j < nConnections; j++) {
            affectance[i][j] = powerSender / pow(distanceMatrix[i][j], alfa);
        }
    }
}