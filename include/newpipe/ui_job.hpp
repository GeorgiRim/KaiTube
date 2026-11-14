#include "newpipe/ui_text.hpp"
#pragma once
#include <atomic>
#include "newpipe/http_client.hpp"
#include <borealis.hpp>
#include <memory>
#include "newpipe/log.hpp"
#include <type_traits>
#include <utility>
namespace newpipe {
struct UiLifetime {
  std::atomic_bool alive{true};
  std::atomic<unsigned> generation{0};
};
template <class Work, class Done>
void ui_job(std::shared_ptr<UiLifetime> life, Work work, Done done) {
  unsigned generation = ++life->generation;
  brls::async([life, generation, work = std::move(work),
               done = std::move(done)]() mutable {
    HttpCancelScope cancellation(&ui_requests_cancel);
    if (!life->alive || ui_requests_cancel.load())
      return;
    using Value = std::invoke_result_t<Work>;
    Value value{};
    std::string error;
    try {
      logf("ui_job: work begin generation=%u", generation);
      value = work();
      logf("ui_job: work end generation=%u", generation);
    } catch (const std::exception &e) {
      error = e.what();
    } catch (...) {
      error = newpipe::ui("Caricamento non riuscito");
    }
    brls::sync([life, generation, value = std::move(value), error,
                done = std::move(done)]() mutable {
      if (life->alive && life->generation == generation)
        { logf("ui_job: apply begin generation=%u",generation); done(std::move(value), error); logf("ui_job: apply end generation=%u",generation); }
    });
  });
}
} // namespace newpipe
