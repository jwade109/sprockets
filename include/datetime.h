#ifndef SPROCKETS_DATETIME_H
#define SPROCKETS_DATETIME_H

#include <sys/time.h>
#include <stdint.h>

typedef struct timeval datetime;

typedef struct
{
    datetime first;
    datetime next;
    datetime delta;
    uint32_t hit_count;
}
rate_limit;

void init_rate_limit(rate_limit *rate, uint32_t frequency);

int poll_rate(rate_limit *rate, const datetime *now);

// computes t2 - t1
datetime get_timedelta(const datetime *t1, const datetime *t2);

int32_t to_usecs(const datetime *t);

datetime current_time();

char* strdt(const datetime *t);

void print_rate_stats(const rate_limit *rate);

#endif // SPROCKETS_DATETIME_H
