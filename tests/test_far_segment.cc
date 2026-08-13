#include "catch.hpp"

#include <cstdint>
#include <vector>

#include "channel_state.h"
#include "configuration.h"
#include "timing.h"

namespace {

struct TimedCommand {
    dramsim3::CommandType type;
    uint64_t cycle;
};

std::vector<TimedCommand> IssueUntil(dramsim3::ChannelState& channel_state,
                                     const dramsim3::Command& request,
                                     dramsim3::CommandType final_type,
                                     uint64_t& cycle) {
    std::vector<TimedCommand> issued;
    for (int attempts = 0; attempts < 1000; attempts++, cycle++) {
        dramsim3::Command ready =
            channel_state.GetReadyCommand(request, cycle);
        if (!ready.IsValid()) {
            continue;
        }

        issued.push_back({ready.cmd_type, cycle});
        channel_state.UpdateTimingAndStates(ready, cycle);
        if (ready.cmd_type == final_type) {
            cycle++;
            return issued;
        }
    }

    FAIL("Timed out waiting for far-segment command sequence");
    return issued;
}

void RequireTypes(const std::vector<TimedCommand>& issued,
                  const std::vector<dramsim3::CommandType>& expected) {
    REQUIRE(issued.size() == expected.size());
    for (size_t i = 0; i < expected.size(); i++) {
        REQUIRE(issued[i].type == expected[i]);
    }
}

dramsim3::Command ReadCommand(int row) {
    dramsim3::Address addr(0, 0, 0, 0, row, 0);
    return dramsim3::Command(dramsim3::CommandType::READ, addr,
                             static_cast<uint64_t>(row) << 16);
}

}  // namespace

TEST_CASE("Far-segment cycle timing configuration", "[far_segment]") {
    dramsim3::Config config("tests/configs/far_segment.ini", ".");

    REQUIRE(config.tTGON == 4);
    REQUIRE(config.tTGOFF == 4);
    REQUIRE(config.near_segment_latency_scale == 0.5);
    REQUIRE(config.tRP_near == 3);
    REQUIRE(config.tRCD_near == 3);
    REQUIRE(config.near_segment_switch_latency == 6);
}

TEST_CASE("CPU and PIM near rows preserve independent contexts",
          "[far_segment]") {
    dramsim3::Config config("tests/configs/far_segment.ini", ".");
    dramsim3::Timing timing(config);
    dramsim3::ChannelState channel_state(config, timing);
    uint64_t cycle = 0;

    IssueUntil(channel_state, ReadCommand(7), dramsim3::CommandType::READ,
               cycle);
    IssueUntil(channel_state, ReadCommand(7), dramsim3::CommandType::READ,
               cycle);
    REQUIRE(channel_state.OpenRow(0, 0, 0) == 7);
    REQUIRE(channel_state.RowHitCount(0, 0, 0) == 2);

    cycle += static_cast<uint64_t>(config.near_segment_switch_latency);
    channel_state.SetPimMode(true);
    REQUIRE(channel_state.OpenRow(0, 0, 0) == -1);
    IssueUntil(channel_state, ReadCommand(8), dramsim3::CommandType::READ,
               cycle);
    REQUIRE(channel_state.OpenRow(0, 0, 0) == 8);
    REQUIRE(channel_state.RowHitCount(0, 0, 0) == 1);

    cycle += static_cast<uint64_t>(config.near_segment_switch_latency);
    channel_state.SetPimMode(false);
    REQUIRE(channel_state.OpenRow(0, 0, 0) == 7);
    REQUIRE(channel_state.RowHitCount(0, 0, 0) == 2);

    dramsim3::Command cpu_hit =
        channel_state.GetReadyCommand(ReadCommand(7), cycle);
    REQUIRE(cpu_hit.IsValid());
    REQUIRE(cpu_hit.cmd_type == dramsim3::CommandType::READ);
    channel_state.UpdateTimingAndStates(cpu_hit, cycle);
    REQUIRE(channel_state.RowHitCount(0, 0, 0) == 3);

    cycle += static_cast<uint64_t>(config.near_segment_switch_latency);
    channel_state.SetPimMode(true);
    REQUIRE(channel_state.OpenRow(0, 0, 0) == 8);
    REQUIRE(channel_state.RowHitCount(0, 0, 0) == 1);
}

