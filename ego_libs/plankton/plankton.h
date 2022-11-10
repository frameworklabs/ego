// plankton
//
// Copyright (c) 2022, Framework Labs.

#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>

#include <map>
#include <vector>

/// A simple pub-sub mechanism which broadcasts UDP packets from publishers to subscribers.
class Plankton {
public:
    void begin() {
        udp_.begin(planktonPort);
    }

    bool isReady() const {
        return WiFi.getMode() != WIFI_MODE_STA || WiFi.isConnected();
    }

    bool publish(uint32_t topic, const uint8_t* data, size_t size) {
        if (!isReady()) {
            return false;
        }
        if (udp_.beginPacket(IPAddress(0xFFFFFFFF), planktonPort) != 1) {
            return false;
        }
        udp_.write((const uint8_t*)&topic, sizeof(topic));
        udp_.write(data, size);
        return udp_.endPacket() == 1;
    }

    struct SubscriptionConfig {};

    bool subscribe(uint32_t topic, SubscriptionConfig config) {
        if (entries_.count(topic) == 1) {
            return false;
        }
        auto entry = TopicEntry{};
        entry.config = config;
        entries_[topic] = entry;
        return true;
    }

    bool poll() {
        if (!isReady()) {
            return false;
        }

        auto hasNewPacket = false;
        while (true) {
            const auto count = udp_.parsePacket();
            if (count <= 4) {
                return hasNewPacket;
            }

            const auto topic = uint32_t{};
            udp_.read((uint8_t*)&topic, sizeof(topic));

            const auto it = entries_.find(topic);
            if (it == entries_.end()) {
                udp_.flush();
                continue;
            }

            auto& entry = it->second;
            entry.data.resize(count - 4);
            udp_.read((unsigned char*)entry.data.data(), entry.data.size());
            
            hasNewPacket = true;
        }
    }

    bool read(uint32_t topic, uint8_t* data, size_t size) {
        if (!isReady()) {
            return false;
        }
        const auto it = entries_.find(topic);
        if (it == entries_.end()) {
            return false;
        }

        const auto& buf = it->second.data;
        memcpy(data, buf.data(), min(buf.size(), size));
        return true;
    }

private:
    struct TopicEntry {
        SubscriptionConfig config;
        std::vector<uint8_t> data;
    };

private:
    static constexpr uint16_t planktonPort = 4839;
    WiFiUDP udp_;
    std::map<uint32_t, TopicEntry> entries_;
};
