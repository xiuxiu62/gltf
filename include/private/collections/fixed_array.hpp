#pragma once

#include "common/types.hpp"
#include <vcruntime_string.h>

template <typename T> struct FixedArray {
    T *data = nullptr;
    usize size = 0;

    static FixedArray<T> create(usize size) {
        FixedArray<T> a;
        a.data = malloc(size * sizeof(T));
        a.size = size;
        return a;
    }

    void destroy() {
        if (data) free(data);
        data = nullptr;
    }

    T *operator[](usize i) {
        if (i >= size) return nullptr;
        return data[i];
    }

    const T *operator[](usize i) const {
        if (i >= size) return nullptr;
        return data[i];
    }

    void clear() {
        if (data) memset(data, 0, size * sizeof(T));
    }
};
