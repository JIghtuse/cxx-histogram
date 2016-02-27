#include "bitmap.h"

bitmap::bitmap(size_t size)
    : m_size(size)
    , m_pixels(nullptr)
{
    m_pixels = new pixel[m_size];

    for (size_t i = 0; i < m_size; ++i) {
        m_pixels[i].red = i % 255;
        m_pixels[i].green = i % 255;
        m_pixels[i].blue = i % 255;
    }
}

bitmap::~bitmap()
{
    delete[] m_pixels;
}

size_t bitmap::size() const { return m_size; }

pixel* bitmap::pixels() const { return m_pixels; }
