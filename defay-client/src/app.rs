use crate::config::{self, Theme};
use crate::state::{AppState, ChatMessage, Runtime};
use dioxus::events::Key;
use dioxus::prelude::*;
use dioxus_desktop::{
    tao::event::{Event, WindowEvent},
    use_wry_event_handler,
};
use once_cell::sync::OnceCell;
use std::sync::atomic::Ordering;
use std::sync::Arc;

static RUNTIME: OnceCell<(Arc<AppState>, config::Paths)> = OnceCell::new();

pub fn set_runtime(state: Arc<AppState>, paths: config::Paths) {
    let _ = RUNTIME.set((state, paths));
}

fn runtime() -> &'static (Arc<AppState>, config::Paths) {
    RUNTIME.get().expect("runtime not initialized")
}

#[component]
pub fn App() -> Element {
    let mut route = use_signal(|| Route::Onboarding);
    let search_query = use_signal(String::new);
    let search_results = use_signal(Vec::<usize>::new);
    let mut show_settings = use_signal(|| false);
    let mut show_diag = use_signal(|| false);
    let input_text = use_signal(String::new);
    let ui_scale = use_signal(|| runtime().0.cfg.lock().unwrap().ui_scale);

    let theme_class = {
        let cfg = runtime().0.cfg.lock().unwrap().theme;
        match cfg {
            Theme::Light => "light".to_string(),
            Theme::Dark => "dark".to_string(),
            Theme::System => match dark_light::detect() {
                dark_light::Mode::Dark => "dark".into(),
                _ => "light".into(),
            },
        }
    };

    let state_for_focus = runtime().0.clone();
    use_wry_event_handler(move |event, _| {
        if let Event::WindowEvent {
            event: WindowEvent::Focused(f),
            ..
        } = event
        {
            state_for_focus.window_focused.store(*f, Ordering::SeqCst);
        }
    });

    use_effect(move || {
        let state = runtime().0.clone();

        log::info!("app: starting background services");
        state.start_background();

        let cfg = state.cfg.lock().unwrap().clone();
        if cfg.auto_connect && !cfg.last_nick.is_empty() {
            log::info!(
                "app: auto_connect to {}:{} as {}",
                cfg.last_host,
                cfg.last_port,
                cfg.last_nick
            );
            route.set(Route::Chat);
            spawn(async move {
                state.connect().await;
            });
        }
    });

    rsx! {
        div { class: "{theme_class}", style: "zoom: {ui_scale.read()}",
            match route()
            {
                Route::Onboarding => rsx!(OnboardingScreen { route: route.clone() }),
                Route::Chat => rsx!(
                    MainShell {
                        route: route.clone(),
                        show_settings: show_settings.clone(),
                        show_diag: show_diag.clone(),
                        search_query: search_query.clone(),
                        search_results: search_results.clone(),
                        input_text: input_text.clone(),
                    }
                    if *show_settings.read() {
                        SettingsDialog { on_close: move |_| show_settings.set(false), ui_scale: ui_scale.clone() }
                    }
                    if *show_diag.read() {
                        DiagnosticsDialog { on_close: move |_| show_diag.set(false) }
                    }
                )
            }
        }
    }
}

#[derive(Clone, Copy, PartialEq)]
enum Route {
    Onboarding,
    Chat,
}

