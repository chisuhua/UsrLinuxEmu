#include <catch_amalgamated.hpp>
#include <cstring>

#include "scheduler/translator/gpfifo_translator.h"
#include "gpu_types.h"

static uint32_t pack_pm4_header(uint32_t subchan, uint32_t method_addr,
                                 uint32_t data_count, bool inc) {
  uint32_t h = (subchan << 16)
             | (method_addr << 1)
             | ((data_count & 0xF) << 20);
  if (inc) h |= 1u;
  return h;
}

static const char*  g_cb_kernel  = nullptr;
static uint32_t     g_cb_grid_x  = 0;
static uint32_t     g_cb_grid_y  = 0;
static uint32_t     g_cb_grid_z  = 0;
static int          g_cb_count   = 0;

static void reset_cb() {
  g_cb_kernel = nullptr;
  g_cb_grid_x = g_cb_grid_y = g_cb_grid_z = 0;
  g_cb_count = 0;
}

TEST_CASE("pm4 basic method write subchannel 0", "[pm4]") {
  usr_linux_emu::GpfifoToLaunchParamsTranslator translator;

  translator.setLaunchCallback([](const char* kn,
                                  uint32_t gx, uint32_t gy, uint32_t gz,
                                  uint32_t, uint32_t, uint32_t, uint32_t) {
    g_cb_kernel = kn;
    g_cb_grid_x = gx; g_cb_grid_y = gy; g_cb_grid_z = gz;
    ++g_cb_count;
  });

  reset_cb();
  gpu_gpfifo_entry entry{};
  entry.valid  = 1;
  entry.format = FORMAT_PM4;
  entry.payload[0] = pack_pm4_header(0, 0x1000, 1, false);
  entry.payload[1] = 0xDEADBEEFu;

  bool ok = translator.translateForTest(entry);
  REQUIRE(ok == true);
  REQUIRE(g_cb_count == 1);
  REQUIRE(std::strcmp(g_cb_kernel, "pm4_method") == 0);
  REQUIRE(g_cb_grid_x == 0);
  REQUIRE(g_cb_grid_y == 0x1000);
  REQUIRE(g_cb_grid_z == 0xDEADBEEFu);
}

TEST_CASE("pm4 basic method write subchannel 1", "[pm4]") {
  usr_linux_emu::GpfifoToLaunchParamsTranslator translator;

  translator.setLaunchCallback([](const char* kn,
                                  uint32_t gx, uint32_t gy, uint32_t gz,
                                  uint32_t, uint32_t, uint32_t, uint32_t) {
    g_cb_kernel = kn;
    g_cb_grid_x = gx; g_cb_grid_y = gy; ++g_cb_count;
  });

  reset_cb();
  gpu_gpfifo_entry entry{};
  entry.valid  = 1;
  entry.format = FORMAT_PM4;
  entry.payload[0] = pack_pm4_header(1, 0x2000, 1, false);
  entry.payload[1] = 0xCAFEBABEu;

  bool ok = translator.translateForTest(entry);
  REQUIRE(ok == true);
  REQUIRE(g_cb_count == 1);
  REQUIRE(g_cb_grid_x == 1);
  REQUIRE(g_cb_grid_y == 0x2000);
}

TEST_CASE("pm4 basic method write subchannel 2", "[pm4]") {
  usr_linux_emu::GpfifoToLaunchParamsTranslator translator;

  translator.setLaunchCallback([](const char* kn,
                                  uint32_t gx, uint32_t gy, uint32_t,
                                  uint32_t, uint32_t, uint32_t, uint32_t) {
    g_cb_grid_x = gx; g_cb_grid_y = gy; ++g_cb_count;
  });

  reset_cb();
  gpu_gpfifo_entry entry{};
  entry.valid  = 1;
  entry.format = FORMAT_PM4;
  entry.payload[0] = pack_pm4_header(2, 0x3000, 1, false);
  entry.payload[1] = 0x12345678u;

  bool ok = translator.translateForTest(entry);
  REQUIRE(ok == true);
  REQUIRE(g_cb_count == 1);
  REQUIRE(g_cb_grid_x == 2);
  REQUIRE(g_cb_grid_y == 0x3000);
}

