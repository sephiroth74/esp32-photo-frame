use arboard;
use ratatui::widgets::Padding;
use ratatui::{
    Frame, Terminal,
    backend::CrosstermBackend,
    crossterm::{
        event::{self, DisableMouseCapture, EnableMouseCapture, Event, KeyCode, KeyEventKind},
        execute,
        terminal::{EnterAlternateScreen, LeaveAlternateScreen, disable_raw_mode, enable_raw_mode},
    },
    layout::{Alignment, Constraint, Direction, Layout, Rect},
    style::{Color, Modifier, Style},
    text::{Line, Span},
    widgets::{Block, Borders, Clear, List, ListItem, ListState, Paragraph, Wrap},
};
use reqwest::blocking::Client;
use serde::{Deserialize, Serialize};
use std::io;
use std::sync::{Arc, Mutex};
use std::{collections::VecDeque, path::Path};
use tracing::field::{Field, Visit};
use tracing::{Event as TracingEvent, Subscriber};
use tracing::{debug, error, info};
use tracing_subscriber::layer::Context;
use tracing_subscriber::registry::LookupSpan;
use tracing_subscriber::{layer::SubscriberExt, util::SubscriberInitExt};
use tui_input::Input;
use tui_input::backend::crossterm::EventHandler;

mod google_auth;
mod project;

use google_auth::{GoogleAuthFileManager, GoogleAuthStatus};
use project::{ProjectFileManager, ProjectKeys};

const MAX_LOG_ENTRIES: usize = 5000;

#[derive(Debug, Deserialize)]
struct GoogleAlbumResponse {
    id: Option<String>,
    title: Option<String>,
}

#[derive(Debug, Serialize)]
struct CreateAlbumRequest {
    album: CreateAlbumPayload,
}

#[derive(Debug, Serialize)]
struct CreateAlbumPayload {
    title: String,
}

#[derive(Debug)]
enum PendingOperation {
    CheckAlbum,
    CreateAlbum { album_name: String },
    TestGoogleApi,
}

#[derive(Default)]
struct AppLogFieldVisitor {
    message: Option<String>,
    fields: Vec<String>,
}

impl Visit for AppLogFieldVisitor {
    fn record_debug(&mut self, field: &Field, value: &dyn std::fmt::Debug) {
        let value_str = format!("{:?}", value);
        if field.name() == "message" {
            self.message = Some(value_str.trim_matches('"').to_string());
        } else {
            self.fields.push(format!("{}={}", field.name(), value_str));
        }
    }
}

#[derive(Clone)]
struct AppLogCaptureLayer {
    logs: Arc<Mutex<VecDeque<String>>>,
    max_entries: usize,
}

impl AppLogCaptureLayer {
    fn new(logs: Arc<Mutex<VecDeque<String>>>, max_entries: usize) -> Self {
        Self { logs, max_entries }
    }
}

