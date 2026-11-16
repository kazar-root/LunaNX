#include "input/stream_menu_buttons.hpp"
#include "stream/live_metrics.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
    using namespace lunanx;
    using namespace std::chrono;
    const auto t = steady_clock::time_point(seconds(1));
    input::StreamMenuButtons buttons;
    auto state = buttons.Update(true, true, t);
    assert(state.toggle_stats && !state.start && !state.back && !state.guide);
    assert(!buttons.Update(true, true, t + seconds(1)).toggle_stats);
    state = buttons.Update(true, false, t + seconds(2));
    assert(!state.start && !state.guide && !state.back);
    assert(!buttons.Update(false, false, t + seconds(3)).start);
    assert(buttons.Update(true, true, t + seconds(4)).toggle_stats);

    // − peut précéder + de quelques images sans ouvrir le menu du jeu.
    buttons = {};
    assert(!buttons.Update(false, true, t).back);
    state = buttons.Update(true, true, t + milliseconds(40));
    assert(state.toggle_stats && !state.start && !state.back);
    assert(!buttons.Update(false, true, t + milliseconds(100)).back);
    assert(!buttons.Update(false, false, t + milliseconds(200)).start);

    // Les commandes individuelles gardent leurs impulsions garanties.
    buttons = {};
    assert(!buttons.Update(true, false, t).start);
    state = buttons.Update(false, false, t + milliseconds(50));
    assert(state.start && !state.guide);
    assert(buttons.OnReportDelivered(true, false, t + milliseconds(60)));
    assert(buttons.Update(false, false, t + milliseconds(100)).start);
    assert(!buttons.Update(false, false, t + milliseconds(300)).start);
    buttons = {};
    buttons.Update(true, false, t);
    assert(buttons.Update(true, false, t + milliseconds(600)).guide);
    assert(!buttons.Update(false, false, t + milliseconds(700)).start);
    buttons = {};
    buttons.Update(false, true, t);
    assert(buttons.Update(false, false, t + milliseconds(50)).back);
    buttons = {};
    buttons.Update(false, true, t);
    assert(buttons.Update(false, true, t + milliseconds(120)).back);
    assert(!buttons.Update(false, false, t + milliseconds(200)).back);

    // Un accord annule une éventuelle impulsion Start encore en attente de livraison.
    buttons = {};
    buttons.Update(true, false, t);
    assert(buttons.Update(false, false, t + milliseconds(20)).start);
    state = buttons.Update(true, true, t + milliseconds(40));
    assert(state.toggle_stats && !state.start && !state.back && !state.guide);

    StreamRateSampler rates;
    rates.Update(t, {});
    assert(!rates.ready());
    rates.Update(t + seconds(1), {1250000, 60});
    assert(rates.ready() && std::abs(rates.mbps() - 10) < 0.001 && std::abs(rates.fps() - 60) < 0.001);
    rates.Update(t + seconds(2), {1250000, 60});
    assert(rates.ready() && rates.mbps() == 0 && rates.fps() == 0);
    rates.Update(t + seconds(3), {100, 1});
    assert(!rates.ready());
    rates.Update(t + seconds(10), {2000000, 120});
    assert(!rates.ready());

    BatteryEstimate battery;
    assert(!battery.Update(t, 80, false));
    std::optional<int> remaining;
    for (int second = 1; second <= 60; ++second)
        remaining = battery.Update(t + seconds(second), 80 - second * 0.01, false);
    assert(remaining && *remaining == 132);
    assert(!battery.Update(t + seconds(61), 79.4, true));
    assert(!battery.Update(t + seconds(62), 79.4, false));
    assert(!battery.Update(t + seconds(120), 78, false)); // sommeil : repartir sans extrapoler
    assert(!battery.Update(t + seconds(121), -1, false));
    assert(!battery.Update(t + seconds(122), 101, false));
    assert(!battery.Update(t + seconds(123), NAN, false));
    std::cout << "Menu shortcut, measured rates and battery estimate: passed\n";
}
