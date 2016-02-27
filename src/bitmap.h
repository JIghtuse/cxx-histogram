#ifndef BITMAP_H
#define BITMAP_H

#include <cstddef>
#include "pixel.h"

struct bitmap {
public:
    bitmap(size_t size);
    ~bitmap();
    bitmap(const bitmap&) = delete;
    bitmap& operator=(const bitmap&) = delete;
    size_t size() const;
    // FIXME: incapsulation is broken
    pixel* pixels() const;
private:
    size_t m_size;
    pixel* m_pixels;
};

#endif // BITMAP_H
