#include <iostream>
#include "serial_device.h"
#include "poll_watcher.h"

int SerialDevice::open(const char* path, int flags) {
    std::cout << "[SerialDevice] Opened: " << path << std::endl;
    return 0;
}

ssize_t SerialDevice::read(int fd, void* buf, size_t count) {
    if (rx_buffer_.empty()) {
        if ((fd & O_NONBLOCK)) {
            return -EAGAIN;
        } else {
            PollWatcher::instance().add_event(fd, EventType::Readable,
                [this](int fd) { this->wait_queue_.wake_up(); });
            wait_queue_.wait();
            PollWatcher::instance().remove_event(fd, EventType::Readable);
        }
    }

    auto out = static_cast<char*>(buf);
    for (size_t i = 0; i < count && !rx_buffer_.empty(); ++i) {
        out[i] = rx_buffer_.front();
        rx_buffer_.pop();
    }

    return count;
}

ssize_t SerialDevice::write(int fd, const void* buf, size_t count) {
    const char* data = static_cast<const char*>(buf);
    for (size_t i = 0; i < count; ++i) {
        std::lock_guard<std::mutex> lock(mtx_);
        rx_buffer_.push(data[i]);
    }
    wait_queue_.wake_up();
    return count;
}


long SerialDevice::ioctl(int fd, unsigned long request, void* argp) {
    (void)fd;

    switch (request) {
        case SERIAL_SET_BAUDRATE:
            baud_rate_ = *(int*)argp;
            std::cout << "[SerialDevice] Baud rate set to: " << baud_rate_ << std::endl;
            break;
        case SERIAL_GET_BAUDRATE:
            *(int*)argp = baud_rate_;
            std::cout << "[SerialDevice] Current baud rate: " << baud_rate_ << std::endl;
            break;
        case SERIAL_FLUSH:
            {
                std::lock_guard<std::mutex> lock(mtx_);
                while (!rx_buffer_.empty()) rx_buffer_.pop();
            }
            std::cout << "[SerialDevice] RX buffer flushed." << std::endl;
            break;
        default:
            std::cerr << "[SerialDevice] Unknown ioctl command" << std::endl;
            return -1;
            break;
    }

    return 0;
}

void SerialDevice::push_data(const std::string& data) {
    for (char c : data) {
        std::lock_guard<std::mutex> lock(mtx_);
        rx_buffer_.push(c);
    }
    //PollWatcher::instance().trigger_event(0, EventType::Readable);
    wait_queue_.wake_up();
}

