#include <algorithm>
#include <array>
#include <cassert>
#include <future>
#include <unordered_map>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <boost/program_options.hpp>
#include "bitmap.h"
#include "hpctimer.h"
#include "join_threads.h"

namespace po = boost::program_options;

struct EnumClassHash
{
    template <typename T>
    std::size_t operator()(T t) const
    {
        return static_cast<std::size_t>(t);
    }
};

constexpr auto kMaxPixelValue{256};
constexpr auto kBuckets{256};
constexpr auto kBorder{kMaxPixelValue / kBuckets};

/* luminance values */
constexpr double rY = 0.2126;
constexpr double gY = 0.7152;
constexpr double bY = 0.0722;

constexpr auto luminance(const pixel& p)
{
    return rY * p.red + gY * p.green + bY * p.blue;
}

struct Bucket {
	std::mutex m{};
	int value{};
};

std::array<Bucket, kBuckets> histogram;

void print_histogram()
{
    std::cout << "Histogram: ";
    for (auto& bucket : histogram)
        std::cout << bucket.value << ' ';
    std::cout << std::endl;
}

enum class Algorithm {
    Sequential,
    TransacitonalMemory,
    Atomic,
    Mutex,
};

using HistUpdateArray = std::array<int, histogram.size()>;
using ParallelHistUpdater = void(*)(const HistUpdateArray&);

void histogram_sequential(const bitmap& b, const size_t)
{
    for (size_t i = 0; i < b.size(); ++i) {
        auto bucket = static_cast<int>(luminance(b.pixels()[i]) / kBorder);
        ++histogram[bucket].value;
    }
}

size_t get_num_threads(const bitmap& b, const size_t nthreads)
{
    size_t const min_per_thread{200};
    size_t const max_threads{(b.size() + min_per_thread - 1) / min_per_thread};
    return std::min(nthreads ? nthreads : 2, max_threads);
}

HistUpdateArray calculate_updates(const bitmap& b, size_t start, size_t end)
{
    auto updates = std::array<int, histogram.size()>{};
    for (size_t i = start; i < end; ++i) {
        auto bucket = static_cast<int>(luminance(b.pixels()[i]) / kBorder);
        ++updates[bucket];
    }
    return updates;
}

void histogram_mutex(const HistUpdateArray& updates)
{
    for (size_t i = 0; i < histogram.size(); ++i) {
        if (updates[i]) {
            std::lock_guard<std::mutex> lock{histogram[i].m};
            histogram[i].value += updates[i];
        }
    }
}

void histogram_atomic(const HistUpdateArray& updates)
{
    for (size_t i = 0; i < histogram.size(); ++i) {
        if (updates[i]) {
            __sync_fetch_and_add(&histogram[i].value, updates[i]);
        }
    }
}

void histogram_transactional(const HistUpdateArray& updates)
{
    for (size_t i = 0; i < histogram.size(); ++i) {
        if (updates[i]) {
            __transaction_atomic {
                histogram[i].value += updates[i];
            }
        }
    }
}

std::unordered_map<Algorithm, ParallelHistUpdater, EnumClassHash> parallel_function_selector{
    { Algorithm::Atomic, histogram_atomic },
    { Algorithm::Mutex, histogram_mutex },
    { Algorithm::TransacitonalMemory, histogram_transactional },
};

void histogram_parallel(const bitmap& b, const size_t nthreads, ParallelHistUpdater function)
{
    auto num_threads = get_num_threads(b, nthreads);
    size_t const block_size{b.size() / num_threads};

    std::vector<std::future<void> > futures{num_threads - 1};
    std::vector<std::thread> threads{num_threads - 1};
    join_threads joiner(threads);

    size_t block_start = 0;
    for (size_t i = 0; i < (num_threads - 1); ++i) {
        auto block_end = block_start;
        block_end += block_size;

        std::packaged_task<void(void)> task([block_start,block_end,function,&b] {
            function(calculate_updates(b, block_start, block_end));
        });

        futures[i] = task.get_future();
        threads[i] = std::thread(std::move(task));
        block_start = block_end;
    }
    function(calculate_updates(b, block_start, b.size()));
    for (size_t i = 0; i < (num_threads - 1); ++i) {
        futures[i].get();
    }
}

void print_info(const bitmap& bmap, size_t nthreads, Algorithm algorithm)
{
    std::cout << "Experiment parameters:\n"
              << "\tNumber of threads = " << nthreads << "\n"
              << "\tBitmap size = " << bmap.size() << "\n"
    ;

    switch (algorithm) {
    case Algorithm::Sequential:
        std::cout << "\tAlgorithm = Sequential\n";
        break;
    case Algorithm::TransacitonalMemory:
        std::cout << "\tAlgorithm = Transactional Memory\n";
        break;
    case Algorithm::Atomic:
        std::cout << "\tAlgorithm = Atomic\n";
        break;
    case Algorithm::Mutex:
        std::cout << "\tAlgorithm = Mutex\n";
        break;
    }
}

double run_experiment(const bitmap& bmap, size_t nthreads, Algorithm algorithm)
{
    for (auto& bucket: histogram) {
        bucket.value = 0;
    }
    // print_info(bmap, nthreads, algorithm);
    auto t = hpctimer_wtime();
    switch (algorithm) {
    case Algorithm::Mutex:
    case Algorithm::Atomic:
    case Algorithm::TransacitonalMemory:
        histogram_parallel(bmap, nthreads, parallel_function_selector[algorithm]);
        break;
    case Algorithm::Sequential:
        histogram_sequential(bmap, nthreads);
        break;
    }
    t = hpctimer_wtime() - t;

    return t;
}

int main(int argc, char* argv[])
{
    po::options_description desc("Available options");
    desc.add_options()
        ("help", "produce help message")
        ("bitmap-size", po::value<size_t>(), "set bitmap size")
        ("nthreads", po::value<size_t>(), "set number of threads")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    if (!vm.count("bitmap-size")) {
        std::cout << "Bitmap size was not set.\n";
        return 1;
    }
    auto bitmap_size = vm["bitmap-size"].as<size_t>();

    size_t nthreads = std::thread::hardware_concurrency();
    if (vm.count("nthreads")) {
        nthreads = vm["nthreads"].as<size_t>();
    }

    bitmap bmap(bitmap_size);

    std::cout << bitmap_size << ' '
              << run_experiment(bmap, nthreads, Algorithm::Sequential) << ' '
              << run_experiment(bmap, nthreads, Algorithm::TransacitonalMemory) << ' '
              << run_experiment(bmap, nthreads, Algorithm::Mutex) << ' '
              << run_experiment(bmap, nthreads, Algorithm::Atomic) << ' '
              << std::endl;
}