impl<S> tracing_subscriber::Layer<S> for AppLogCaptureLayer
where
    S: Subscriber + for<'span> LookupSpan<'span>,
{
    fn on_event(&self, event: &TracingEvent<'_>, _ctx: Context<'_, S>) {
        let mut visitor = AppLogFieldVisitor::default();
        event.record(&mut visitor);

        let meta = event.metadata();
        let mut line = format!("[{}]", meta.level());

        if let Some(message) = visitor.message {
            line.push(' ');
            line.push_str(&message);
        }

        if !visitor.fields.is_empty() {
            if !line.ends_with(']') {
                line.push(' ');
            }
            line.push_str(&visitor.fields.join(" "));
        }

        if let Ok(mut logs) = self.logs.lock() {
            logs.push_back(line);
            while logs.len() > self.max_entries {
                logs.pop_front();
            }
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
enum DashboardEntry {
    LoginToGoogle,
    TestGoogleApi,
    LogoutFromGoogle,
    CreateGooglePhotoAlbum,
    AlbumName,
    Back,
}

impl DashboardEntry {
    fn label(&self, album_name: Option<&str>) -> String {
        match self {
            Self::LoginToGoogle => "Login to Google".to_string(),
            Self::TestGoogleApi => "Test Google api".to_string(),
            Self::LogoutFromGoogle => "Logout".to_string(),
            Self::CreateGooglePhotoAlbum => "Create Google Photo Album".to_string(),
            Self::AlbumName => format!("Album name: {}", album_name.unwrap_or("(unknown)")),
            Self::Back => "Back".to_string(),
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq)]
enum DialogFocus {
    Input,
    OkButton,
    CancelButton,
}

#[derive(Debug, Clone)]
enum AppState {
    MainMenu,
    ProjectPathInput { input: Input, focus: DialogFocus },
    OpenProjectInput { input: Input, focus: DialogFocus },
    GoogleCredentialsPathInput { input: Input, focus: DialogFocus },
    CreatingProject { path: String },
    OpeningProject { path: String },
    ValidatingGoogleCredentials { credentials_path: String },
    GoogleLoginConfirm { credentials_path: String },
    PerformingGoogleLogin { credentials_path: String },
    Dashboard { path: String, selected_entry: usize },
    AlbumNameInput { input: Input, focus: DialogFocus },
    OperationInProgress { message: String },
    TestingGoogleApiSuccess,
    Error { message: String },
}

#[derive(Debug, Clone, Copy, PartialEq)]
enum MenuItem {
    NewProject,
    OpenProject,
    Exit,
}

impl MenuItem {
    fn all() -> Vec<Self> {
        vec![Self::NewProject, Self::OpenProject, Self::Exit]
    }

    fn label(&self) -> &str {
        match self {
            Self::NewProject => "New Project",
            Self::OpenProject => "Open Project",
            Self::Exit => "Exit",
        }
    }
}

struct App {
    selected_menu: usize,
    should_quit: bool,
    state: AppState,
    current_project_manager: Option<ProjectFileManager>,
    current_project_path: Option<String>,
    logs_expanded: bool,
    logs_focused: bool,
    logs_selected_index: Option<usize>,
    logs: Arc<Mutex<VecDeque<String>>>,
    album_id: Option<String>,
    album_name: Option<String>,
    pending_operation: Option<PendingOperation>,
    album_checked_project: Option<String>,
}

impl App {
    fn new(logs: Arc<Mutex<VecDeque<String>>>) -> Self {
        Self {
            selected_menu: 0,
            should_quit: false,
            state: AppState::MainMenu,
            current_project_manager: None,
            current_project_path: None,
            logs_expanded: false,
            logs_focused: false,
            logs_selected_index: None,
            logs,
            album_id: None,
            album_name: None,
            pending_operation: None,
            album_checked_project: None,
        }
    }

    fn logs_len(&self) -> usize {
        self.logs.lock().map(|logs| logs.len()).unwrap_or(0)
    }

    fn start_operation(&mut self, message: &str, operation: PendingOperation) {
        self.pending_operation = Some(operation);
        self.state = AppState::OperationInProgress {
            message: message.to_string(),
        };
    }

    fn on_event(&mut self, event: Event) {
        let mut queued_operation: Option<(String, PendingOperation)> = None;
        // Handle log panel interactions when focused
        if self.logs_focused {
            if let Event::Key(key) = event {
                match key.code {
                    KeyCode::Char('l') | KeyCode::Esc => {
                        info!("Closing logs panel");
                        self.logs_expanded = false;
                        self.logs_focused = false;
                        self.logs_selected_index = None;
                        return;
                    }
                    KeyCode::Up | KeyCode::Char('k') => {
                        if let Some(selected_index) = self.logs_selected_index {
                            if selected_index > 0 {
                                self.logs_selected_index = Some(selected_index - 1);
                            }
                        }
                        return;
                    }
                    KeyCode::Down | KeyCode::Char('j') => {
                        let logs_len = self.logs_len();
                        if let Some(selected_index) = self.logs_selected_index {
                            if logs_len > 0 && selected_index + 1 < logs_len {
                                self.logs_selected_index = Some(selected_index + 1);
                            }
                        }
                        return;
                    }
                    KeyCode::PageUp => {
                        if let Some(selected_index) = self.logs_selected_index {
                            self.logs_selected_index = Some(selected_index.saturating_sub(10));
                        }
                        return;
                    }
                    KeyCode::PageDown => {
                        let logs_len = self.logs_len();
                        if logs_len > 0 {
                            if let Some(selected_index) = self.logs_selected_index {
                                self.logs_selected_index =
                                    Some(std::cmp::min(selected_index + 10, logs_len - 1));
                            }
                        }
                        return;
                    }
                    KeyCode::Char('c') => {
                        // Copy selected log entry to clipboard
                        info!("Copying log entry to clipboard");
                        if let Some(selected_index) = self.logs_selected_index
                            && let Ok(mut clipboard) = arboard::Clipboard::new()
                        {
                            let selected_log = self
                                .logs
                                .lock()
                                .ok()
                                .and_then(|logs| logs.get(selected_index).cloned())
                                .unwrap_or_else(|| "No log selected".to_string());
                            if let Err(e) = clipboard.set_text(selected_log) {
                                error!("Failed to copy to clipboard: {}", e);
                            }
                        } else {
                            error!("Failed to access clipboard");
                        }
                        return;
                    }
                    _ => return,
                }
            }
            return;
        }

        let next_state = match &mut self.state {
            AppState::MainMenu => match event {
                Event::Key(key) => match key.code {
                    KeyCode::Char('q') | KeyCode::Esc => {
                        info!("Quit requested by user");
                        self.should_quit = true;
                        None
                    }
                    KeyCode::Up | KeyCode::Char('k') => {
                        if self.selected_menu > 0 {
                            self.selected_menu -= 1;
                            debug!("Menu selection moved up to: {}", self.selected_menu);
                        }
                        None
                    }
                    KeyCode::Down | KeyCode::Char('j') => {
                        let menu_items = MenuItem::all();
                        if self.selected_menu < menu_items.len() - 1 {
                            self.selected_menu += 1;
                            debug!("Menu selection moved down to: {}", self.selected_menu);
                        }
                        None
                    }
                    KeyCode::Enter => {
                        let menu_items = MenuItem::all();
                        if let Some(item) = menu_items.get(self.selected_menu) {
                            info!("Selected menu item: {:?}", item);
                            self.handle_menu_selection(*item);
                        }
                        None
                    }
                    KeyCode::Char('l') => {
                        info!("Toggling logs view");
                        if !self.logs_expanded {
                            // Expanding logs - give focus and select latest log entry
                            self.logs_expanded = true;
                            self.logs_focused = true;
                            let logs_len = self.logs_len();
                            self.logs_selected_index = Some(logs_len.saturating_sub(1));
                        } else {
                            // Collapsing logs - remove focus
                            self.logs_expanded = false;
                            self.logs_focused = false;
                            self.logs_selected_index = None;
                        }
                        None
                    }
                    _ => None,
                },
                _ => None,
            },
            AppState::ProjectPathInput { input, focus, .. } => {
                match event {
                    Event::Key(key) => match key.code {
                        KeyCode::Esc => {
                            info!("Project creation cancelled");
                            Some(AppState::MainMenu)
                        }
                        KeyCode::Tab | KeyCode::Right => {
                            // Move focus to next element
                            *focus = match focus {
                                DialogFocus::Input => DialogFocus::OkButton,
                                DialogFocus::OkButton => DialogFocus::CancelButton,
                                DialogFocus::CancelButton => DialogFocus::Input,
                            };
                            debug!("Dialog focus changed to: {:?}", focus);
                            None
                        }
                        KeyCode::BackTab | KeyCode::Left => {
                            // Move focus to previous element
                            *focus = match focus {
                                DialogFocus::Input => DialogFocus::CancelButton,
                                DialogFocus::OkButton => DialogFocus::Input,
                                DialogFocus::CancelButton => DialogFocus::OkButton,
                            };
                            debug!("Dialog focus changed to: {:?}", focus);
                            None
                        }
                        KeyCode::Enter => match focus {
                            DialogFocus::Input => {
                                // Move to Ok button on Enter from input
                                *focus = DialogFocus::OkButton;
                                None
                            }
                            DialogFocus::OkButton => {
                                // Confirm project creation
                                let path = input.value().trim();
                                if !path.is_empty() {
                                    info!("Creating project at: {}", path);
                                    Some(AppState::CreatingProject {
                                        path: path.to_string(),
                                    })
                                } else {
                                    None
                                }
                            }
                            DialogFocus::CancelButton => {
                                // Cancel
                                info!("Project creation cancelled");
                                Some(AppState::MainMenu)
                            }
                        },
                        _ => {
                            if *focus == DialogFocus::Input {
                                input.handle_event(&event);
                            }
                            None
                        }
                    },
                    _ => None,
                }
            }
            AppState::OpenProjectInput { input, focus, .. } => {
                match event {
                    Event::Key(key) => match key.code {
                        KeyCode::Esc => {
                            info!("Open project cancelled");
                            Some(AppState::MainMenu)
                        }
                        KeyCode::Tab | KeyCode::Right => {
                            // Move focus to next element
                            *focus = match focus {
                                DialogFocus::Input => DialogFocus::OkButton,
                                DialogFocus::OkButton => DialogFocus::CancelButton,
                                DialogFocus::CancelButton => DialogFocus::Input,
                            };
                            debug!("Dialog focus changed to: {:?}", focus);
                            None
                        }
                        KeyCode::BackTab | KeyCode::Left => {
                            // Move focus to previous element
                            *focus = match focus {
                                DialogFocus::Input => DialogFocus::CancelButton,
                                DialogFocus::OkButton => DialogFocus::Input,
                                DialogFocus::CancelButton => DialogFocus::OkButton,
                            };
                            debug!("Dialog focus changed to: {:?}", focus);
                            None
                        }
                        KeyCode::Enter => match focus {
                            DialogFocus::Input => {
                                // Move to Ok button on Enter from input
                                *focus = DialogFocus::OkButton;
                                None
                            }
                            DialogFocus::OkButton => {
                                // Confirm project opening
                                let path = input.value().trim();
                                if !path.is_empty() {
                                    info!("Opening project at: {}", path);
                                    Some(AppState::OpeningProject {
                                        path: path.to_string(),
                                    })
                                } else {
                                    None
                                }
                            }
                            DialogFocus::CancelButton => {
                                // Cancel
                                info!("Open project cancelled");
                                Some(AppState::MainMenu)
                            }
                        },
                        _ => {
                            if *focus == DialogFocus::Input {
                                input.handle_event(&event);
                            }
                            None
                        }
                    },
                    _ => None,
                }
            }
            AppState::GoogleCredentialsPathInput { input, focus, .. } => match event {
                Event::Key(key) => match key.code {
                    KeyCode::Esc => {
                        info!("Google login cancelled");
                        let path = self.current_project_path.clone().unwrap_or_default();
                        Some(AppState::Dashboard {
                            path,
                            selected_entry: 0,
                        })
                    }
                    KeyCode::Tab | KeyCode::Right => {
                        *focus = match focus {
                            DialogFocus::Input => DialogFocus::OkButton,
                            DialogFocus::OkButton => DialogFocus::CancelButton,
                            DialogFocus::CancelButton => DialogFocus::Input,
                        };
                        debug!("Dialog focus changed to: {:?}", focus);
                        None
                    }
                    KeyCode::BackTab | KeyCode::Left => {
                        *focus = match focus {
                            DialogFocus::Input => DialogFocus::CancelButton,
                            DialogFocus::OkButton => DialogFocus::Input,
                            DialogFocus::CancelButton => DialogFocus::OkButton,
                        };
                        debug!("Dialog focus changed to: {:?}", focus);
                        None
                    }
                    KeyCode::Enter => match focus {
                        DialogFocus::Input => {
                            *focus = DialogFocus::OkButton;
                            None
                        }
                        DialogFocus::OkButton => {
                            let credentials_path = input.value().trim();
                            if !credentials_path.is_empty() {
                                Some(AppState::ValidatingGoogleCredentials {
                                    credentials_path: credentials_path.to_string(),
                                })
                            } else {
                                None
                            }
                        }
                        DialogFocus::CancelButton => {
                            let path = self.current_project_path.clone().unwrap_or_default();
                            Some(AppState::Dashboard {
                                path,
                                selected_entry: 0,
                            })
                        }
                    },
                    _ => {
                        if *focus == DialogFocus::Input {
                            input.handle_event(&event);
                        }
                        None
                    }
                },
                _ => None,
            },
            AppState::GoogleLoginConfirm { credentials_path } => match event {
                Event::Key(key) => match key.code {
                    KeyCode::Esc => {
                        let path = self.current_project_path.clone().unwrap_or_default();
                        Some(AppState::Dashboard {
                            path,
                            selected_entry: 0,
                        })
                    }
                    KeyCode::Enter => Some(AppState::PerformingGoogleLogin {
                        credentials_path: credentials_path.clone(),
                    }),
                    _ => None,
                },
                _ => None,
            },
            AppState::AlbumNameInput { input, focus } => match event {
                Event::Key(key) => match key.code {
                    KeyCode::Esc => {
                        let path = self.current_project_path.clone().unwrap_or_default();
                        Some(AppState::Dashboard {
                            path,
                            selected_entry: 0,
                        })
                    }
                    KeyCode::Tab | KeyCode::Right => {
                        *focus = match focus {
                            DialogFocus::Input => DialogFocus::OkButton,
                            DialogFocus::OkButton => DialogFocus::CancelButton,
                            DialogFocus::CancelButton => DialogFocus::Input,
                        };
                        debug!("Dialog focus changed to: {:?}", focus);
                        None
                    }
                    KeyCode::BackTab | KeyCode::Left => {
                        *focus = match focus {
                            DialogFocus::Input => DialogFocus::CancelButton,
                            DialogFocus::OkButton => DialogFocus::Input,
                            DialogFocus::CancelButton => DialogFocus::OkButton,
                        };
                        debug!("Dialog focus changed to: {:?}", focus);
                        None
                    }
                    KeyCode::Enter => match focus {
                        DialogFocus::Input => {
                            *focus = DialogFocus::OkButton;
                            None
                        }
                        DialogFocus::OkButton => {
                            let album_name = input.value().trim();
                            if album_name.is_empty() {
                                None
                            } else {
                                queued_operation = Some((
                                    "Operation in progress".to_string(),
                                    PendingOperation::CreateAlbum {
                                        album_name: album_name.to_string(),
                                    },
                                ));
                                None
                            }
                        }
                        DialogFocus::CancelButton => {
                            let path = self.current_project_path.clone().unwrap_or_default();
                            Some(AppState::Dashboard {
                                path,
                                selected_entry: 0,
                            })
                        }
                    },
                    _ => {
                        if *focus == DialogFocus::Input {
                            input.handle_event(&event);
                        }
                        None
                    }
                },
                _ => None,
            },
            AppState::Error { .. } => match event {
                Event::Key(key) => match key.code {
                    KeyCode::Enter | KeyCode::Esc | KeyCode::Char(' ') => Some(AppState::MainMenu),
                    _ => None,
                },
                _ => None,
            },
            AppState::OperationInProgress { .. } => {
                // Modal dialog - ignore user input while operation is running
                None
            }
            AppState::Dashboard {
                path,
                selected_entry,
            } => match event {
                Event::Key(key) => match key.code {
                    KeyCode::Char('q') => {
                        self.should_quit = true;
                        None
                    }
                    KeyCode::Esc => Some(AppState::MainMenu),
                    KeyCode::Up | KeyCode::Char('k') => {
                        if *selected_entry > 0 {
                            *selected_entry -= 1;
                        }
                        None
                    }
                    KeyCode::Down | KeyCode::Char('j') => {
                        let entries = dashboard_entries_for(
                            path,
                            self.current_project_manager.as_ref(),
                            self.album_name.as_deref(),
                        );
                        if !entries.is_empty() && *selected_entry < entries.len() - 1 {
                            *selected_entry += 1;
                        }
                        None
                    }
                    KeyCode::Char('l') => {
                        info!("Toggling logs view from dashboard");
                        if !self.logs_expanded {
                            // Expanding logs - give focus and select latest log entry
                            self.logs_expanded = true;
                            self.logs_focused = true;
                            let logs_len = self.logs_len();
                            self.logs_selected_index = Some(logs_len.saturating_sub(1));
                        } else {
                            // Collapsing logs - remove focus
                            self.logs_expanded = false;
                            self.logs_focused = false;
                            self.logs_selected_index = None;
                        }
                        None
                    }
                    KeyCode::Enter => {
                        // Set current project path when in Dashboard
                        self.current_project_path = Some(path.clone());

                        let entries = dashboard_entries_for(
                            path,
                            self.current_project_manager.as_ref(),
                            self.album_name.as_deref(),
                        );
                        if entries.is_empty() {
                            None
                        } else {
                            let idx = (*selected_entry).min(entries.len() - 1);
                            match entries[idx] {
                                DashboardEntry::LoginToGoogle => {
                                    if let Some(project_manager) = &self.current_project_manager {
                                        match project_manager.get_google_auth_status() {
                                            Ok(GoogleAuthStatus::MissingCredentials) => {
                                                Some(AppState::GoogleCredentialsPathInput {
                                                    input: Input::default(),
                                                    focus: DialogFocus::Input,
                                                })
                                            }
                                            Ok(GoogleAuthStatus::NeedsLogin {
                                                credentials_path,
                                            }) => Some(AppState::GoogleLoginConfirm {
                                                credentials_path: credentials_path
                                                    .to_string_lossy()
                                                    .to_string(),
                                            }),
                                            Ok(GoogleAuthStatus::LoggedIn { .. }) => None,
                                            Err(err) => Some(AppState::Error {
                                                message: err.to_string(),
                                            }),
                                        }
                                    } else {
                                        Some(AppState::Error {
                                            message: "Project manager not available".to_string(),
                                        })
                                    }
                                }
                                DashboardEntry::TestGoogleApi => {
                                    info!("Testing Google API");
                                    queued_operation = Some((
                                        "Operation in progress".to_string(),
                                        PendingOperation::TestGoogleApi,
                                    ));
                                    None
                                }
                                DashboardEntry::CreateGooglePhotoAlbum => {
                                    Some(AppState::AlbumNameInput {
                                        input: Input::default(),
                                        focus: DialogFocus::Input,
                                    })
                                }
                                DashboardEntry::AlbumName => None,
                                DashboardEntry::LogoutFromGoogle => {
                                    if let Some(project_manager) = &self.current_project_manager {
                                        match project_manager.logout_from_google() {
                                            Ok(_) => {
                                                self.album_id = None;
                                                self.album_name = None;
                                                self.album_checked_project = None;
                                                Some(AppState::Dashboard {
                                                    path: path.clone(),
                                                    selected_entry: 0,
                                                })
                                            }
                                            Err(err) => Some(AppState::Error {
                                                message: err.to_string(),
                                            }),
                                        }
                                    } else {
                                        Some(AppState::Error {
                                            message: "Project manager not available".to_string(),
                                        })
                                    }
                                }
                                DashboardEntry::Back => Some(AppState::MainMenu),
                            }
                        }
                    }
                    _ => None,
                },
                _ => None,
            },
            AppState::CreatingProject { .. } => {
                // Don't accept input while creating project
                None
            }
            AppState::OpeningProject { .. } => {
                // Don't accept input while opening project
                None
            }
            AppState::ValidatingGoogleCredentials { .. } => {
                // Don't accept input while validating credentials
                None
            }
            AppState::PerformingGoogleLogin { .. } => {
                // Don't accept input while performing login
                None
            }
            AppState::TestingGoogleApiSuccess => {
                // Auto-transition back to dashboard, accept any key to force return
                match event {
                    Event::Key(_) => None, // Will be handled by the success state handler
                    _ => None,
                }
            }
        };

        if let Some(new_state) = next_state {
            // Clear project manager when returning to MainMenu
            if matches!(new_state, AppState::MainMenu) {
                self.current_project_manager = None;
            }
            self.state = new_state;
        }

        if let Some((message, operation)) = queued_operation {
            self.start_operation(&message, operation);
        }
    }

    fn handle_menu_selection(&mut self, item: MenuItem) {
        match item {
            MenuItem::Exit => {
                info!("Exit selected from main menu");
                self.should_quit = true;
            }
            MenuItem::NewProject => {
                info!("Opening project path input dialog...");
                self.state = AppState::ProjectPathInput {
                    input: Input::default(),
                    focus: DialogFocus::Input,
                };
            }
            MenuItem::OpenProject => {
                info!("Opening existing project...");
                self.state = AppState::OpenProjectInput {
                    input: Input::default(),
                    focus: DialogFocus::Input,
                };
            }
        }
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Setup tracing with tui-logger
    tui_logger::init_logger(log::LevelFilter::Debug)?;
    tui_logger::set_default_level(log::LevelFilter::Debug);

    let app_logs = Arc::new(Mutex::new(VecDeque::new()));

    // Setup tracing subscriber
    tracing_subscriber::registry()
        .with(tui_logger::TuiTracingSubscriberLayer)
        .with(AppLogCaptureLayer::new(app_logs.clone(), MAX_LOG_ENTRIES))
        .init();

    info!("Starting Photo Frame Manager");
    debug!("Initializing terminal...");

    // Setup terminal
    enable_raw_mode()?;
    let mut stdout = io::stdout();
    execute!(stdout, EnterAlternateScreen, EnableMouseCapture)?;
    let backend = CrosstermBackend::new(stdout);
    let mut terminal = Terminal::new(backend)?;

    // Create app
    let mut app = App::new(app_logs);
    info!("Application initialized successfully");

    // Run app
    let res = run_app(&mut terminal, &mut app);

    // Restore terminal
    disable_raw_mode()?;
    execute!(
        terminal.backend_mut(),
        LeaveAlternateScreen,
        DisableMouseCapture
    )?;
    terminal.show_cursor()?;

    if let Err(err) = res {
        eprintln!("Error: {:?}", err);
    }

    info!("Application shutting down");
    Ok(())
}

fn run_app<B: ratatui::backend::Backend + io::Write>(
    terminal: &mut Terminal<B>,
    app: &mut App,
) -> Result<(), Box<dyn std::error::Error>>
where
    <B as ratatui::backend::Backend>::Error: 'static,
{
    loop {
        // Handle project creation
        if let AppState::CreatingProject { path } = &app.state {
            let path = path.clone();
            match ProjectFileManager::new(&path) {
                Ok(project_manager) => {
                    info!("Project created successfully at: {}", path);
                    app.current_project_manager = Some(project_manager);
                    app.current_project_path = Some(path.clone());
                    app.album_id = None;
                    app.album_name = None;
                    app.album_checked_project = None;
                    app.state = AppState::Dashboard {
                        path,
                        selected_entry: 0,
                    };
                }
                Err(err) => {
                    error!("Failed to create project: {}", err);
                    app.state = AppState::Error {
                        message: err.to_string(),
                    };
                }
            }
        }

        // Handle project opening
        if let AppState::OpeningProject { path } = &app.state {
            let path = path.clone();
            match ProjectFileManager::open(&path) {
                Ok(project_manager) => {
                    info!("Project opened successfully at: {}", path);
                    app.current_project_manager = Some(project_manager);
                    app.current_project_path = Some(path.clone());
                    app.album_id = None;
                    app.album_name = None;
                    app.album_checked_project = None;
                    app.state = AppState::Dashboard {
                        path,
                        selected_entry: 0,
                    };
                }
                Err(err) => {
                    error!("Failed to open project: {}", err);
                    app.state = AppState::Error {
                        message: err.to_string(),
                    };
                }
            }
        }

        // Handle Google credentials validation
        if let AppState::ValidatingGoogleCredentials { credentials_path } = &app.state {
            let credentials_path = credentials_path.clone();
            match GoogleAuthFileManager::validate_google_credentials_file(Path::new(
                &credentials_path,
            )) {
                Ok(_credentials) => {
                    info!("Google credentials validated successfully");
                    app.state = AppState::GoogleLoginConfirm { credentials_path };
                }
                Err(err) => {
                    error!("Google credentials validation failed: {}", err);
                    app.state = AppState::Error {
                        message: err.to_string(),
                    };
                }
            }
        }

        if let AppState::PerformingGoogleLogin { credentials_path } = &app.state {
            let project_manager = match &app.current_project_manager {
                Some(pm) => pm,
                None => {
                    app.state = AppState::Error {
                        message: "No project loaded".to_string(),
                    };
                    continue;
                }
            };
            let credentials_path = credentials_path.clone();
            let project_path = app.current_project_path.clone().unwrap_or_default();

            match GoogleAuthFileManager::perform_google_oauth_login(
                project_manager.get_token_path().as_path(),
                Path::new(&credentials_path),
            ) {
                Ok(_token_path) => {
                    let _credentials = match GoogleAuthFileManager::validate_google_credentials_file(
                        Path::new(&credentials_path),
                    ) {
                        Ok(c) => c,
                        Err(err) => {
                            app.state = AppState::Error {
                                message: err.to_string(),
                            };
                            continue;
                        }
                    };

                    // Copy credentials file to project
                    let _project_credentials_path = match project_manager
                        .copy_credentials_to_project(Path::new(&credentials_path))
                    {
                        Ok(_) => {
                            info!("Google login completed successfully");
                            if let Ok(pm) = ProjectFileManager::open(&project_path) {
                                app.current_project_manager = Some(pm);
                                app.current_project_path = Some(project_path.clone());
                            }
                            app.album_id = None;
                            app.album_name = None;
                            app.album_checked_project = None;
                            app.state = AppState::Dashboard {
                                path: project_path,
                                selected_entry: 0,
                            };
                        }
                        Err(err) => {
                            error!("Failed to copy credentials file: {}", err);
                            app.state = AppState::Error {
                                message: err.to_string(),
                            };
                            continue;
                        }
                    };
                }

                Err(err) => {
                    error!("Google OAuth login failed: {}", err);
                    app.state = AppState::Error {
                        message: err.to_string(),
                    };
                }
            }
        }

        // Handle Google API testing success - auto return to dashboard after display
        if let AppState::TestingGoogleApiSuccess = &app.state {
            let project_path = app.current_project_path.clone().unwrap_or_default();
            // Auto transition back to dashboard after showing success briefly
            app.state = AppState::Dashboard {
                path: project_path,
                selected_entry: 1, // Stay on TestGoogleApi entry
            };
            continue;
        }

        // Auto-check album when entering dashboard
        if let AppState::Dashboard { path, .. } = &app.state {
            let should_check = app
                .album_checked_project
                .as_deref()
                .map(|p| p != path)
                .unwrap_or(true);

            if should_check && app.pending_operation.is_none() {
                if let Some(pm) = &app.current_project_manager {
                    let is_logged_in = matches!(
                        pm.get_google_auth_status(),
                        Ok(GoogleAuthStatus::LoggedIn { .. })
                    );
                    if is_logged_in {
                        app.start_operation("Operation in progress", PendingOperation::CheckAlbum);
                    } else {
                        app.album_id = None;
                        app.album_name = None;
                        app.album_checked_project = Some(path.clone());
                    }
                }
            }
        }

        terminal.draw(|f| ui(f, app))?;

        // Execute pending operations after rendering the progress dialog
        if let AppState::OperationInProgress { .. } = &app.state {
            if let Some(operation) = app.pending_operation.take() {
                if let Some(project_manager) = app.current_project_manager.as_ref() {
                    let project_path = app.current_project_path.clone().unwrap_or_default();

                    match operation {
                        PendingOperation::TestGoogleApi => {
                            let access_token = match get_valid_access_token(&project_manager) {
                                Ok(token) => token,
                                Err(err) => {
                                    app.state = AppState::Error {
                                        message: format!("Failed to read token: {}", err),
                                    };
                                    continue;
                                }
                            };

                            let client = Client::new();
                            let result = client
                                .get("https://photoslibrary.googleapis.com/v1/albums")
                                .header("Authorization", format!("Bearer {}", access_token))
                                .query(&[("pageSize", "1")])
                                .send();

                            match result {
                                Ok(response) => {
                                    if response.status().is_success() {
                                        app.state = AppState::TestingGoogleApiSuccess;
                                    } else {
                                        app.state = AppState::Error {
                                            message: format!(
                                                "Google API test failed: {}",
                                                response.status()
                                            ),
                                        };
                                    }
                                }
                                Err(err) => {
                                    app.state = AppState::Error {
                                        message: format!("API request failed: {}", err),
                                    };
                                }
                            }
                        }
                        PendingOperation::CheckAlbum => {
                            let access_token = match get_valid_access_token(&project_manager) {
                                Ok(token) => token,
                                Err(err) => {
                                    app.state = AppState::Error {
                                        message: format!("Failed to read token: {}", err),
                                    };
                                    continue;
                                }
                            };

                            let album_id = match project_manager
                                .get_property(ProjectKeys::GooglePhotosAlbumId)
                            {
                                Ok(Some(value)) => value,
                                Ok(None) => {
                                    app.album_id = None;
                                    app.album_name = None;
                                    app.album_checked_project = Some(project_path.clone());
                                    app.state = AppState::Dashboard {
                                        path: project_path,
                                        selected_entry: 0,
                                    };
                                    continue;
                                }
                                Err(err) => {
                                    app.state = AppState::Error {
                                        message: format!("Failed to read album id: {}", err),
                                    };
                                    continue;
                                }
                            };

                            match fetch_album(&access_token, &album_id) {
                                Ok(Some(album)) => {
                                    let title =
                                        album.title.unwrap_or_else(|| "(unknown)".to_string());
                                    let _ = project_manager
                                        .set_property(ProjectKeys::GooglePhotosAlbumName, &title);
                                    app.album_id = album.id.or(Some(album_id));
                                    app.album_name = Some(title);
                                    app.album_checked_project = Some(project_path.clone());
                                    app.state = AppState::Dashboard {
                                        path: project_path,
                                        selected_entry: 0,
                                    };
                                }
                                Ok(None) => {
                                    let _ = project_manager.remove_properties(&[
                                        ProjectKeys::GooglePhotosAlbumId,
                                        ProjectKeys::GooglePhotosAlbumName,
                                    ]);
                                    app.album_id = None;
                                    app.album_name = None;
                                    app.album_checked_project = Some(project_path.clone());
                                    app.state = AppState::Dashboard {
                                        path: project_path,
                                        selected_entry: 0,
                                    };
                                }
                                Err(err) => {
                                    app.state = AppState::Error {
                                        message: format!("Failed to fetch album: {}", err),
                                    };
                                }
                            }
                        }
                        PendingOperation::CreateAlbum { album_name } => {
                            let access_token = match get_valid_access_token(&project_manager) {
                                Ok(token) => token,
                                Err(err) => {
                                    app.state = AppState::Error {
                                        message: format!("Failed to read token: {}", err),
                                    };
                                    continue;
                                }
                            };

                            match create_album(&access_token, &album_name) {
                                Ok((album_id, title)) => {
                                    let _ = project_manager
                                        .set_property(ProjectKeys::GooglePhotosAlbumId, &album_id);
                                    let _ = project_manager
                                        .set_property(ProjectKeys::GooglePhotosAlbumName, &title);
                                    app.album_id = Some(album_id);
                                    app.album_name = Some(title);
                                    app.album_checked_project = Some(project_path.clone());
                                    app.state = AppState::Dashboard {
                                        path: project_path,
                                        selected_entry: 0,
                                    };
                                }
                                Err(err) => {
                                    app.state = AppState::Error {
                                        message: format!("Failed to create album: {}", err),
                                    };
                                }
                            }
                        }
                    }
                } else {
                    app.state = AppState::Error {
                        message: "No project loaded".into(),
                    };
                }
            }
        }

        if event::poll(std::time::Duration::from_millis(100))? {
            let evt = event::read()?;
            if let Event::Key(key) = &evt {
                if key.kind != KeyEventKind::Release {
                    app.on_event(evt);
                }
            }
        }

        if app.should_quit {
            break;
        }
    }

    Ok(())
}

fn get_valid_access_token(
    project_manager: &ProjectFileManager,
) -> Result<String, Box<dyn std::error::Error>> {
    project_manager
        .get_valid_access_token()
        .or_else(|_| Err("No token configured - please login first".into()))
}

fn fetch_album(
    access_token: &str,
    album_id: &str,
) -> Result<Option<GoogleAlbumResponse>, Box<dyn std::error::Error>> {
    let client = Client::new();
    let response = client
        .get(format!(
            "https://photoslibrary.googleapis.com/v1/albums/{}",
            album_id
        ))
        .header("Authorization", format!("Bearer {}", access_token))
        .send()?;

    if response.status().is_success() {
        let album: GoogleAlbumResponse = response.json()?;
        Ok(Some(album))
    } else if response.status().as_u16() == 404 {
        Ok(None)
    } else {
        Err(format!("Album lookup failed: {}", response.status()).into())
    }
}

fn create_album(
    access_token: &str,
    album_name: &str,
) -> Result<(String, String), Box<dyn std::error::Error>> {
    let client = Client::new();
    let payload = CreateAlbumRequest {
        album: CreateAlbumPayload {
            title: album_name.to_string(),
        },
    };

    let response = client
        .post("https://photoslibrary.googleapis.com/v1/albums")
        .header("Authorization", format!("Bearer {}", access_token))
        .json(&payload)
        .send()?;

    if response.status().is_success() {
        let album: GoogleAlbumResponse = response.json()?;
        let album_id = album
            .id
            .ok_or("Album creation succeeded but id was missing")?;
        let title = album.title.unwrap_or_else(|| album_name.to_string());
        Ok((album_id, title))
    } else {
        Err(format!("Album creation failed: {}", response.status()).into())
    }
}

fn ui(f: &mut Frame, app: &App) {
    let size = f.area();

    // Determine logs panel height based on expansion state
    let logs_height = if app.logs_expanded {
        (size.height * 60) / 100 // 60% when expanded
    } else {
        8 // 8 lines when collapsed
    };

    let (logs_height, content_height, separator_height) =
        if logs_height > size.height.saturating_sub(5) {
            // Logs would be too big, limit it
            let max_logs = size.height.saturating_sub(5);
            (max_logs, size.height.saturating_sub(max_logs + 1), 1)
        } else {
            (logs_height, size.height.saturating_sub(logs_height + 1), 1)
        };

    // Split vertical layout: content area + separator + logs area
    let layout = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(content_height),
            Constraint::Length(separator_height),
            Constraint::Length(logs_height),
        ])
        .split(size);

    let content_area = layout[0];
    let logs_area = layout[2];

    // Render content based on current state
    match &app.state {
        AppState::ProjectPathInput { .. } => {
            render_main_menu(f, app, content_area);
        }
        AppState::Dashboard {
            path,
            selected_entry,
        } => {
            render_dashboard(
                f,
                content_area,
                path,
                *selected_entry,
                app.current_project_manager.as_ref(),
                app.album_name.as_deref(),
            );
        }
        _ => {
            render_main_menu(f, app, content_area);
        }
    }

    // Render separator
    let separator = Block::default().borders(Borders::TOP);
    f.render_widget(separator, layout[1]);

    // Always render logs at the bottom
    render_logs_panel(
        f,
        logs_area,
        app.logs_expanded,
        app.logs_focused,
        app.logs_selected_index,
        &app.logs,
    );

    // Render dialogs on top if needed
    match &app.state {
        AppState::ProjectPathInput { input, focus, .. } => {
            let dialog_area = centered_rect_fixed_height(60, 9, size);
            render_project_input_dialog(
                f,
                dialog_area,
                "Create New Project",
                "Project folder path:",
                input,
                *focus,
            );
        }
        AppState::OpenProjectInput { input, focus, .. } => {
            let dialog_area = centered_rect_fixed_height(60, 9, size);
            render_project_input_dialog(
                f,
                dialog_area,
                "Open Project",
                "Project folder path:",
                input,
                *focus,
            );
        }
        AppState::GoogleCredentialsPathInput { input, focus, .. } => {
            let dialog_area = centered_rect_fixed_height(70, 9, size);
            render_project_input_dialog(
                f,
                dialog_area,
                "Login to Google",
                "credentials.json path:",
                input,
                *focus,
            );
        }
        AppState::GoogleLoginConfirm { .. } => {
            let dialog_area = centered_rect_fixed_height(70, 9, size);
            render_google_login_confirm_dialog(f, dialog_area);
        }
        AppState::AlbumNameInput { input, focus, .. } => {
            let dialog_area = centered_rect_fixed_height(70, 9, size);
            render_project_input_dialog(
                f,
                dialog_area,
                "Create Google Photo Album",
                "Album name:",
                input,
                *focus,
            );
        }
        AppState::OperationInProgress { message } => {
            let dialog_area = centered_rect_fixed_height(50, 5, size);
            let progress_text = Paragraph::new(message.as_str())
                .style(
                    Style::default()
                        .fg(Color::Black)
                        .bg(Color::Yellow)
                        .add_modifier(Modifier::BOLD),
                )
                .block(
                    Block::default()
                        .borders(Borders::ALL)
                        .title("Hang tight...")
                        .border_style(Style::default().bg(Color::Yellow).fg(Color::Black))
                        .padding(Padding::uniform(1)),
                )
                .alignment(Alignment::Center);
            f.render_widget(Clear, dialog_area);
            f.render_widget(progress_text, dialog_area);
        }
        AppState::TestingGoogleApiSuccess { .. } => {
            let dialog_area = centered_rect_fixed_height(50, 5, size);
            let success_text = Paragraph::new("✓ Google API test successful!")
                .style(
                    Style::default()
                        .fg(Color::Green)
                        .add_modifier(Modifier::BOLD),
                )
                .block(
                    Block::default()
                        .borders(Borders::ALL)
                        .title("Success")
                        .border_style(Style::default().fg(Color::Green)),
                )
                .alignment(Alignment::Center);
            f.render_widget(Clear, dialog_area);
            f.render_widget(success_text, dialog_area);
        }
        AppState::Error { message } => {
            let dialog_area = centered_rect_fixed_height(60, 7, size);
            render_error_dialog(f, dialog_area, message);
        }
        _ => {}
    }
}

fn render_dashboard(
    f: &mut Frame,
    area: Rect,
    path: &str,
    selected_entry: usize,
    project_manager: Option<&ProjectFileManager>,
    album_name: Option<&str>,
) {
    let block = Block::default()
        .borders(Borders::ALL)
        .title("Dashboard")
        .title_style(
            Style::default()
                .fg(Color::Cyan)
                .add_modifier(Modifier::BOLD),
        );

    let inner = block.inner(area);
    f.render_widget(block, area);

    let layout_chunks = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(2), // Title
            Constraint::Length(1), // Path
            Constraint::Min(0),    // Menu entries (use remaining space)
        ])
        .split(inner);

    let title = Paragraph::new("Project loaded").style(
        Style::default()
            .fg(Color::Green)
            .add_modifier(Modifier::BOLD),
    );
    f.render_widget(title, layout_chunks[0]);

    let path_line =
        Paragraph::new(format!("Path: {}", path)).style(Style::default().fg(Color::White));
    f.render_widget(path_line, layout_chunks[1]);

    let entries = dashboard_entries_for(path, project_manager, album_name);
    let dashboard_entries: Vec<ListItem> = entries
        .iter()
        .enumerate()
        .map(|(idx, entry)| {
            let is_selected = idx == selected_entry;
            let marker = if is_selected { "▶ " } else { "  " };
            let style = if is_selected {
                Style::default()
                    .fg(Color::Yellow)
                    .add_modifier(Modifier::BOLD)
            } else {
                Style::default().fg(Color::White)
            };
            ListItem::new(Line::from(vec![
                Span::raw(marker),
                Span::styled(entry.label(album_name), style),
            ]))
        })
        .collect();

    let entries_widget = List::new(dashboard_entries).block(Block::default());
    f.render_widget(entries_widget, layout_chunks[2]);
}

fn dashboard_entries_for(
    _path: &str,
    project_manager: Option<&ProjectFileManager>,
    album_name: Option<&str>,
) -> Vec<DashboardEntry> {
    if let Some(pm) = project_manager {
        match pm.get_google_auth_status() {
            Ok(GoogleAuthStatus::MissingCredentials) => {
                vec![DashboardEntry::LoginToGoogle, DashboardEntry::Back]
            }
            Ok(GoogleAuthStatus::NeedsLogin { .. }) => {
                vec![DashboardEntry::LoginToGoogle, DashboardEntry::Back]
            }
            Ok(GoogleAuthStatus::LoggedIn { .. }) => {
                if album_name.is_some() {
                    vec![
                        DashboardEntry::AlbumName,
                        DashboardEntry::TestGoogleApi,
                        DashboardEntry::LogoutFromGoogle,
                        DashboardEntry::Back,
                    ]
                } else {
                    vec![
                        DashboardEntry::CreateGooglePhotoAlbum,
                        DashboardEntry::TestGoogleApi,
                        DashboardEntry::LogoutFromGoogle,
                        DashboardEntry::Back,
                    ]
                }
            }
            Err(_) => vec![DashboardEntry::LoginToGoogle, DashboardEntry::Back],
        }
    } else {
        vec![DashboardEntry::LoginToGoogle, DashboardEntry::Back]
    }
}

fn render_main_menu(f: &mut Frame, app: &App, area: Rect) {
    let menu_items = MenuItem::all();

    let items: Vec<ListItem> = menu_items
        .iter()
        .enumerate()
        .map(|(i, item)| {
            let content = if i == app.selected_menu {
                Line::from(vec![
                    Span::raw("▶ "),
                    Span::styled(
                        item.label(),
                        Style::default()
                            .fg(Color::Yellow)
                            .add_modifier(Modifier::BOLD),
                    ),
                ])
            } else {
                Line::from(vec![
                    Span::raw("  "),
                    Span::styled(item.label(), Style::default().fg(Color::White)),
                ])
            };
            ListItem::new(content)
        })
        .collect();

    let list = List::new(items)
        .block(
            Block::default()
                .borders(Borders::ALL)
                .title("Photo Frame Manager - Main Menu")
                .title_style(
                    Style::default()
                        .fg(Color::Cyan)
                        .add_modifier(Modifier::BOLD),
                ),
        )
        .highlight_style(Style::default().fg(Color::Yellow));

    f.render_widget(list, area);

    // Render help text
    let help_chunks = Layout::default()
        .direction(Direction::Vertical)
        .constraints([Constraint::Min(0), Constraint::Length(3)])
        .split(area);

    let help_text = Paragraph::new("↑/k: Up  ↓/j: Down  Enter: Select  q/Esc: Quit")
        .style(Style::default().fg(Color::DarkGray))
        .block(Block::default().borders(Borders::TOP));

    f.render_widget(help_text, help_chunks[1]);
}

fn render_logs_panel(
    f: &mut Frame,
    area: Rect,
    expanded: bool,
    focused: bool,
    selected_index: Option<usize>,
    logs: &Arc<Mutex<VecDeque<String>>>,
) {
    // Add hint about expansion state
    let title = if expanded {
        if focused {
            "Logs [FOCUSED] (↑↓/jk: navigate, PgUp/PgDn: page, 'c': copy, 'l'/Esc: close)"
                .to_string()
        } else {
            "Logs (Press 'l' to focus, or press 'l' again to collapse)".to_string()
        }
    } else {
        "Logs (Press 'l' to expand)".to_string()
    };

    let block = if focused {
        Block::default()
            .borders(Borders::ALL)
            .title(title)
            .title_style(
                Style::default()
                    .fg(Color::Green)
                    .add_modifier(Modifier::BOLD),
            )
            .border_style(Style::default().fg(Color::Green))
    } else {
        Block::default()
            .borders(Borders::ALL)
            .title(title)
            .title_style(Style::default().fg(Color::Cyan))
    };

    let inner_area = block.inner(area);
    f.render_widget(block, area);

    let logs_snapshot: Vec<String> = logs
        .lock()
        .map(|l| l.iter().cloned().collect())
        .unwrap_or_default();

    if logs_snapshot.is_empty() {
        let empty = Paragraph::new("No logs yet")
            .style(Style::default().fg(Color::DarkGray))
            .alignment(Alignment::Left);
        f.render_widget(empty, inner_area);
        return;
    }

    let safe_selected = if let Some(idx) = selected_index {
        Some(std::cmp::min(idx, logs_snapshot.len().saturating_sub(1)))
    } else {
        None
    };

    let visible_rows = inner_area.height as usize;
    let start = if visible_rows == 0 {
        0
    } else if let Some(idx) = safe_selected {
        idx.saturating_sub(visible_rows.saturating_sub(1))
    } else {
        logs_snapshot.len().saturating_sub(visible_rows)
    };

    let end = std::cmp::min(start + visible_rows, logs_snapshot.len());

    let items: Vec<ListItem> = logs_snapshot[start..end]
        .iter()
        .enumerate()
        .map(|(index, entry)| {
            let absolute_index = start + index;
            if let Some(idx) = safe_selected {
                if absolute_index == idx {
                    ListItem::new(format!("> {}", entry))
                } else {
                    ListItem::new(format!("  {}", entry))
                }
            } else {
                ListItem::new(format!("  {}", entry))
            }
        })
        .collect();

    let mut state = ListState::default();

    if let Some(idx) = safe_selected {
        state.select(Some(idx.saturating_sub(start)));
    }

    let list = List::new(items).highlight_style(
        Style::default()
            .bg(Color::Yellow)
            .fg(Color::Black)
            .add_modifier(Modifier::BOLD),
    );

    f.render_stateful_widget(list, inner_area, &mut state);
}

/// Calculates a centered rectangle area with fixed height
fn centered_rect_fixed_height(percent_x: u16, height: u16, r: Rect) -> Rect {
    let vertical = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length((r.height.saturating_sub(height)) / 2),
            Constraint::Length(height),
            Constraint::Min(0),
        ])
        .split(r);

    Layout::default()
        .direction(Direction::Horizontal)
        .constraints([
            Constraint::Percentage((100 - percent_x) / 2),
            Constraint::Percentage(percent_x),
            Constraint::Percentage((100 - percent_x) / 2),
        ])
        .split(vertical[1])[1]
}

