#include <bits/stdc++.h>

#define _ ios_base::sync_with_stdio(0); cin.tie(0);
#define endl '\n'
#define ff first
#define ss second
#define pb push_back

typedef long long ll;

const int INF = 0x3f3f3f3f;
const ll LINF = 0x3f3f3f3f3f3f3f3fll;

using namespace std;

const ll MAX_CONN = 2048;
const int X_c = 0;
const int Y_c = 1;
const double EPS = 1e-7;

int nConnections, nSpectrums;
vector<vector<double>> dataRates, SINR, beta;
vector<vector<double>> distanceMatrix(MAX_CONN, vector<double>(MAX_CONN)), interferenceMatrix(MAX_CONN, vector<double>(MAX_CONN));
vector<vector<double>> senders(MAX_CONN, vector<double>(2)), receivers(MAX_CONN, vector<double>(2));
vector<vector<double>> affectance(MAX_CONN, vector<double>(MAX_CONN));
double powerSender, alfa, noise, ttm;

static vector<Spectrum> init_conf;
static vector<int> spectrum_size;

inline double distance(double X_si, double Y_si, double X_ri, double Y_ri) {
    return hypot((X_si - X_ri), (Y_si - Y_ri));
}

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

double convertDBMToMW(double value) {
    // dBm dividido por 10
    // Converte de DBm para mW
    return pow(10.0, value / 10.0);
}

void convertTableToMW(const vector<vector<double>> &_SINR, vector<vector<double>> &_SINR_Mw) {
    double result, b;
    for (int i = 0; i < _SINR_Mw.size(); i++) {
        for (int j = 0; j < _SINR_Mw[i].size(); j++) {
            if (_SINR[i][j] != 0) _SINR_Mw[i][j] = convertDBMToMW(_SINR[i][j]);
            else _SINR_Mw[i][j] = 0;
        }
    }
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

    dataRates = SINR = vector<vector<double>>(12, vector<double>(4, 0));
    for (int i = 0; i < 12; i++) {
        for (int j = 0; j < 4; j++) cin >> dataRates[i][j];
    }

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