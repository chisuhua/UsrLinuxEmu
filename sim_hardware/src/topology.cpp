// sim_hardware/src/topology.cpp — T2.2 GREEN (Wave 2)
// Topology JSON loader with strict schema validation per design.md §9.3 + §9.4
// Per spec.md Delta 5 + Delta 11 (error codes)
#include "topology.h"

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace usr_linux_emu::sim_hardware::topology {

namespace {

using json = nlohmann::json;

constexpr uint32_t kSchemaVersion = 1;
constexpr size_t kBdfStrLen = 12;
constexpr size_t kMaxDevices = 16;
constexpr size_t kMaxBarsPerDevice = 6;

uint16_t pack_bdf(const std::string& bdf) {
    if (bdf.size() != kBdfStrLen) return 0;
    auto hex_byte = [](const std::string& s, size_t pos, size_t len) -> uint32_t {
        uint32_t v = 0;
        for (size_t i = 0; i < len; ++i) {
            char c = s[pos + i];
            uint32_t d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return 0xFFFFFFFFu;
            v = (v << 4) | d;
        }
        return v;
    };
    if (bdf[4] != ':' || bdf[7] != ':' || bdf[10] != '.') return 0;
    uint32_t bus    = hex_byte(bdf, 0, 4);
    uint32_t device = hex_byte(bdf, 5, 2);
    uint32_t func   = hex_byte(bdf, 8, 2);
    if (bus == 0xFFFFFFFFu || device == 0xFFFFFFFFu || func == 0xFFFFFFFFu) return 0;
    if (bus > 0x1F || device > 0x1F || func > 0x07) return 0;
    return static_cast<uint16_t>((bus << 8) | (device << 3) | func);
}

pcie::BypassMode parse_bypass_mode(const std::string& s) {
    if (s == "Full")    return pcie::BypassMode::kFull;
    if (s == "Bypass")  return pcie::BypassMode::kBypass;
    if (s == "Partial") return pcie::BypassMode::kPartial;
    return pcie::BypassMode::kFull;
}

bool valid_bypass_mode_string(const std::string& s) {
    return s == "Full" || s == "Bypass" || s == "Partial";
}

pcie::DrainPolicy parse_drain_policy(const std::string& s) {
    if (s == "GracefulDrain")  return pcie::DrainPolicy::kGracefulDrain;
    if (s == "ImmediateAbort")  return pcie::DrainPolicy::kImmediateAbort;
    return pcie::DrainPolicy::kGracefulDrain;
}

bool valid_drain_policy_string(const std::string& s) {
    return s == "GracefulDrain" || s == "ImmediateAbort";
}

bool parse_hex_u16(const std::string& s, uint16_t* out) {
    if (s.empty()) return false;
    const char* p = s.c_str();
    char* end = nullptr;
    unsigned long v;
    if (s.size() > 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        v = std::strtoul(p + 2, &end, 16);
    } else {
        v = std::strtoul(p, &end, 0);
    }
    if (!end || *end != '\0') return false;
    if (v > 0xFFFFul) return false;
    *out = static_cast<uint16_t>(v);
    return true;
}

bool parse_hex_u32(const std::string& s, uint32_t* out) {
    if (s.empty()) return false;
    const char* p = s.c_str();
    char* end = nullptr;
    unsigned long long v;
    if (s.size() > 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        v = std::strtoull(p + 2, &end, 16);
    } else {
        v = std::strtoull(p, &end, 0);
    }
    if (!end || *end != '\0') return false;
    *out = static_cast<uint32_t>(v);
    return true;
}

}  // namespace

int topology_load_json(const std::string& path, Topology* out) {
    if (!out) return -EINVAL;
    if (path.empty()) return -EINVAL;

    std::ifstream f(path);
    if (!f.is_open()) return -ENOENT;

    json j;
    try {
        f >> j;
    } catch (const json::parse_error&) {
        return -EINVAL;
    }
    if (!j.is_object()) return -EINVAL;

    // --- strict schema: only known top-level keys ---
    static const std::set<std::string> kAllowedTopKeys = {
        "schema_version", "platform", "pcie", "devices"};
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (kAllowedTopKeys.find(it.key()) == kAllowedTopKeys.end()) {
            return -EINVAL;
        }
    }

    // --- schema_version ---
    if (!j.contains("schema_version") || !j["schema_version"].is_number_integer()) {
        return -EINVAL;
    }
    if (j["schema_version"].get<uint32_t>() != kSchemaVersion) {
        return -EINVAL;
    }

    // --- platform ---
    if (!j.contains("platform") || !j["platform"].is_string()) return -EINVAL;
    std::string platform = j["platform"].get<std::string>();
    if (platform.empty()) return -EINVAL;

    // --- pcie ---
    if (!j.contains("pcie") || !j["pcie"].is_object()) return -EINVAL;
    const auto& pcie = j["pcie"];
    static const std::set<std::string> kAllowedPcieKeys = {
        "root_complex", "link_layer", "phy", "bypass_mux"};
    for (auto it = pcie.begin(); it != pcie.end(); ++it) {
        if (kAllowedPcieKeys.find(it.key()) == kAllowedPcieKeys.end()) {
            return -EINVAL;
        }
    }

    // --- pcie.root_complex ---
    if (!pcie.contains("root_complex") || !pcie["root_complex"].is_object()) {
        return -EINVAL;
    }
    const auto& rc = pcie["root_complex"];
    if (!rc.contains("enabled") || !rc["enabled"].is_boolean()) return -EINVAL;

    // --- pcie.bypass_mux ---
    if (!pcie.contains("bypass_mux") || !pcie["bypass_mux"].is_object()) {
        return -EINVAL;
    }
    const auto& bm = pcie["bypass_mux"];
    if (!bm.contains("default_mode") || !bm["default_mode"].is_string()) return -EINVAL;
    if (!bm.contains("default_drain_policy") ||
        !bm["default_drain_policy"].is_string()) {
        return -EINVAL;
    }
    if (!valid_bypass_mode_string(bm["default_mode"].get<std::string>())) {
        return -EINVAL;
    }
    if (!valid_drain_policy_string(bm["default_drain_policy"].get<std::string>())) {
        return -EINVAL;
    }

    // --- devices ---
    if (!j.contains("devices") || !j["devices"].is_array()) return -EINVAL;
    const auto& devices = j["devices"];
    if (devices.size() > kMaxDevices) return -EINVAL;

    Topology result{};
    result.schema_version = kSchemaVersion;
    result.platform_name  = platform;
    result.enable_root_complex = rc["enabled"].get<bool>();
    if (pcie.contains("link_layer") && pcie["link_layer"].is_object() &&
        pcie["link_layer"].contains("enabled") &&
        pcie["link_layer"]["enabled"].is_boolean()) {
        result.enable_link_layer = pcie["link_layer"]["enabled"].get<bool>();
    }
    if (pcie.contains("phy") && pcie["phy"].is_object() &&
        pcie["phy"].contains("enabled") &&
        pcie["phy"]["enabled"].is_boolean()) {
        result.enable_phy = pcie["phy"]["enabled"].get<bool>();
    }
    result.default_bypass_mode   = parse_bypass_mode(
        bm["default_mode"].get<std::string>());
    result.default_drain_policy  = parse_drain_policy(
        bm["default_drain_policy"].get<std::string>());

    static const std::set<std::string> kAllowedDeviceKeys = {
        "bdf", "vendor_id", "device_id", "class_code", "endpoint_kind", "bars"};
    static const std::set<std::string> kAllowedBarKeys = {
        "index", "size_bytes", "prefetchable", "is_mmio", "is_64bit"};

    std::set<std::string> seen_bdfs;

    for (size_t di = 0; di < devices.size(); ++di) {
        const auto& dev = devices[di];
        if (!dev.is_object()) return -EINVAL;
        for (auto it = dev.begin(); it != dev.end(); ++it) {
            if (kAllowedDeviceKeys.find(it.key()) == kAllowedDeviceKeys.end()) {
                return -EINVAL;
            }
        }
        if (!dev.contains("bdf") || !dev["bdf"].is_string()) return -EINVAL;
        std::string bdf = dev["bdf"].get<std::string>();
        if (bdf.size() != kBdfStrLen) return -EINVAL;
        if (seen_bdfs.find(bdf) != seen_bdfs.end()) return -EINVAL;
        uint16_t packed = pack_bdf(bdf);
        if (packed == 0 && bdf != "0000:00:00.0") return -EINVAL;
        seen_bdfs.insert(bdf);

        if (!dev.contains("vendor_id") || !dev["vendor_id"].is_string()) return -EINVAL;
        if (!dev.contains("device_id") || !dev["device_id"].is_string()) return -EINVAL;
        uint16_t vendor_id, device_id;
        if (!parse_hex_u16(dev["vendor_id"].get<std::string>(), &vendor_id)) return -EINVAL;
        if (!parse_hex_u16(dev["device_id"].get<std::string>(), &device_id)) return -EINVAL;

        if (!dev.contains("class_code") || !dev["class_code"].is_string()) return -EINVAL;
        uint32_t class_code;
        if (!parse_hex_u32(dev["class_code"].get<std::string>(), &class_code)) return -EINVAL;

        if (!dev.contains("endpoint_kind") || !dev["endpoint_kind"].is_string()) {
            return -EINVAL;
        }
        std::string ek = dev["endpoint_kind"].get<std::string>();
        if (ek.empty()) return -EINVAL;

        if (!dev.contains("bars") || !dev["bars"].is_array()) return -EINVAL;
        const auto& bars = dev["bars"];
        if (bars.size() > kMaxBarsPerDevice) return -EINVAL;

        pcie::TopologyDevice td{};
        td.bdf         = packed;
        td.vendor_id   = vendor_id;
        td.device_id   = device_id;
        td.class_code  = class_code;
        std::strncpy(td.endpoint_kind, ek.c_str(), sizeof(td.endpoint_kind) - 1);
        td.endpoint_kind[sizeof(td.endpoint_kind) - 1] = '\0';

        std::set<uint8_t> seen_bar_idx;
        for (size_t bi = 0; bi < bars.size(); ++bi) {
            const auto& bar = bars[bi];
            if (!bar.is_object()) return -EINVAL;
            for (auto it = bar.begin(); it != bar.end(); ++it) {
                if (kAllowedBarKeys.find(it.key()) == kAllowedBarKeys.end()) {
                    return -EINVAL;
                }
            }
            if (!bar.contains("index") || !bar["index"].is_number_integer()) return -EINVAL;
            int idx = bar["index"].get<int>();
            if (idx < 0 || idx > 5) return -EINVAL;
            uint8_t bidx = static_cast<uint8_t>(idx);
            if (seen_bar_idx.find(bidx) != seen_bar_idx.end()) return -EINVAL;
            seen_bar_idx.insert(bidx);

            if (!bar.contains("size_bytes") || !bar["size_bytes"].is_number_unsigned()) {
                return -EINVAL;
            }
            uint64_t size_bytes = bar["size_bytes"].get<uint64_t>();
            if (size_bytes == 0) return -EINVAL;

            bool prefetchable = false, is_mmio = true, is_64bit = false;
            if (bar.contains("prefetchable")) {
                if (!bar["prefetchable"].is_boolean()) return -EINVAL;
                prefetchable = bar["prefetchable"].get<bool>();
            }
            if (bar.contains("is_mmio")) {
                if (!bar["is_mmio"].is_boolean()) return -EINVAL;
                is_mmio = bar["is_mmio"].get<bool>();
            }
            if (bar.contains("is_64bit")) {
                if (!bar["is_64bit"].is_boolean()) return -EINVAL;
                is_64bit = bar["is_64bit"].get<bool>();
            }
            if (is_64bit) {  // BUG FIX: only reserve next slot when actually 64-bit
                if (idx == 5) return -EINVAL;
                if (seen_bar_idx.find(static_cast<uint8_t>(idx + 1)) != seen_bar_idx.end()) {
                    return -EINVAL;
                }
                seen_bar_idx.insert(static_cast<uint8_t>(idx + 1));
            }

            pcie::TopologyBar tb{};
            tb.index        = bidx;
            tb.size_bytes   = size_bytes;
            tb.prefetchable = prefetchable;
            tb.is_mmio      = is_mmio;
            tb.is_64bit     = is_64bit;
            td.bars.push_back(tb);
        }
        result.devices.push_back(td);
    }

    *out = std::move(result);
    return 0;
}