TEST_CASE("pm4 NI mode does not increment method_addr across packets", "[pm4]") {
  usr_linux_emu::GpfifoToLaunchParamsTranslator translator;
  reset_cb();

  uint32_t captured_y[2] = {0, 0};

  translator.setLaunchCallback([&captured_y](const char*,
                                             uint32_t, uint32_t gy, uint32_t,
                                             uint32_t, uint32_t, uint32_t,
                                             uint32_t) {
    captured_y[g_cb_count] = gy;
    ++g_cb_count;
  });

  gpu_gpfifo_entry e1{};
  e1.valid  = 1;
  e1.format = FORMAT_PM4;
  e1.payload[0] = pack_pm4_header(0, 0x1000, 1, false);
  e1.payload[1] = 0xAAAA0001u;
  REQUIRE(translator.translateForTest(e1) == true);
  REQUIRE(g_cb_count == 1);
  REQUIRE(captured_y[0] == 0x1000);

  gpu_gpfifo_entry e2{};
  e2.valid  = 1;
  e2.format = FORMAT_PM4;
  e2.payload[0] = pack_pm4_header(0, 0x1000, 1, false);
  e2.payload[1] = 0xBBBB0002u;
  REQUIRE(translator.translateForTest(e2) == true);
  REQUIRE(g_cb_count == 2);
  REQUIRE(captured_y[1] == 0x1000);
}

TEST_CASE("pm4 INC mode increments method_addr per data word", "[pm4]") {
  usr_linux_emu::GpfifoToLaunchParamsTranslator translator;
  reset_cb();

  uint32_t captured_y[3] = {0, 0, 0};

  translator.setLaunchCallback([&captured_y](const char*,
                                             uint32_t, uint32_t gy, uint32_t,
                                             uint32_t, uint32_t, uint32_t,
                                             uint32_t) {
    captured_y[g_cb_count] = gy;
    ++g_cb_count;
  });

  gpu_gpfifo_entry entry{};
  entry.valid  = 1;
  entry.format = FORMAT_PM4;
  entry.payload[0] = pack_pm4_header(0, 0x1000, 3, true);
  entry.payload[1] = 0x11110000u;
  entry.payload[2] = 0x22220000u;
  entry.payload[3] = 0x33330000u;

  bool ok = translator.translateForTest(entry);
  REQUIRE(ok == true);
  REQUIRE(g_cb_count == 3);
  REQUIRE(captured_y[0] == 0x1000);
  REQUIRE(captured_y[1] == 0x1001);
  REQUIRE(captured_y[2] == 0x1002);
}

TEST_CASE("pm4 INC mode preserves method_addr across packets", "[pm4]") {
  usr_linux_emu::GpfifoToLaunchParamsTranslator translator;
  reset_cb();

  uint32_t captured_y[3] = {0, 0, 0};

  translator.setLaunchCallback([&captured_y](const char*,
                                             uint32_t, uint32_t gy, uint32_t,
                                             uint32_t, uint32_t, uint32_t,
                                             uint32_t) {
    captured_y[g_cb_count] = gy;
    ++g_cb_count;
  });

  gpu_gpfifo_entry e1{};
  e1.valid  = 1;
  e1.format = FORMAT_PM4;
  e1.payload[0] = pack_pm4_header(0, 0x1000, 1, true);
  e1.payload[1] = 0xAAAA0000u;
  REQUIRE(translator.translateForTest(e1) == true);
  REQUIRE(g_cb_count == 1);
  REQUIRE(captured_y[0] == 0x1000);

  gpu_gpfifo_entry e2{};
  e2.valid  = 1;
  e2.format = FORMAT_PM4;
  e2.payload[0] = pack_pm4_header(0, 0x1000, 1, true);
  e2.payload[1] = 0xBBBB0000u;
  REQUIRE(translator.translateForTest(e2) == true);
  REQUIRE(g_cb_count == 2);
  REQUIRE(captured_y[1] == 0x1001);

  gpu_gpfifo_entry e3{};
  e3.valid  = 1;
  e3.format = FORMAT_PM4;
  e3.payload[0] = pack_pm4_header(0, 0x1000, 1, true);
  e3.payload[1] = 0xCCCC0000u;
  REQUIRE(translator.translateForTest(e3) == true);
  REQUIRE(g_cb_count == 3);
  REQUIRE(captured_y[2] == 0x1002);
}

