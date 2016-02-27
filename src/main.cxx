#include <algorithm>
#include <array>
#include <cassert>
#include <unordered_map>
#include <iostream>
#include <mutex>
#include <thread>
#include <boost/program_options.hpp>
#include "bitmap.h"
#include "hpctimer.h"

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
constexpr auto kBuckets{16};
constexpr auto kBorder{kMaxPixelValue / kBuckets};

/* luminance values */
constexpr double rY = 0.2126;
constexpr double gY = 0.7152;
constexpr double bY = 0.0722;

constexpr auto luminance(const pixel& p)
{
    return rY * p.red + gY * p.green + bY * p.blue;
}

std::array<int, kBuckets> histogram;

void print_histogram()
{
    std::cout << "Histogram: ";
    for (auto i : histogram)
        std::cout << i << ' ';
    std::cout << std::endl;
}

enum class Algorithm {
    Sequential,
    TransacitonalMemory,
    Atomic,
    Mutex,
};

void histogram_sequential(const bitmap& b, size_t)
{
    for (size_t i = 0; i < b.size(); ++i) {
        auto bucket = static_cast<int>(luminance(b.pixels()[i]) / kBorder);
        ++histogram[bucket];
    }
}

void histogram_transactional(const bitmap& b, size_t)
{
    for (size_t i = 0; i < b.size(); ++i) {
        auto bucket = static_cast<int>(luminance(b.pixels()[i]) / kBorder);
        __transaction_atomic {
            ++histogram[bucket];
        }
    }
}

void histogram_atomic(const bitmap& b, size_t)
{
    for (size_t i = 0; i < b.size(); ++i) {
        auto bucket = static_cast<int>(luminance(b.pixels()[i]) / kBorder);
        __sync_fetch_and_add(&histogram[bucket], 1);
    }
}

void histogram_mutex(const bitmap& b, size_t)
{
    std::mutex m;
    for (size_t i = 0; i < b.size(); ++i) {
        auto bucket = static_cast<int>(luminance(b.pixels()[i]) / kBorder);
        std::lock_guard<std::mutex> lock{m};
        ++histogram[bucket];
    }
}

std::unordered_map<Algorithm, void(*)(const bitmap&, size_t), EnumClassHash> function_selector{
    { Algorithm::Sequential, histogram_sequential },
    { Algorithm::Atomic, histogram_atomic },
    { Algorithm::Mutex, histogram_mutex },
    { Algorithm::TransacitonalMemory, histogram_transactional },
};

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

void start_experiment(const bitmap& bmap, size_t nthreads, Algorithm algorithm)
{
    print_info(bmap, nthreads, algorithm);

    auto t = hpctimer_wtime();
    function_selector[algorithm](bmap, nthreads);
    t = hpctimer_wtime() - t;

    std::cout << "Running time: " << t << std::endl << std::endl;
}

int main(int argc, char* argv[])
{
    po::options_description desc("Available options");
    desc.add_options()
        ("help", "produce help message")
        ("bitmap_size", po::value<size_t>(), "set bitmap size")
        ("nthreads", po::value<size_t>(), "set number of threads")
    ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    if (!vm.count("bitmap_size")) {
        std::cout << "Bitmap size was not set.\n";
        return 1;
    }
    auto bitmap_size = vm["bitmap_size"].as<size_t>();

    size_t nthreads = std::thread::hardware_concurrency();
    if (vm.count("nthreads")) {
        nthreads = vm["nthreads"].as<size_t>();
    }

    bitmap bmap(bitmap_size);

    start_experiment(bmap, nthreads, Algorithm::Sequential);
    auto etanol = histogram;
    print_histogram();
    std::fill(std::begin(histogram), std::end(histogram), 0);

    start_experiment(bmap, nthreads, Algorithm::TransacitonalMemory);
    assert(etanol == histogram);
    print_histogram();
    std::fill(std::begin(histogram), std::end(histogram), 0);

    start_experiment(bmap, nthreads, Algorithm::Mutex);
    assert(etanol == histogram);
    print_histogram();
    std::fill(std::begin(histogram), std::end(histogram), 0);

    start_experiment(bmap, nthreads, Algorithm::Atomic);
    assert(etanol == histogram);
    print_histogram();
    std::fill(std::begin(histogram), std::end(histogram), 0);
}
