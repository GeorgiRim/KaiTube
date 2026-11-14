#include "activity/google_login.hpp"
#include "newpipe/auth_store.hpp"
#include "newpipe/ui_text.hpp"
#include <borealis.hpp>
void show_google_login(bool) {
 auto* d=new brls::Dialog(newpipe::ui("Importa i cookie YouTube dal file /switch/kaitube_native_auth.txt sulla SD. Dopo l'importazione riavvia KaiTube per aggiornare tutte le sezioni."));
 d->addButton(newpipe::ui("Importa cookie"),[d]{std::string error;if(newpipe::AuthStore::instance().import_from_file({},&error)){d->close();brls::Application::notify(newpipe::ui("Cookie importati. Riavvia KaiTube."));}else brls::Application::notify(error);});
 d->addButton(newpipe::ui("Disconnetti"),[d]{std::string error;newpipe::AuthStore::instance().clear(&error);d->close();brls::Application::notify(error.empty()?newpipe::ui("Sessione rimossa. Elimina anche il file di importazione dalla SD."):error);});
 d->addButton(newpipe::ui("Chiudi"),[d]{d->close();});d->setCancelable(true);d->open();
}
