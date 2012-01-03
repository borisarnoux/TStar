#ifndef GLOBALCOUNTER_H
#define GLOBALCOUNTER_H

#include <numeric>
#include <algorithm>
#include <misc.h>

struct padded_int {
    int val;
    char _padding[CACHE_LINE_SIZE-sizeof(int)];

    const padded_int &operator=( const int &_val ) {
        val = _val;
        return *this;
    }

    int * iterator() {
        return &val;
    }
};

template <int (*GetTID)()>
class GlobalCounter
{
    struct padded_int *counters;
    int nthreads;

public:
    GlobalCounter( int _nthreads )
        : counters(new padded_int[_nthreads]),
          nthreads(_nthreads)
    {
        CFATAL(_nthreads <= 0, "Invalid nthreads.");
        static_assert(sizeof(padded_int) == CACHE_LINE_SIZE, "Invalid padded_int size.");
        std::fill(counters,counters+nthreads, 0);
    }

    void set(int x ) {
        std::fill(counters,counters+nthreads, 0);
        counters[0] = x;
    }

    void incr() {
        CFATAL( GetTID()< 0 || GetTID()>=nthreads, "Error in thread number initialization (use set instead ?)");
        counters[GetTID()].val ++;

    }

    void decr() {
        CFATAL( GetTID()<0 || GetTID()>=nthreads, "Error in thread number initialization (use set instead ?).");
        counters[GetTID()].val --;
    }

    int get_value() {
        return std::accumulate(counters->iterator(),(counters + nthreads)->iterator(), 0);
    }
};

#endif // GLOBALCOUNTER_H
