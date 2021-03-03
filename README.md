# wstpool
## Work Stealing Thread Pool, Header Only, C++ Threads

* Consistent with the C++ async/future programming model.
* Drop-in replacement for 'async' for using the thread pool.
* Header only--no dependencies. Just copy the header to your project.
* Work Stealing, multi-queue

```c++
#include "wstpool.h"

wstpool::ThreadPool tp(8);

std::future<...> f = tp.async(...);

f.get();
```
Build instructions for example:
```bash
cd example/
cmake .
make