#[component]
fn OnboardingScreen(route: Signal<Route>) -> Element {
    let state = runtime().0.clone();
    let paths = &runtime().1;

    let mut host = use_signal(|| state.cfg.lock().unwrap().last_host.clone());
    let mut port = use_signal(|| state.cfg.lock().unwrap().last_port);
    let mut nick = use_signal(|| state.cfg.lock().unwrap().last_nick.clone());
    let mut auto = use_signal(|| state.cfg.lock().unwrap().auto_connect);
    let mut error_text = use_signal(String::new);

    let connect = move |_| {
        error_text.set(String::new());
        log::info!(
            "onboarding: connect pressed host={} port={} nick={} auto={}",
            host.read(),
            port.read(),
            nick.read(),
            auto.read()
        );
        if nick.read().trim().is_empty() {
            error_text.set("Введите никнейм".into());
            return;
        }
        let mut new_cfg = state.cfg.lock().unwrap().clone();
        new_cfg.last_host = host.read().clone();
        new_cfg.last_port = *port.read();
        new_cfg.last_nick = nick.read().trim().to_string();
        new_cfg.auto_connect = *auto.read();

        if let Err(e) = config::save(&paths.cfg_path, &new_cfg) {
            let msg = format!("Не удалось сохранить конфиг: {e}");
            log::error!("onboarding: {msg}");
            error_text.set(msg);
            return;
        }
        log::info!("onboarding: config saved, connecting");
        *state.cfg.lock().unwrap() = new_cfg.clone();
        route.set(Route::Chat);
        let st = state.clone();
        spawn(async move {
            st.connect().await;
        });
    };

    rsx! {
        div { class: "onboarding fade-in slide-right",
            div { class: "logo",
                img { src: "assets/logo.png", alt: "Defay 1x9 logo" }
                h1 { "Defay 1x9" }
                p { "LAN-чат клиент" }
            }
            div { class: "form",
                label { "Сервер" }
                input {
                    value: "{host}",
                    oninput: move |e| host.set(e.value())
                }
                label { "Порт" }
                input {
                    r#type: "number",
                    min: "1",
                    max: "65535",
                    value: "{port}",
                    oninput: move |e| {
                        if let Ok(v) = e.value().parse::<u16>() {
                            port.set(v);
                        }
                    }
                }
                label { "Никнейм" }
                input {
                    maxlength: "32",
                    value: "{nick}",
                    oninput: move |e| nick.set(e.value())
                }
                div { class: "row",
                    input {
                        r#type: "checkbox",
                        checked: "{auto}",
                        oninput: move |e| auto.set(e.checked())
                    }
                    span { "Автоподключение при запуске" }
                }
                if !error_text.read().is_empty() {
                    div { class: "error", "{error_text}" }
                }
                button { class: "btn primary", onclick: connect, "Подключиться" }
            }
        }
    }
}

#[component]
fn MainShell(
    route: Signal<Route>,
    show_settings: Signal<bool>,
    show_diag: Signal<bool>,
    search_query: Signal<String>,
    search_results: Signal<Vec<usize>>,
    input_text: Signal<String>,
) -> Element {
    let state = runtime().0.clone();
    let connected = *state.connected.lock().unwrap();
    let status = state.status_text.lock().unwrap().clone();

    use std::rc::Rc;
    let send_closure: Rc<dyn Fn()> = {
        let state_for_send = state.clone();
        let input_signal = input_text.clone();
        Rc::new(move || {
            let msg = input_signal.read().clone();
            if msg.trim().is_empty() {
                return;
            }
            log::info!("ui: sending message '{}' ({} bytes)", msg, msg.len());
            let st = state_for_send.clone();
            spawn(async move {
                if let Err(err) = st.send_message(msg).await {
                    log::error!("ui: send_message failed: {err}");
                    *st.status_text.lock().unwrap() = err.to_string();
                }
            });
            let mut sig = input_signal.clone();
            sig.set(String::new());
        })
    };

    let state_for_search = state.clone();
    let do_search = move |_| {
        let q = search_query.read().clone();
        if q.trim().is_empty() {
            log::info!("ui: cleared search");
            search_results.set(vec![]);
        } else {
            let found = state_for_search.search(&q);
            let list = state_for_search.messages.lock().unwrap();
            let indices: Vec<usize> = found
                .into_iter()
                .filter_map(|fm| list.iter().position(|m| m.ts_ms == fm.ts_ms))
                .collect();
            log::info!("ui: search for '{q}' found {} results", indices.len());
            search_results.set(indices);
        }
    };

    rsx! {
        div { class: "app fade-in slide-left",
            header {
                div { class: "title", "Defay 1x9" }
                div { class: "status", if connected { "Подключено" } else { "{status}" } }
                div { class: "spacer" }
                button { class: "btn danger", onclick: move |_| Runtime(state.clone()).disconnect(), "Отключиться" }
                button { class: "btn", onclick: move |_| show_settings.set(true), "Настройки" }
                button { class: "btn", onclick: move |_| show_diag.set(true), "Диагностика" }
            }
            div { class: "toolbar",
                input {
                    placeholder: "Поиск...",
                    value: "{search_query}",
                    oninput: move |e| search_query.set(e.value())
                }
                button { class: "btn", onclick: do_search, "Найти" }
                if !search_results.read().is_empty() {
                    span { class: "hint", "Найдено: {search_results.read().len()}" }
                }
            }
            div { class: "chat",
                ChatList { matches: search_results.read().clone() }
            }
            div { class: "composer",
                textarea {
                    rows: "3",
                    placeholder: "Напишите сообщение…",
                    value: "{input_text}",
                    oninput: move |e| input_text.set(e.value()),
                    onkeydown: {
                        let send = send_closure.clone();
                        move |e| {
                            if e.key() == Key::Enter && !e.modifiers().shift() {
                                log::info!("ui: Enter pressed, sending message");
                                (send)();
                            }
                        }
                    }
                }
                button {
                    class: "btn primary",
                    onclick: {
                        let send = send_closure.clone();
                        move |_| (send)()
                    },
                    "Отправить"
                }
                FloodIndicator {}
            }
        }
    }
}

