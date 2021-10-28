#include <string>

#include <choreograph/Choreograph.h>
#include <fmt/format.h>

#include "lib/util/with_limits.hpp"

#include "lib/itc/itc.hpp"
#include "lib/skia/skia.hpp"
#include "lib/widget.hpp"

#include "app/engines/synths/ottofm/state.hpp"
#include "app/input.hpp"
#include "app/services/config.hpp"
#include "app/services/graphics.hpp"
#include "app/services/ui_manager.hpp"

#include "ottofm.hpp"
#include "widgets.hpp"

namespace otto::engines::ottofm {

  struct ModHandler final : InputReducer<State>, IInputLayer {
    using InputReducer::InputReducer;

    [[nodiscard]] util::enum_bitset<Key> key_mask() const noexcept override
    {
      return key_groups::enc_clicks + Key::shift;
    }

    void reduce(KeyPress e, State& state) noexcept final
    {
      switch (e.key) {
        case Key::blue_enc_click: state.cur_op_idx = 3; break;
        case Key::green_enc_click: state.cur_op_idx = 2; break;
        case Key::yellow_enc_click: state.cur_op_idx = 1; break;
        case Key::red_enc_click: state.cur_op_idx = 0; break;
        case Key::shift: state.shift = true; break;
        default: break;
      }
    }

    void reduce(KeyRelease e, State& state) noexcept final
    {
      switch (e.key) {
        case Key::shift: state.shift = false; break;
        default: break;
      }
    }

    void reduce(EncoderEvent e, State& state) noexcept final
    {
      switch (e.encoder) {
        case Encoder::blue: {
          if (!state.shift) {
            state.current_op().envelope.attack += e.steps * 0.01;
          } else {
            for (auto& op : state.operators) op.envelope.attack += e.steps * 0.01;
          }
        } break;
        case Encoder::green: {
          if (!state.shift) {
            state.current_op().envelope.decay += e.steps * 0.01;
          } else {
            for (auto& op : state.operators) op.envelope.decay += e.steps * 0.01;
          }
        } break;
        case Encoder::yellow: {
          if (!state.shift) {
            state.current_op().envelope.sustain += e.steps * 0.01;
          } else {
            for (auto& op : state.operators) op.envelope.sustain += e.steps * 0.01;
          }
        } break;
        case Encoder::red: {
          if (!state.shift) {
            state.current_op().envelope.release += e.steps * 0.01;
          } else {
            for (auto& op : state.operators) op.envelope.release += e.steps * 0.01;
          }
        } break;
      }
    }
  };

  struct ADSRGraphic {
    ADSRGraphic(int idx) : index(idx) {}
    int index;
    bool active = false;
    ADSR graphic;
    skia::Anim<float> size = {0, 0.25};
    void on_state_change(const State& s)
    {
      const auto& env = s.operators[index].envelope;
      graphic.a = env.attack;
      graphic.d = env.decay;
      graphic.s = env.sustain;
      graphic.r = env.release;
      active = s.cur_op_idx == index;
      graphic.active = active || s.shift;
      size = active ? 1.f : 0.f;
    }

    int will_change(const State& s)
    {
      const auto& env = s.operators[index].envelope;
      if (graphic.a != env.attack) return 1;
      if (graphic.d != env.decay) return 2;
      if (graphic.s != env.sustain) return 3;
      if (graphic.r != env.release) return 4;
      return 0;
    }
  };

  struct ModScreen final : itc::Consumer<State, AudioState>, ScreenBase {
    using Consumer::Consumer;

    Operators ops{Consumer<AudioState>::state().activity};
    // Operators are numbered from the bottom up
    std::array<ADSRGraphic, 4> envelopes = {3, 2, 1, 0};

    float env_size = 0;

    ModScreen(itc::Channel& c) : Consumer(c)
    {
      ops.bounding_box = {{10, 30}, {50, 180}};
      for (auto& env : envelopes) env.on_state_change(Consumer<State>::state());
    }

    void on_state_change(const State& s) noexcept override
    {
      ops.algorithm_idx = s.algorithm_idx;
      ops.cur_op = s.cur_op_idx;
      // Before overwriting the envelope state, we check if the active envelope has any changes to show with the pop-up
      switch (envelopes[3 - ops.cur_op].will_change(s)) {
        case 1:
          last_changed_parameter = std::string("ATTACK");
          value_str =
            fmt::format("{:4.0f}ms", 1000 * envelope_stage_duration(s.operators[s.cur_op_idx].envelope.attack));
          popup_brightness = 1.0f;
          break;
        case 2:
          last_changed_parameter = std::string("DECAY");
          value_str =
            fmt::format("{:4.0f}ms", 1000 * envelope_stage_duration(s.operators[s.cur_op_idx].envelope.decay));
          popup_brightness = 1.0f;
          break;
        case 3:
          last_changed_parameter = std::string("SUSTAIN");
          value_str = fmt::format("{:.2}", s.operators[s.cur_op_idx].envelope.sustain);
          popup_brightness = 1.0f;
          break;
        case 4:
          last_changed_parameter = std::string("RELEASE");
          value_str =
            fmt::format("{:4.0f}ms", 1000 * envelope_stage_duration(s.operators[s.cur_op_idx].envelope.release));
          popup_brightness = 1.0f;
          break;
        default: break;
      }
      for (auto& env : envelopes) env.on_state_change(s);
    }

    void on_state_change(const AudioState&) noexcept override{};

    void draw(skia::Canvas& ctx) noexcept override
    {
      ops.draw(ctx);

      // Draw params for envelopes
      constexpr int active_y = 50;
      constexpr int not_active_y = 15;
      constexpr int y_pad = 33;
      constexpr int x_start = 80;
      constexpr int x_size = 220;
      float step = (skia::height - 2 * y_pad - active_y - not_active_y * 3) / 3.f;

      float upper_y = y_pad;
      for (auto& env : envelopes) {
        auto& graphic = env.graphic;
        env_size = active_y * env.size + not_active_y * (1 - env.size);
        graphic.bounding_box.move_to({x_start, upper_y});
        graphic.bounding_box.resize({x_size, env_size});
        graphic.expanded = env.size;
        graphic.active_segment = state<AudioState>().stage[env.index];
        graphic.draw(ctx);
        // If knobs are being turned, show the pop-up
        if (env.active) {
          constexpr float padding = 6;
          skia::place_text(ctx, last_changed_parameter, fonts::regular(14), colors::white.brighten(popup_brightness),
                           {x_start, upper_y + env_size + padding}, anchors::top_left);
          skia::place_text(ctx, value_str, fonts::regular(14), colors::white.brighten(popup_brightness),
                           {x_start + x_size, upper_y + env_size + padding}, anchors::top_right);
        }

        upper_y += env_size + step;
      }
    }

    skia::ReturnTo<float> popup_brightness = {0, 0.5, 0.15};
    std::string last_changed_parameter;
    std::string value_str;
  };

  ScreenWithHandler make_mod_screen(itc::Channel& chan)
  {
    return {
      .screen = std::make_unique<ModScreen>(chan),
      .input = std::make_unique<ModHandler>(chan),
    };
  }

} // namespace otto::engines::ottofm
