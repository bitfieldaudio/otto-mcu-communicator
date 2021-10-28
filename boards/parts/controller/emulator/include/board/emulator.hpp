#pragma once

#include "core/ui/canvas.hpp"
#include "services/controller.hpp"
#include "util/enum.hpp"

namespace otto::board {

  struct Emulator : core::ui::vg::Drawable, services::Controller {
    enum struct ClickAction { down, up };

    void draw(core::ui::vg::Canvas& ctx) override;

    void set_color(services::LED, services::LEDColor) override;
    void flush_leds() override;
    void clear_leds() override;

    void handle_click(core::ui::vg::Point p, ClickAction);
    void handle_scroll(core::ui::vg::Point p, float offset);

    constexpr static core::ui::vg::Size size = {1115, 352};

  private:
    template<typename LEDFunc, typename BTNFunc>
    void draw_btn(core::ui::vg::Canvas& ctx, core::input::Key key, LEDFunc&& lf, BTNFunc&& bf);

    template<typename BTNFunc, typename LEDFunc>
    void draw_s_btn(core::ui::vg::Canvas& ctx, core::input::Key key, BTNFunc&& bf, LEDFunc&& lf);

    template<typename BTNFunc, typename LEDFunc>
    void draw_c_btn(core::ui::vg::Canvas& ctx, core::input::Key key, BTNFunc&& bf, LEDFunc&& lf);

    void draw_func_btns(core::ui::vg::Canvas& ctx);
    void draw_sc_btns(core::ui::vg::Canvas& ctx);
    void draw_encoders(core::ui::vg::Canvas& ctx);

    util::enum_map<core::input::Key, services::LEDColor> _led_colors = {};
  };

} // namespace otto::board


// kak: other_file=../../src/emulator.cpp