/// Renders the project path input dialog
fn render_project_input_dialog(
    f: &mut Frame,
    area: Rect,
    title: &str,
    label: &str,
    input: &Input,
    focus: DialogFocus,
) {
    f.render_widget(Clear, area);

    // Create dialog block
    let dialog_block = Block::default()
        .borders(Borders::ALL)
        .title(title)
        .title_style(
            Style::default()
                .fg(Color::Cyan)
                .add_modifier(Modifier::BOLD),
        )
        .style(Style::default().bg(Color::Black));

    let inner = dialog_block.inner(area);
    f.render_widget(dialog_block, area);

    // Create layout for dialog contents
    let layout_chunks = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(1), // Label
            Constraint::Length(3), // Input field
            Constraint::Length(1), // Spacing
            Constraint::Length(1), // Buttons
        ])
        .split(inner);

    // Render label
    let label = Paragraph::new(label);
    f.render_widget(label, layout_chunks[0]);

    // Render input field
    let input_block =
        Block::default()
            .borders(Borders::ALL)
            .style(if focus == DialogFocus::Input {
                Style::default()
                    .bg(Color::DarkGray)
                    .fg(Color::White)
                    .add_modifier(Modifier::BOLD)
            } else {
                Style::default().bg(Color::Black).fg(Color::White)
            });

    let input_area = layout_chunks[1];
    let width = input_area.width.max(3) - 3;
    let scroll = input.visual_scroll(width as usize);
    let input_paragraph = Paragraph::new(input.value())
        .block(input_block)
        .style(Style::default().fg(Color::White))
        .scroll((0, scroll as u16));
    f.render_widget(input_paragraph, input_area);

    if focus == DialogFocus::Input {
        let x = input.visual_cursor().max(scroll) - scroll + 1;
        f.set_cursor_position((input_area.x + x as u16, input_area.y + 1));
    }

    // Render buttons
    let button_layout = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([
            Constraint::Length(10),
            Constraint::Length(2),
            Constraint::Length(10),
        ])
        .split(layout_chunks[3]);

    // Ok button
    let ok_style = if focus == DialogFocus::OkButton {
        Style::default()
            .bg(Color::Green)
            .fg(Color::Black)
            .add_modifier(Modifier::BOLD)
    } else {
        Style::default().fg(Color::Green)
    };
    let ok_button = Paragraph::new("[ Ok ]")
        .style(ok_style)
        .alignment(Alignment::Center);
    f.render_widget(ok_button, button_layout[0]);

    // Cancel button
    let cancel_style = if focus == DialogFocus::CancelButton {
        Style::default()
            .bg(Color::Red)
            .fg(Color::White)
            .add_modifier(Modifier::BOLD)
    } else {
        Style::default().fg(Color::Red)
    };
    let cancel_button = Paragraph::new("[ Cancel ]")
        .style(cancel_style)
        .alignment(Alignment::Center);
    f.render_widget(cancel_button, button_layout[2]);
}

