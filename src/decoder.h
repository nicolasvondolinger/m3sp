#include <bits/stdc++.h>
#include "common.cpp"

using namespace std;

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

    double decode(vector<double>& variables) const;
};