#[component]
fn ChatList(matches: Vec<usize>) -> Element {
    let state = runtime().0.clone();
    let msg_count = use_signal(|| 0_u64);
    let should_scroll = use_signal(|| false);
    {
        let state = state.clone();
        let mc = msg_count.clone();
        let should_scroll = should_scroll.clone();
        use_effect(move || {
            let mut rx = state.msg_tx.subscribe();
            let mut mc = mc.clone();
            let st = state.clone();
            let mut should_scroll = should_scroll.clone();
            spawn(async move {
                while rx.changed().await.is_ok() {
                    let count = *rx.borrow();
                    log::info!("ui: chat list received update, total messages {count}");
                    mc.set(count);
                    let my_nick = st.cfg.lock().unwrap().last_nick.clone();
                    let list = st.messages.lock().unwrap();
                    if list.last().map_or(false, |last| last.username == my_nick) {
                        should_scroll.set(true);
                    }
                }
            });
        });
    }

    {
        let mut should_scroll = should_scroll.clone();
        use_effect(move || {
            if *should_scroll.read() {
                should_scroll.set(false);
                let fut = eval(
                    "const c=document.querySelector('.chat'); if(c){ c.scrollTop=c.scrollHeight; }",
                );
                spawn(async move {
                    let _ = fut.await;
                });
            }
        });
    }

    let _ = msg_count();
    let list = state.messages.lock().unwrap().clone();
    let match_set: std::collections::HashSet<usize> = matches.into_iter().collect();
    rsx! {
        div { class: "list",
            for (i, m) in list.iter().enumerate() {
                ChatBubble { key: "{m.ts_ms}-{i}", index: i, highlighted: match_set.contains(&i) }
            }
        }
    }
}
#[component]
fn ChatBubble(index: usize, highlighted: bool) -> Element {
    let state = runtime().0.clone();
    let list = state.messages.lock().unwrap();
    let m: &ChatMessage = &list[index];
    let my_nick = state.cfg.lock().unwrap().last_nick.clone();
    let is_self = m.username == my_nick;

    rsx! {
        div { class: format_args!("bubble {} {} fade-in",
                                   if highlighted { "hl" } else { "" },
                                   if is_self { "self slide-right" } else { "slide-left" }),
            div { class: "meta",
                span { class: "user", "{m.username}" }
            }
            div { class: "text", "{m.text}" }
            div { class: "time", "{crate::state::AppState::format_ts(m.ts_ms)}" }
        }
    }
}

#[component]
fn FloodIndicator() -> Element {
    let state = runtime().0.clone();
    let avail = state.rate.available();
    if avail == 0 {
        let wait = state.rate.seconds_to_next();
        rsx!( span { class: "flood warn", "Анти-флуд: подождите {wait} сек." } )
    } else {
        rsx!( span { class: "flood ok", "Можно отправить: {avail}" } )
    }
}