/// Renders an error alert dialog
fn render_error_dialog(f: &mut Frame, area: Rect, message: &str) {
    f.render_widget(Clear, area);

    let dialog_block = Block::default()
        .borders(Borders::ALL)
        .title("Error")
        .title_style(Style::default().fg(Color::Red).add_modifier(Modifier::BOLD))
        .style(Style::default().bg(Color::Black));

    let inner = dialog_block.inner(area);
    f.render_widget(dialog_block, area);

    let layout_chunks = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Min(3),    // Message
            Constraint::Length(1), // Spacing
            Constraint::Length(1), // Button
        ])
        .split(inner);

    let message_paragraph = Paragraph::new(message)
        .style(Style::default().fg(Color::White))
        .wrap(Wrap { trim: true });
    f.render_widget(message_paragraph, layout_chunks[0]);

    let ok_button = Paragraph::new("[ Ok ]")
        .style(
            Style::default()
                .bg(Color::Green)
                .fg(Color::Black)
                .add_modifier(Modifier::BOLD),
        )
        .alignment(Alignment::Center);
    f.render_widget(ok_button, layout_chunks[2]);
}

fn render_google_login_confirm_dialog(f: &mut Frame, area: Rect) {
    f.render_widget(Clear, area);

    let dialog_block = Block::default()
        .borders(Borders::ALL)
        .title("Google OAuth Login")
        .title_style(
            Style::default()
                .fg(Color::Cyan)
                .add_modifier(Modifier::BOLD),
        )
        .style(Style::default().bg(Color::Black));

    let inner = dialog_block.inner(area);
    f.render_widget(dialog_block, area);

    let layout_chunks = Layout::default()
        .direction(Direction::Vertical)
        .constraints([
            Constraint::Length(3),
            Constraint::Length(1),
            Constraint::Length(1),
        ])
        .split(inner);

    let msg = Paragraph::new(
        "A browser window will open for Google OAuth login.\nRequired permissions: read/write access to Google Photos and Google Drive.\nPress the button to continue.",
    )
    .style(Style::default().fg(Color::White))
    .wrap(Wrap { trim: true });
    f.render_widget(msg, layout_chunks[0]);

    let button_layout = Layout::default()
        .direction(Direction::Horizontal)
        .constraints([
            Constraint::Min(10),
            Constraint::Length(19),
            Constraint::Min(10),
        ])
        .split(layout_chunks[2]);

    let button = Paragraph::new("[ Login to Google ]")
        .style(
            Style::default()
                .bg(Color::Green)
                .fg(Color::White)
                .add_modifier(Modifier::BOLD),
        )
        .alignment(Alignment::Center);
    f.render_widget(button, button_layout[1]);
}
