#pragma once

#include "lib/util/concepts.hpp"
#include "lib/util/spin_lock.hpp"

#include "receiver.hpp"
#include "state.hpp"

namespace otto::itc {
  // Declarations
  template<AState State>
  struct Consumer<State> : Receiver<state_change_action<State>> {
    using Action = state_change_action<State>;
    Consumer(Channel& channels) : Receiver<Action>(channels) {}

    /// Access the newest state available.
    const State& state() const noexcept
    {
      return state_;
    }

    // FOR UNIFORMITY WITH Consumer<State...>

    /// Access the newest state available.
    template<std::same_as<State> S>
    const S& state() const noexcept
    {
      return state();
    }

  protected:
    /// Hook called immediately after the state is updated.
    ///
    /// The parameter is the same as `this->state()`
    ///
    /// Override in subclass if needed
    virtual void on_state_change(const State& state) noexcept {}

  private:
    /// Called from {@ref TypedChannelLeaf::internal_commit}
    void receive(Action e) noexcept override
    {
      // {
      //   std::scoped_lock l(lock_);
      //   tmp_state_ = e.state;
      // }
      // executor()->execute([this] {
      //   // Apply changes
      //   {
      //     std::scoped_lock l(lock_);
      //     state_ = tmp_state_;
      //   }
      //   on_state_change(state_);
      // });
      // TODO: Proper implementation
      state_ = std::move(e.state);
      on_state_change(state_);
    }

    // util::spin_lock lock_;
    State state_;
    // State tmp_state_;
  };

  template<AState... States>
  struct Consumer : Consumer<States>... {
    Consumer(Channel& channels) : Consumer<States>(channels)... {}

    template<util::one_of<States...> S>
    const S& state() const noexcept
    {
      return Consumer<S>::state();
    }
  };

} // namespace otto::itc
