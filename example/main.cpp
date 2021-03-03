#include <list>
#include <utility>
#include "../wstpool.h"

using PrimeResult = std::pair<int, bool>;

PrimeResult isPrime(int n)
{
    for (int i = 2; i < n; i++)
    {
        if (n % i == 0)
        {
            return std::make_pair(i, false);
        }
    }
    return std::make_pair(n, true);
}

int main(int, char**)
{
    wstpool::ThreadPool pool(8);
    std::list<std::future<PrimeResult>> fut;

    // Submit lots of jobs (check if a number is prime)
    for (int i = 2; i < 100000; i++)
    {
        fut.emplace_back(pool.async(isPrime, i));
    }

    // Print out prime numbers
    int x = 0;
    for (auto& f : fut)
    {
        PrimeResult result = f.get();
        if (result.second)
        {
            printf("%d ", result.first);
            if (++x % 16 == 0)
            {
                printf("\n");
            }
        }
    }
}

