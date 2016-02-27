# cxx-histogram
Measuring performance of various synchronization primitives

This program runs the same calculation sequentially and concurrently using various synchronization primitives:

* Locks (std::mutex)
* Atomic variables (through GCC built-in `__sync_fetch_and_add`)
* Transactional Memory (through GCC TM library `__transaction_atomic` on Intel CPU with Intel TSX)

Some results can be found in [scripts/](./scripts/) directory:

![4 threads](./scripts/histogram_4_threads.png)
![8 threads](./scripts/histogram_8_threads.png)