#[component]
fn SettingsDialog(on_close: EventHandler<()>, ui_scale: Signal<f32>) -> Element {
    let state = runtime().0.clone();
    let paths = &runtime().1;
    let work = std::sync::Arc::new(std::sync::Mutex::new(state.cfg.lock().unwrap().clone()));

    let theme_value = {
        let w = work.lock().unwrap();
        match w.theme {
            Theme::System => "system",
            Theme::Light => "light",
            Theme::Dark => "dark",
        }
    };

    let ui_scale_value = format!("{}", ui_scale.read());
    let notifications_checked = work.lock().unwrap().notifications_enabled;
    let flood_burst_value = format!("{}", work.lock().unwrap().flood_limit_burst);
    let flood_window_value = format!("{}", work.lock().unwrap().flood_limit_window_sec);

    let work_for_save = work.clone();
    let mut ui_scale_save = ui_scale.clone();
    let save = move |_| {
        let w = work_for_save.lock().unwrap().clone();
        *state.cfg.lock().unwrap() = w.clone();
        ui_scale_save.set(w.ui_scale);
        let _ = config::save(&paths.cfg_path, &w);
        on_close.call(());
    };

    rsx! {
        div { class: "modal fade-in",
            div { class: "dialog scale-in",
                h2 { "Настройки" }
                div { class: "grid",
                    label { "Тема" }
                    select {
                        value: "{theme_value}",
                        oninput: {
                            let work = work.clone();
                            move |e| {
                                let mut w = work.lock().unwrap();
                                w.theme = match e.value().as_str() {
                                    "light" => Theme::Light,
                                    "dark"  => Theme::Dark,
                                    _       => Theme::System,
                                }
                            }
                        },
                        option { value: "system", "Системная" }
                        option { value: "light", "Светлая" }
                        option { value: "dark", "Тёмная" }
                    }

                    label { "Масштаб UI" }
                    input {
                        r#type: "number",
                        step: "0.05",
                        min: "0.75",
                        max: "2.0",
                        value: "{ui_scale_value}",
                        oninput: {
                            let work = work.clone();
                            let mut ui_scale = ui_scale.clone();
                            move |e| {
                                if let Ok(v) = e.value().parse::<f32>() {
                                    let clamped = v.max(0.75).min(2.0);
                                    let mut w = work.lock().unwrap();
                                    w.ui_scale = clamped;
                                    ui_scale.set(clamped);
                                }
                            }
                        }
                    }

                    label { "Уведомления Windows" }
                    input {
                        r#type: "checkbox",
                        checked: "{notifications_checked}",
                        oninput: {
                            let work = work.clone();
                            move |e| {
                                let mut w = work.lock().unwrap();
                                w.notifications_enabled = e.checked();
                            }
                        }
                    }

                    label { "Анти-флуд: burst" }
                    input {
                        r#type: "number",
                        min: "1",
                        max: "50",
                        value: "{flood_burst_value}",
                        oninput: {
                            let work = work.clone();
                            move |e| {
                                if let Ok(v) = e.value().parse::<u32>() {
                                    let mut w = work.lock().unwrap();
                                    w.flood_limit_burst = v;
                                }
                            }
                        }
                    }

                    label { "Анти-флуд: окно (сек)" }
                    input {
                        r#type: "number",
                        min: "5",
                        max: "120",
                        value: "{flood_window_value}",
                        oninput: {
                            let work = work.clone();
                            move |e| {
                                if let Ok(v) = e.value().parse::<u64>() {
                                    let mut w = work.lock().unwrap();
                                    w.flood_limit_window_sec = v;
                                }
                            }
                        }
                    }
                }
                div { class: "actions",
                    button { onclick: save, class: "btn primary", "Сохранить" }
                    button { onclick: move |_| on_close.call(()), class: "btn", "Закрыть" }
                }
            }
        }
    }
}

#[component]
fn DiagnosticsDialog(on_close: EventHandler<()>) -> Element {
    let state = runtime().0.clone();
    let paths = &runtime().1;

    let status = state.status_text.lock().unwrap().clone();
    let logs_dir = paths.logs_dir.display().to_string();

    let state_for_copy = state.clone();
    let logs_dir_copy = logs_dir.clone();
    let copy_diag = move |_| {
        let text = format!(
            "Статус: {}\nПодключено: {}\nСообщений в памяти: {}\nЛоги: {}\n",
            state_for_copy.status_text.lock().unwrap().clone(),
            *state_for_copy.connected.lock().unwrap(),
            state_for_copy.messages.lock().unwrap().len(),
            logs_dir_copy,
        );
        let _ = arboard::Clipboard::new().and_then(|mut c| c.set_text(text));
    };

    rsx! {
        div { class: "modal fade-in",
            div { class: "dialog scale-in",
                h2 { "Диагностика" }
                pre { "{status}" }
                p { "Логи в: {logs_dir}" }
                div { class: "actions",
                    button { onclick: copy_diag, class: "btn", "Скопировать диагностику" }
                    button { onclick: move |_| on_close.call(()), class: "btn", "Закрыть" }
                }
            }
        }
    }
}

#[cfg(windows)]
pub fn notify_win_toast(title: &str, body: &str) -> anyhow::Result<()> {
    use windows::Data::Xml::Dom::XmlDocument;
    use windows::UI::Notifications::{ToastNotification, ToastNotificationManager};

    let xml = format!(
        r#"<toast activationType="foreground">
            <visual>
              <binding template="ToastGeneric">
                <text>{}</text>
                <text>{}</text>
              </binding>
            </visual>
          </toast>"#,
        escape_xml(title),
        escape_xml(body)
    );

    let xml_doc = XmlDocument::new()?;
    xml_doc.LoadXml(&windows::core::HSTRING::from(xml))?;

    let notifier = ToastNotificationManager::CreateToastNotifierWithId(
        &windows::core::HSTRING::from("Defay.Client"),
    )?;
    let toast = ToastNotification::CreateToastNotification(&xml_doc)?;
    notifier.Show(&toast)?;
    Ok(())
}

#[cfg(windows)]
fn escape_xml(s: &str) -> String {
    s.replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
}