int topology_write_default_json(const std::string& path, const Topology* src) {
    if (path.empty() || !src) return -EINVAL;
    std::ofstream f(path);
    if (!f.is_open()) return -ENOENT;
    json j;
    j["schema_version"] = 1;
    j["platform"]      = src->platform_name;
    {
        json pcie;
        json rc;
        rc["type"]    = "PcieRootComplexMock";
        rc["enabled"] = src->enable_root_complex;
        pcie["root_complex"] = rc;
        json ll;
        ll["type"]    = "PcieLinkLayer";
        ll["enabled"] = src->enable_link_layer;
        pcie["link_layer"] = ll;
        json phy_node;
        phy_node["type"]    = "PciePhyDigitalCtrl";
        phy_node["enabled"] = src->enable_phy;
        pcie["phy"] = phy_node;
        json bm;
        switch (src->default_bypass_mode) {
            case pcie::BypassMode::kFull:    bm["default_mode"] = "Full"; break;
            case pcie::BypassMode::kBypass:  bm["default_mode"] = "Bypass"; break;
            case pcie::BypassMode::kPartial: bm["default_mode"] = "Partial"; break;
        }
        switch (src->default_drain_policy) {
            case pcie::DrainPolicy::kGracefulDrain: bm["default_drain_policy"] = "GracefulDrain"; break;
            case pcie::DrainPolicy::kImmediateAbort: bm["default_drain_policy"] = "ImmediateAbort"; break;
        }
        pcie["bypass_mux"] = bm;
        j["pcie"] = pcie;
    }
    json devs = json::array();
    auto bdf_to_str = [](uint16_t packed) -> std::string {
        char buf[16];
        uint8_t bus    = (packed >> 8) & 0x1F;
        uint8_t device = (packed >> 3) & 0x1F;
        uint8_t func   = packed & 0x07;
        // Parser (pack_bdf) reads: bus from chars 0-3 (4 hex), device from chars 5-6
        // (2 hex), func from chars 8-9 (2 hex). Writer must mirror that layout.
        std::snprintf(buf, sizeof(buf), "%04x:%02x:%02x.0", bus, device, func);
        return std::string(buf);
    };
    auto hex_u16 = [](uint16_t v) -> std::string {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "0x%04X", v);
        return std::string(buf);
    };
    auto hex_u32 = [](uint32_t v) -> std::string {
        char buf[12];
        std::snprintf(buf, sizeof(buf), "0x%08X", v);
        return std::string(buf);
    };
    for (const auto& dev : src->devices) {
        json d;
        d["bdf"]          = bdf_to_str(dev.bdf);
        d["vendor_id"]    = hex_u16(dev.vendor_id);
        d["device_id"]    = hex_u16(dev.device_id);
        d["class_code"]   = hex_u32(dev.class_code);
        d["endpoint_kind"] = std::string(dev.endpoint_kind);
        json bars = json::array();
        for (const auto& bar : dev.bars) {
            json b;
            b["index"]        = static_cast<int>(bar.index);
            b["size_bytes"]   = bar.size_bytes;
            b["prefetchable"] = bar.prefetchable;
            b["is_mmio"]      = bar.is_mmio;
            b["is_64bit"]     = bar.is_64bit;
            bars.push_back(b);
        }
        d["bars"] = bars;
        devs.push_back(d);
    }
    j["devices"] = devs;
    f << j.dump(2);
    return 0;
}

}  // namespace usr_linux_emu::sim_hardware::topology