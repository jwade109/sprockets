
#include <datetime.h>
#include <stdio.h>

void init_rate_limit(rate_limit *rate, uint32_t frequency)
{
    rate->first.tv_sec = 0;
    rate->first.tv_usec = 0;
    rate->next = rate->first;
    rate->delta.tv_usec = 1000000U / frequency;
    rate->delta.tv_sec = rate->delta.tv_usec / 1000000U;
    rate->delta.tv_usec %= 1000000U;
    rate->hit_count = 0;
}

int datecmp(const datetime *a, const datetime *b)
{
    if (a->tv_sec < b->tv_sec)
    {
        return -1;
    }
    if (a->tv_sec > b->tv_sec)
    {
        return 1;
    }
    if (a->tv_usec < b->tv_usec)
    {
        return -1;
    }
    if (a->tv_usec > b->tv_usec)
    {
        return 1;
    }
    return 0;
}

void normalize_datetime(datetime *t)
{
    t->tv_sec += t->tv_usec / 1000000U;
    t->tv_usec %= 1000000;
}

int poll_rate(rate_limit *rate, const datetime *now)
{
    if (datecmp(now, &rate->next) < 0) // now < rate->next
    {
        // event doesn't fire
        return 0;
    }

    if (!rate->hit_count)
    {
        rate->next = *now;
        rate->first = *now;
    }

    ++rate->hit_count;

    // fire!
    rate->next.tv_sec += rate->delta.tv_sec;
    rate->next.tv_usec += rate->delta.tv_usec;
    normalize_datetime(&rate->next);
    return 1;
}

// computes t2 - t1
datetime get_timedelta(const datetime *t1, const datetime *t2)
{
    datetime dt;
    dt.tv_sec = t2->tv_sec - t1->tv_sec;
    dt.tv_usec = t2->tv_usec - t1->tv_usec;
    return dt;
}

int32_t to_usecs(const datetime *t)
{
    return t->tv_sec * 1000000 + t->tv_usec;
}

datetime current_time()
{
    datetime ret;
    gettimeofday(&ret, 0);
    return ret;
}

char* strdt(const datetime *t)
{
    static char buffer[300];
    sprintf(buffer, "%ld.%06ld", t->tv_sec, t->tv_usec);
    return buffer;
}

void print_rate_stats(const rate_limit *rate)
{
    printf("f=%lu.%06lu n=%lu.%06lu h=%u\n",
        rate->first.tv_sec, rate->first.tv_usec,
        rate->next.tv_sec, rate->next.tv_usec,
        rate->hit_count);
}