TEST_CASE("pm4 zero data words (header-only) is valid", "[pm4]") {
  usr_linux_emu::GpfifoToLaunchParamsTranslator translator;
  reset_cb();

  translator.setLaunchCallback([](const char*,
                                  uint32_t, uint32_t, uint32_t,
                                  uint32_t, uint32_t, uint32_t, uint32_t) {
    ++g_cb_count;
  });

  gpu_gpfifo_entry entry{};
  entry.valid  = 1;
  entry.format = FORMAT_PM4;
  entry.payload[0] = pack_pm4_header(3, 0x4000, 0, false);

  bool ok = translator.translateForTest(entry);
  REQUIRE(ok == true);
  REQUIRE(g_cb_count == 0);
}

TEST_CASE("pm4 subchannel isolation", "[pm4]") {
  usr_linux_emu::GpfifoToLaunchParamsTranslator translator;
  reset_cb();

  struct Call { uint32_t sc; uint32_t ma; };
  Call calls[4] = {{0,0},{0,0},{0,0},{0,0}};

  translator.setLaunchCallback([&calls](const char*,
                                             uint32_t sc, uint32_t ma, uint32_t,
                                             uint32_t, uint32_t, uint32_t,
                                             uint32_t) {
    calls[g_cb_count].sc = sc;
    calls[g_cb_count].ma = ma;
    ++g_cb_count;
  });

  gpu_gpfifo_entry e1{};
  e1.valid  = 1;
  e1.format = FORMAT_PM4;
  e1.payload[0] = pack_pm4_header(0, 0x1000, 1, true);
  e1.payload[1] = 0xAAAA0000u;
  REQUIRE(translator.translateForTest(e1) == true);
  REQUIRE(g_cb_count == 1);
  REQUIRE(calls[0].sc == 0);
  REQUIRE(calls[0].ma == 0x1000);

  gpu_gpfifo_entry e2{};
  e2.valid  = 1;
  e2.format = FORMAT_PM4;
  e2.payload[0] = pack_pm4_header(1, 0x1000, 1, true);
  e2.payload[1] = 0xBBBB0000u;
  REQUIRE(translator.translateForTest(e2) == true);
  REQUIRE(g_cb_count == 2);
  REQUIRE(calls[1].sc == 1);
  REQUIRE(calls[1].ma == 0x1000);

  gpu_gpfifo_entry e3{};
  e3.valid  = 1;
  e3.format = FORMAT_PM4;
  e3.payload[0] = pack_pm4_header(0, 0x1000, 1, true);
  e3.payload[1] = 0xCCCC0000u;
  REQUIRE(translator.translateForTest(e3) == true);
  REQUIRE(g_cb_count == 3);
  REQUIRE(calls[2].sc == 0);
  REQUIRE(calls[2].ma == 0x1001);

  gpu_gpfifo_entry e4{};
  e4.valid  = 1;
  e4.format = FORMAT_PM4;
  e4.payload[0] = pack_pm4_header(1, 0x1000, 1, true);
  e4.payload[1] = 0xDDDD0000u;
  REQUIRE(translator.translateForTest(e4) == true);
  REQUIRE(g_cb_count == 4);
  REQUIRE(calls[3].sc == 1);
  REQUIRE(calls[3].ma == 0x1001);
}
