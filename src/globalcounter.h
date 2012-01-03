#ifndef GLOBALCOUNTER_H
#define GLOBALCOUNTER_H

#include <numeric>
#include <algorithm>
template <int (*GetTID)()>
class GlobalCounter
{
    int *counters;
    int nthreads;

public:
    GlobalCounter( int _nthreads )
        : counters(new int[_nthreads]),
          nthreads(_nthreads)
    {
        std::fill(counters,counters+nthreads, 0);
    }

    void incr() {
        counters[GetTID()] ++;

    }

    void decr() {
        counters[GetTID()] --;
    }

    void get_value() {
        return std::accumulate(counters, counters + nthreads, 0);
    }
};

#endif // GLOBALCOUNTER_H
