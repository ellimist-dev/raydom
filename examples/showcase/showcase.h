// Shared by the showcase's tabs.
#pragma once

#include <cstdint>
#include <raydom/raydom.hh>

inline constexpr std::uint32_t kBg = 0x0f1115, kPanel = 0x151922, kRaised = 0x1a1f2b, kLine = 0x272d3a,
                               kText = 0xe4e7ee, kMuted = 0x8a93a6, kAccent = 0x5b8cff, kAccentHi = 0x7aa2ff,
                               kAccentLo = 0x456fe0;

// Tour: a section per feature.
rd::Node build_tour(rd::Context& ctx);
void     tour_frame(const rd::Context& ctx);  // refreshes the status bar
rd::Node tour_scroller();                     // the scrolling column, for screenshots

// Messages: a desktop messaging app.
rd::Node build_messages(rd::Context& ctx);

// Network: a Paxos run in raydom::extras' network view.
rd::Node build_network(rd::Context& ctx);

// Router: an app on raydom::router, with a breadcrumb and a nested, kept-alive settings section.
rd::Node build_routes(rd::Context& ctx);
