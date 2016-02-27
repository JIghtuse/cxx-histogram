#include <iostream>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

struct pixel {
    unsigned char blue;
    unsigned char green;
    unsigned char red;
    unsigned char _unused_;
};

struct bitmap {
public:
    bitmap(size_t size)
        : m_size(size)
        , pixels(nullptr)
    {
        pixels = new pixel[m_size];

        for (size_t i = 0; i < m_size; ++i) {
            pixels[i].red = i % 255;
            pixels[i].green = i % 255;
            pixels[i].blue = i % 255;
        }
    }
    ~bitmap()
    {
        delete[] pixels;
    }
    bitmap(const bitmap&) = delete;
    bitmap& operator=(const bitmap&) = delete;
    size_t size() const { return m_size; }
private:
    size_t m_size;
    struct pixel* pixels;
};

int main(int argc, char* argv[])
{
    po::options_description desc("Available options");
    desc.add_options()
        ("help", "produce help message")
        ("bitmap_size", po::value<size_t>(), "set bitmap size")
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

    bitmap bmap(bitmap_size);
    std::cout << bmap.size() << std::endl;
}
