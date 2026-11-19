#include "activity/google_login.hpp"

#include <algorithm>
#include <climits>

#include "newpipe/google_device.hpp"
#include "newpipe/i18n.hpp"
#include "newpipe/runtime.hpp"
#include "newpipe/ui_job.hpp"
#include "view/tab_focus.hpp"

namespace {
using namespace newpipe;

void account_changed() {
    restore_video_focus = false;
    restore_activity = {};
    ui_reload = true;
}

class DeviceDialog final : public brls::Dialog {
public:
    explicit DeviceDialog(GoogleClient client, brls::Box* content = new brls::Box())
        : brls::Dialog(content), client_(std::move(client)) {
        content->setPadding(24);
        label_ = new brls::Label();
        label_->setSingleLine(false);
        label_->setHorizontalAlign(brls::HorizontalAlign::CENTER);
        label_->setText(ui("Richiesta del codice a Google..."));
        content->addView(label_);
        addButton(ui("Annulla"), [] {});
    }

    void willDisappear(bool reset = false) override {
        life_->alive = false;
        brls::Dialog::willDisappear(reset);
    }
    ~DeviceDialog() override { life_->alive = false; }

    void start() {
        open();
        ui_job(life_, [client = client_] {
            HttpsHttpClient http;
            std::string error;
            auto code = request_device_code(http, client, error);
            return std::make_pair(std::move(code), error);
        }, [this](auto result, const std::string& exception) {
            if (!result.first) {
                label_->setText(exception.empty() ? result.second : ui("Connessione a Google non riuscita"));
                return;
            }
            code_ = std::move(*result.first);
            label_->setText(ui("Apri questo indirizzo sul telefono e inserisci il codice:") +
                            "\n\n" + code_.verification_url + "\n\n" + code_.user_code);
            schedule_poll();
        });
    }

private:
    void schedule_poll() {
        using namespace std::chrono;
        const auto remaining = duration_cast<milliseconds>(code_.deadline - steady_clock::now()).count();
        if (remaining <= 0) { label_->setText(ui("Codice scaduto")); return; }
        const auto delay = std::min<int64_t>(remaining + 1, int64_t(code_.interval_seconds) * 1000);
        brls::delay(delay, [this, life = life_] {
            if (!life->alive) return;
            ui_job(life, [client = client_, code = code_] {
                HttpsHttpClient http;
                return poll_device_code(http, client, code);
            }, [this](PollResult result, const std::string& exception) {
                if (!exception.empty()) { label_->setText(ui("Connessione a Google non riuscita")); return; }
                switch (result.state) {
                    case PollState::failed:
                        label_->setText(result.error);
                        return;
                    case PollState::granted: {
                        std::string error;
                        if (!AuthStore::instance().save_google_tokens(result.tokens, error)) {
                            label_->setText(error);
                            return;
                        }
                        close([] {
                            brls::Application::notify(ui("Account Google collegato. Le iscrizioni vengono lette tramite YouTube Data API."));
                            ui_tab = 2;
                            account_changed();
                        });
                        return;
                    }
                    case PollState::slow_down:
                        code_.interval_seconds += std::min(5, INT_MAX - code_.interval_seconds);
                        break;
                    case PollState::retry:
                        code_.interval_seconds += std::min(code_.interval_seconds, INT_MAX - code_.interval_seconds);
                        break;
                    case PollState::pending:
                        break;
                }
                schedule_poll();
            });
        });
    }

    GoogleClient client_;
    DeviceCode code_;
    brls::Label* label_;
    std::shared_ptr<UiLifetime> life_ = std::make_shared<UiLifetime>();
};
}  // namespace

void show_google_login() {
    using namespace newpipe;
    const auto session = AuthStore::instance().session();
    auto* dialog = new brls::Dialog(session.authenticated() ? ui("Account salvato") : ui("Accedi al tuo account Google"));
    dialog->addButton(ui("Accedi con il telefono"), [] {
        std::string error;
        auto client = load_google_client(error);
        if (!client) {
            auto* setup = new brls::Dialog(error);
            setup->addButton(ui("Chiudi"), [] {});
            setup->open();
            return;
        }
        (new DeviceDialog(std::move(*client)))->start();
    });
    dialog->addButton(tr("settings/session/title"), [] {
        std::string error;
        if (AuthStore::instance().import_from_file({}, &error)) account_changed();
        else brls::Application::notify(error);
    });
    if (session.authenticated()) {
        dialog->addButton(ui("Disconnetti"), [] {
            std::string error;
            if (AuthStore::instance().clear(&error)) account_changed();
            else brls::Application::notify(error);
        });
    } else {
        dialog->addButton(ui("Chiudi"), [] {});
    }
    dialog->open();
}
