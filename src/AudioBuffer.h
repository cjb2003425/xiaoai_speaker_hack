#pragma once
#include <iostream>

class AudioBuffer {
public:
    AudioBuffer() : size_(0), capacity_(65536) { // Start with 64K capacity
        buffer_ = new uint8_t[capacity_];
    }

    ~AudioBuffer() {
        delete[] buffer_;
    }

    void append(const uint8_t* data, std::size_t len) {
        if (size_ + len > capacity_) {
            resizeBuffer(size_ + len);
        }
        std::memcpy(buffer_ + size_, data, len);
        size_ += len;
    }

    std::size_t size() const {
        return size_;
    }
    
    uint8_t* get() const {
        return buffer_;
    }

    friend std::ostream& operator<<(std::ostream& os, const AudioBuffer& buffer) {
        os.write(reinterpret_cast<const char*>(buffer.buffer_), buffer.size_);
        return os;
    }

private:
    uint8_t* buffer_;
    std::size_t size_;
    std::size_t capacity_;

    // Resizes the buffer to accommodate new data
    void resizeBuffer(std::size_t new_size) {
        while (capacity_ < new_size) {
            capacity_ *= 2;
        }
        uint8_t* new_buffer = new uint8_t[capacity_];
        std::memcpy(new_buffer, buffer_, size_);
        delete[] buffer_;
        buffer_ = new_buffer;
    }
};