TEST_CASE("Far-segment first access", "[far_segment]") {
    dramsim3::Config config("tests/configs/far_segment.ini", ".");
    dramsim3::Timing timing(config);
    dramsim3::ChannelState channel_state(config, timing);
    uint64_t cycle = 0;

    std::vector<TimedCommand> first =
        IssueUntil(channel_state, ReadCommand(7),
                   dramsim3::CommandType::READ, cycle);
    RequireTypes(first, {dramsim3::CommandType::TOGGLE_ON,
                         dramsim3::CommandType::ACTIVATE,
                         dramsim3::CommandType::TOGGLE_OFF,
                         dramsim3::CommandType::READ});
    REQUIRE(first[1].cycle - first[0].cycle >=
            static_cast<uint64_t>(config.tTGON));
    REQUIRE(first[2].cycle - first[1].cycle >=
            static_cast<uint64_t>(config.tRCD));
    REQUIRE(first[3].cycle - first[2].cycle >=
            static_cast<uint64_t>(config.tTGOFF));
}

TEST_CASE("Far-segment row hit", "[far_segment]") {
    dramsim3::Config config("tests/configs/far_segment.ini", ".");
    dramsim3::Timing timing(config);
    dramsim3::ChannelState channel_state(config, timing);
    uint64_t cycle = 0;
    IssueUntil(channel_state, ReadCommand(7), dramsim3::CommandType::READ,
               cycle);

    std::vector<TimedCommand> hit =
        IssueUntil(channel_state, ReadCommand(7),
                   dramsim3::CommandType::READ, cycle);
    RequireTypes(hit, {dramsim3::CommandType::READ});
}

TEST_CASE("Far-segment row conflict", "[far_segment]") {
    dramsim3::Config config("tests/configs/far_segment.ini", ".");
    dramsim3::Timing timing(config);
    dramsim3::ChannelState channel_state(config, timing);
    uint64_t cycle = 0;
    IssueUntil(channel_state, ReadCommand(7), dramsim3::CommandType::READ,
               cycle);

    std::vector<TimedCommand> conflict =
        IssueUntil(channel_state, ReadCommand(8),
                   dramsim3::CommandType::READ, cycle);
    RequireTypes(conflict, {dramsim3::CommandType::TOGGLE_ON,
                            dramsim3::CommandType::PRECHARGE,
                            dramsim3::CommandType::ACTIVATE,
                            dramsim3::CommandType::TOGGLE_OFF,
                            dramsim3::CommandType::READ});
    REQUIRE(conflict[1].cycle - conflict[0].cycle >=
            static_cast<uint64_t>(config.tTGON));
    REQUIRE(conflict[2].cycle - conflict[1].cycle >=
            static_cast<uint64_t>(config.tRP));
    REQUIRE(conflict[3].cycle - conflict[2].cycle >=
            static_cast<uint64_t>(config.tRCD));
    REQUIRE(conflict[4].cycle - conflict[3].cycle >=
            static_cast<uint64_t>(config.tTGOFF));
    REQUIRE(channel_state.OpenRow(0, 0, 0) == 8);
    REQUIRE(channel_state.RowHitCount(0, 0, 0) == 1);
}

TEST_CASE("Far-segment refresh closes an open row", "[far_segment]") {
    dramsim3::Config config("tests/configs/far_segment.ini", ".");
    dramsim3::Timing timing(config);
    dramsim3::ChannelState channel_state(config, timing);
    uint64_t cycle = 0;
    IssueUntil(channel_state, ReadCommand(7), dramsim3::CommandType::READ,
               cycle);

    dramsim3::Address refresh_addr(-1, 0, -1, -1, -1, -1);
    dramsim3::Command refresh(dramsim3::CommandType::REFRESH, refresh_addr,
                              static_cast<uint64_t>(-1));
    std::vector<TimedCommand> issued =
        IssueUntil(channel_state, refresh, dramsim3::CommandType::REFRESH,
                   cycle);
    RequireTypes(issued, {dramsim3::CommandType::TOGGLE_ON,
                          dramsim3::CommandType::PRECHARGE,
                          dramsim3::CommandType::TOGGLE_OFF,
                          dramsim3::CommandType::REFRESH});
    REQUIRE(issued[1].cycle - issued[0].cycle >=
            static_cast<uint64_t>(config.tTGON));
    REQUIRE(issued[2].cycle - issued[1].cycle >=
            static_cast<uint64_t>(config.tRP));
    REQUIRE(issued[3].cycle - issued[2].cycle >=
            static_cast<uint64_t>(config.tTGOFF));
}

TEST_CASE("Disabled far-segment toggles preserve baseline commands",
          "[far_segment]") {
    dramsim3::Config config("tests/configs/far_segment.ini", ".");
    config.enable_pim_switch = false;
    dramsim3::Timing timing(config);
    dramsim3::ChannelState channel_state(config, timing);
    uint64_t cycle = 0;

    std::vector<TimedCommand> issued =
        IssueUntil(channel_state, ReadCommand(7),
                   dramsim3::CommandType::READ, cycle);
    RequireTypes(issued, {dramsim3::CommandType::ACTIVATE,
                          dramsim3::CommandType::READ});
    REQUIRE(issued[1].cycle - issued[0].cycle ==
            static_cast<uint64_t>(config.tRCD));
}
