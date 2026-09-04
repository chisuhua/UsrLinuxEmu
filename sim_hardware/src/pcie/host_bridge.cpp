// sim_hardware/src/pcie/host_bridge.cpp — T2.3 + T2.4 GREEN (Wave 2)
// Host Bridge enumeration (M2a) + BAR bypass read/write (M2b)
// Per design.md §5.3 + §15.1 + §15.2 + spec.md Delta 3
#include "pcie/host_bridge.h"

#include "cpptlm/bridge.h"
#include "topology.h"

#include <cstring>

namespace usr_linux_emu::sim_hardware::pcie {

namespace {

// Unpack BDF string "DDDD:DD:DD.D" -> packed uint16_t (bus<<8 | dev<<3 | func).
// Used only when topology.devices[i].bdf has not yet been packed; in this Wave 2
// implementation, topology_load_json already packs BDFs before storage, so this
// helper is only here for future direct-API consumers.
uint16_t bdf_string_to_packed(const std::string& bdf) {
    if (bdf.size() != 12) return 0;
    if (bdf[4] != ':' || bdf[7] != ':' || bdf[10] != '.') return 0;
    auto hex = [](const std::string& s, size_t pos, size_t len) -> uint32_t {
        uint32_t v = 0;
        for (size_t i = 0; i < len; ++i) {
            char c = s[pos + i];
            uint32_t d;
            if      (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return 0xFFFFFFFFu;
            v = (v << 4) | d;
        }
        return v;
    };
    uint32_t bus    = hex(bdf, 0, 4);
    uint32_t device = hex(bdf, 5, 2);
    uint32_t func   = hex(bdf, 8, 2);
    if (bus == 0xFFFFFFFFu || device == 0xFFFFFFFFu || func == 0xFFFFFFFFu) return 0;
    if (bus > 0x1F || device > 0x1F || func > 0x07) return 0;
    return static_cast<uint16_t>((bus << 8) | (device << 3) | func);
}

}  // namespace

int host_bridge_enumerate(DiscoveredDevice* devices, std::size_t max_devices,
                          std::size_t* out_count, const char* topology_path) {
    if (!out_count) return -EINVAL;
    if (!topology_path) return -EINVAL;
    // Parameter rules per design §5.3 + spec REQ-SIMHW-HOSTBRIDGE-001:
    //   max_devices == 0 && devices == nullptr  -> legal query form
    //   max_devices >  0 && devices == nullptr  -> -EINVAL
    if (max_devices == 0 && devices != nullptr) return -EINVAL;
    if (max_devices > 0 && devices == nullptr) return -EINVAL;

    topology::Topology topo{};
    int load_rc = topology_load_json(topology_path, &topo);
    if (load_rc != 0) {
        *out_count = 0;
        return load_rc;  // -ENOENT (missing file) or -EINVAL (schema/parse)
    }

    // Empty topology is valid (no devices). Return 0 with count=0, NOT -ENODEV.
    *out_count = topo.devices.size();
    if (max_devices == 0) {
        return 0;  // query-only path
    }

    std::size_t copy_n = (topo.devices.size() < max_devices)
                             ? topo.devices.size()
                             : max_devices;
    for (std::size_t i = 0; i < copy_n; ++i) {
        const auto& td = topo.devices[i];
        devices[i].bdf         = td.bdf;  // already packed by topology_load_json
        devices[i].vendor_id   = td.vendor_id;
        devices[i].device_id   = td.device_id;
        devices[i].class_code  = td.class_code;
        std::strncpy(devices[i].endpoint_kind, td.endpoint_kind,
                     sizeof(devices[i].endpoint_kind) - 1);
        devices[i].endpoint_kind[sizeof(devices[i].endpoint_kind) - 1] = '\0';
    }
    return 0;
}

int host_bridge_bypass_write(uint8_t bar, uint64_t offset,
                             const void* src, std::size_t len) {
    // Per design §5.3 + §10: delegate to active CpptlmBridge.
    CpptlmBridge* bridge = CpptlmBridge_get();
    if (!bridge) return -EINVAL;
    return bridge->mmio_write(bar, offset, src, len);
}

int host_bridge_bypass_read(uint8_t bar, uint64_t offset,
                            void* dst, std::size_t len) {
    // Per design §5.3 + §10: delegate to active CpptlmBridge.
    CpptlmBridge* bridge = CpptlmBridge_get();
    if (!bridge) return -EINVAL;
    return bridge->mmio_read(bar, offset, dst, len);
}

}  // namespace usr_linux_emu::sim_hardware::pcie