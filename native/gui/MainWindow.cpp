#include "MainWindow.h"

#include <QFrame>
#include <QAbstractButton>
#include <QCloseEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QDebug>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QPixmap>
#include <QPushButton>
#include <QProgressDialog>
#include <QPointer>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTextEdit>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>

#include "AboutPage.h"
#include "DashboardPage.h"
#include "IconTheme.h"
#include "JobHistoryPage.h"
#include "SettingsPage.h"
#include "time/ApplicationTime.h"

namespace nlsi::gui {
namespace {

class UppercaseButtonTextFilter final : public QObject {
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Show || event->type() == QEvent::Paint) {
            if (auto* button = qobject_cast<QAbstractButton*>(watched)) {
                if (button->objectName() == QStringLiteral("sendDemoButton")) {
                    return QObject::eventFilter(watched, event);
                }
                const QString upper = button->text().toUpper();
                if (button->text() != upper) {
                    button->setText(upper);
                }
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

} // namespace

MainWindow::MainWindow(
    const std::wstring& title,
    const std::wstring& version,
    telemetry::TelemetryCore& telemetry_core,
    QWidget* parent)
    : QMainWindow(parent),
      telemetry_core_(telemetry_core) {
    setWindowTitle(QString::fromStdWString(title));
    setWindowIcon(QIcon(QStringLiteral(":/icons/logo.ico")));
    QCoreApplication::instance()->installEventFilter(new UppercaseButtonTextFilter(this));
    setMinimumSize(900, 600);
    resize(900, 600);

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("appRoot"));
    auto* root_layout = new QHBoxLayout(root);
    root_layout->setContentsMargins(0, 0, 0, 0);
    root_layout->setSpacing(0);

    auto* sidebar = new QFrame(root);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(210);
    sidebar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    auto* sidebar_layout = new QVBoxLayout(sidebar);
    sidebar_layout->setContentsMargins(0, 0, 0, 0);
    sidebar_layout->setSpacing(4);

    auto* brand = new QWidget(sidebar);
    auto* brand_layout = new QHBoxLayout(brand);
    brand_layout->setContentsMargins(12, 18, 12, 0);
    brand_layout->setSpacing(10);
    auto* logo = new QLabel(brand);
    logo->setObjectName(QStringLiteral("brandLogo"));
    logo->setPixmap(QPixmap(QStringLiteral(":/images/logo.png"))
        .scaled(44, 44, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setFixedSize(44, 44);
    logo->setAlignment(Qt::AlignCenter);
    brand_layout->addWidget(logo);

    auto* brand_text = new QVBoxLayout();
    brand_text->setContentsMargins(0, 0, 0, 0);
    brand_text->setSpacing(0);
    auto* brand_title = new QLabel(QStringLiteral("NABSKI"), brand);
    brand_title->setObjectName(QStringLiteral("brandTitle"));
    brand_text->addWidget(brand_title);
    auto* product = new QLabel(QStringLiteral("Logistics Solutions Inc."), brand);
    product->setObjectName(QStringLiteral("brandSubtitle"));
    product->setWordWrap(true);
    brand_text->addWidget(product);
    brand_layout->addLayout(brand_text, 1);
    sidebar_layout->addWidget(brand);
    sidebar_layout->addSpacing(18);

    auto* content = new QWidget(root);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* content_layout = new QVBoxLayout(content);
    content_layout->setContentsMargins(20, 16, 20, 10);
    content_layout->setSpacing(12);

    auto* header = new QFrame(content);
    header->setObjectName(QStringLiteral("appHeader"));
    auto* header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(18, 12, 18, 12);
    active_page_title_ = new QLabel(QStringLiteral("Dashboard"), header);
    active_page_title_->setObjectName(QStringLiteral("headerTitle"));
    active_page_title_->setMinimumWidth(0);
    active_page_title_->setWordWrap(false);
    active_page_title_->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
    const QString version_label = QString::fromStdWString(version);
    header_clock_ = new QLabel(header);
    header_clock_->setObjectName(QStringLiteral("headerClock"));
    header_clock_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    header_clock_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    header_clock_->setWordWrap(false);
    header_clock_->setMinimumWidth(0);
    header_clock_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    connection_indicator_ = new QLabel(QStringLiteral("DISCONNECTED"), header);
    connection_indicator_->setObjectName(QStringLiteral("statusIndicator"));
    connection_indicator_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    sync_now_button_ = new QPushButton(QStringLiteral("SYNC NOW"), header);
    sync_now_button_->setObjectName(QStringLiteral("syncNowButton"));
    sync_now_button_->setToolTip(
        QStringLiteral("Upload locally queued records to the configured authenticated API."));
    connect(sync_now_button_, &QPushButton::clicked, this, [this] {
        telemetry_core_.SyncNow();
    });
    enter_driver_key_button_ = new QPushButton(QStringLiteral("ENTER YOUR KEY"), header);
    enter_driver_key_button_->setObjectName(QStringLiteral("enterDriverKeyButton"));
    enter_driver_key_button_->setToolTip(
        QStringLiteral("Optionally validate a driver key with the NLSI website over HTTPS."));
    send_demo_button_ = new QPushButton(QStringLiteral("Send Demo"), header);
    send_demo_button_->setObjectName(QStringLiteral("sendDemoButton"));
    send_demo_button_->setToolTip(
        QStringLiteral("Create a clearly marked demo delivery and queue it through normal synchronization."));
    connect(send_demo_button_, &QPushButton::clicked, this, [this] {
        send_demo_button_->setEnabled(false);
        QString error;
        if (!telemetry_core_.CreateDemoDelivery(&error)) {
            statusBar()->showMessage(
                QStringLiteral("Demo delivery was not saved: %1").arg(error), 10000);
            send_demo_button_->setEnabled(true);
            return;
        }
        statusBar()->showMessage(
            QStringLiteral(
                "Demo delivery saved locally and queued for API sync. The header shows current server status."),
            10000);
        send_demo_button_->setEnabled(true);
    });
    connect(enter_driver_key_button_, &QPushButton::clicked, this, [this] {
        QDialog dialog(this);
        dialog.setWindowTitle(QStringLiteral("Enter your NLSI driver key"));
        dialog.setModal(true);
        auto* layout = new QVBoxLayout(&dialog);
        auto* form = new QFormLayout();
        const QString configured_api_base = telemetry_core_.DriverKeyApiBaseUrl().isEmpty()
            ? telemetry_core_.SyncApiBaseUrl()
            : telemetry_core_.DriverKeyApiBaseUrl();
        auto* website_api = new QLineEdit(configured_api_base, &dialog);
        website_api->setObjectName(QStringLiteral("driverKeyWebsiteApiBaseUrl"));
        website_api->setPlaceholderText(QStringLiteral("https://your-domain.example/nlsi_vtc"));
        auto* driver_key = new QLineEdit(&dialog);
        driver_key->setObjectName(QStringLiteral("driverKeyEntry"));
        driver_key->setEchoMode(QLineEdit::Password);
        driver_key->setMaxLength(12);
        driver_key->setPlaceholderText(QStringLiteral("12 letters or numbers"));
        driver_key->setValidator(new QRegularExpressionValidator(
            QRegularExpression(QStringLiteral("[A-Za-z0-9]{0,12}")), driver_key));
        form->addRow(QStringLiteral("Website HTTPS API base URL"), website_api);
        form->addRow(QStringLiteral("Driver key"), driver_key);
        layout->addLayout(form);

        auto* status = new QLabel(&dialog);
        status->setObjectName(QStringLiteral("driverKeyValidationStatus"));
        status->setWordWrap(true);
        const auto update_status = [this, status] {
            const sync::DriverKeyStatus state = telemetry_core_.DriverKeyState();
            if (state.validation_pending) {
                status->setText(QStringLiteral("Checking the saved driver key…"));
            } else if (state.verified) {
                status->setText(QStringLiteral("Verified for %1 (driver %2).")
                    .arg(state.display_name)
                    .arg(state.driver_id));
            } else if (!state.error.isEmpty()) {
                status->setText(state.error);
            } else if (state.configured) {
                status->setText(QStringLiteral(
                    "A driver key is saved but has not been verified in this session."));
            } else {
                status->setText(QStringLiteral(
                    "No driver key is saved. A key is optional; local telemetry continues normally."));
            }
        };
        update_status();
        layout->addWidget(status);

        auto* note = new QLabel(
            QStringLiteral(
                "The driver key is sent only to the configured HTTPS website APIs and is "
                "protected with Windows DPAPI for this Windows user. Website "
                "outages do not interrupt local delivery history or record storage."),
            &dialog);
        note->setWordWrap(true);
        layout->addWidget(note);

        auto* actions = new QHBoxLayout();
        auto* validate = new QPushButton(QStringLiteral("Validate and Save"), &dialog);
        validate->setObjectName(QStringLiteral("validateDriverKeyButton"));
        auto* clear = new QPushButton(QStringLiteral("Clear Saved Key"), &dialog);
        clear->setObjectName(QStringLiteral("clearDriverKeyButton"));
        QPointer<QDialog> dialog_ref(&dialog);
        actions->addWidget(validate);
        actions->addWidget(clear);
        layout->addLayout(actions);

        auto* close_buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
        connect(close_buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(close_buttons);

        connect(validate, &QPushButton::clicked, &dialog, [this, website_api,
                                                            driver_key,
                                                            status, validate, dialog_ref] {
            const QString submitted_key = driver_key->text();
            const auto save_sync_settings = [this, website_api, submitted_key, status] {
                QString sync_error;
                if (!telemetry_core_.ConfigureSync(
                        website_api->text(), submitted_key, &sync_error)) {
                    status->setText(QStringLiteral("Driver key saved, but telemetry sync was not configured: %1")
                        .arg(sync_error));
                    return false;
                }
                status->setText(QStringLiteral(
                    "Telemetry synchronization settings were saved securely. Upload status is shown in the app header."));
                return true;
            };
            if (driver_key->text().trimmed().isEmpty()) {
                status->setText(QStringLiteral("Enter a driver key to validate and configure telemetry synchronization."));
                return;
            }
            validate->setEnabled(false);
            status->setText(QStringLiteral("Validating with the NLSI website…"));
            QPointer<QLineEdit> key_ref(driver_key);
            QPointer<QLabel> status_ref(status);
            QPointer<QPushButton> button_ref(validate);
            telemetry_core_.ValidateDriverKey(
                website_api->text(),
                submitted_key,
                [dialog_ref, key_ref, status_ref, button_ref,
                 save_sync_settings](
                    bool valid, const QString& result) {
                    if (!dialog_ref) {
                        return;
                    }
                    if (button_ref) {
                        button_ref->setEnabled(true);
                    }
                    if (status_ref) {
                        status_ref->setText(valid
                            ? QStringLiteral("Driver key verified for %1.").arg(result)
                            : result);
                    }
                    if (valid && key_ref) {
                        key_ref->clear();
                    }
                    if (valid) {
                        save_sync_settings();
                    }
                });
        });
        connect(clear, &QPushButton::clicked, &dialog, [this, &dialog, status, driver_key] {
            if (QMessageBox::question(
                    &dialog,
                    QStringLiteral("Clear saved driver key"),
                    QStringLiteral("Remove the saved driver key from this Windows user profile?"))
                != QMessageBox::Yes) {
                return;
            }
            QString error;
            if (!telemetry_core_.ClearDriverKey(&error)) {
                QMessageBox::warning(
                    &dialog, QStringLiteral("Driver key was not cleared"), error);
                return;
            }
            driver_key->clear();
            status->setText(QStringLiteral(
                "No driver key is saved. A key is optional; local telemetry continues normally."));
        });
        dialog.exec();
    });
    header_layout->addWidget(active_page_title_, 0, Qt::AlignLeft | Qt::AlignVCenter);
    header_layout->addWidget(connection_indicator_, 0, Qt::AlignVCenter);
    header_layout->addWidget(header_clock_, 1);
    content_layout->addWidget(header);

    page_stack_ = new QStackedWidget(content);
    page_stack_->setObjectName(QStringLiteral("pageStack"));
    page_stack_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    page_stack_->setMinimumSize(0, 0);
    content_layout->addWidget(page_stack_, 1);

    struct NavigationEntry {
        QString key;
        QString title;
        QString icon;
        StatePage* page;
    };
    const QList<NavigationEntry> workspace_entries = {
        {QStringLiteral("dashboard"), QStringLiteral("DASHBOARD"),
            QStringLiteral("dashboard"),
            new DashboardPage(this)},
        {QStringLiteral("jobs"), QStringLiteral("COMPLETED JOBS"),
            QStringLiteral("jobs"),
            new JobsPage(this)},
        {QStringLiteral("history"), QStringLiteral("HISTORY"),
            QStringLiteral("history"),
            new HistoryPage(this)},
        {QStringLiteral("events"), QStringLiteral("EVENTS"),
            QStringLiteral("events"),
            new EventsPage(this)},
    };
    auto* settings_page = new SettingsPage(this);
    settings_page->AddConnectControl(enter_driver_key_button_);
    settings_page->AddConnectControl(sync_now_button_);
    settings_page->AddConnectControl(send_demo_button_);
    const QList<NavigationEntry> system_entries = {
        {QStringLiteral("settings"), QStringLiteral("SETTINGS"),
            QStringLiteral("settings"),
            settings_page},
        {QStringLiteral("about"), QStringLiteral("ABOUT"),
            QStringLiteral("about"),
            new AboutPage(version_label, this)},
    };

    auto* workspace_label = new QLabel(QStringLiteral("WORKSPACE"), sidebar);
    workspace_label->setObjectName(QStringLiteral("sectionLabel"));
    sidebar_layout->addWidget(workspace_label);

    const auto add_navigation_entry = [this, sidebar, sidebar_layout](
                                          const NavigationEntry& entry) {
        auto* button = new QToolButton(sidebar);
        button->setObjectName(QStringLiteral("navigationButton"));
        button->setText(entry.title.toUpper());
        button->setIcon(NavigationIcon(entry.icon, false));
        button->setIconSize(QSize(18, 18));
        button->setToolTip(entry.title);
        button->setAccessibleName(entry.title);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        sidebar_layout->addWidget(button);
        navigation_buttons_.insert(entry.key, button);
        pages_.insert(entry.key, entry.page);
        auto* page_scroll = new QScrollArea(page_stack_);
        page_scroll->setObjectName(QStringLiteral("pageScroll"));
        page_scroll->setWidgetResizable(true);
        page_scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        page_scroll->setMinimumSize(0, 0);
        page_scroll->setFrameShape(QFrame::NoFrame);
        page_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        page_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        page_scroll->setWidget(entry.page);
        page_scroll_areas_.insert(entry.key, page_scroll);
        page_stack_->addWidget(page_scroll);
        connect(button, &QToolButton::clicked, this, [this, key = entry.key] {
            ActivatePage(key);
        });
    };

    for (const auto& entry : workspace_entries) {
        add_navigation_entry(entry);
    }

    auto* system_label = new QLabel(QStringLiteral("SYSTEM"), sidebar);
    system_label->setObjectName(QStringLiteral("sectionLabel"));
    system_label->setContentsMargins(0, 10, 0, 0);
    sidebar_layout->addWidget(system_label);
    for (const auto& entry : system_entries) {
        add_navigation_entry(entry);
    }

    sidebar_layout->addStretch(1);
    auto* close_button = new QToolButton(sidebar);
    close_button->setObjectName(QStringLiteral("navigationButton"));
    close_button->setText(QStringLiteral("CLOSE"));
    close_button->setIcon(NavigationIcon(QStringLiteral("close"), false));
    close_button->setIconSize(QSize(18, 18));
    close_button->setToolTip(QStringLiteral("Close NLSI Exclusive Logbook"));
    close_button->setAccessibleName(QStringLiteral("Close NLSI Exclusive Logbook"));
    close_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    close_button->setAutoRaise(true);
    close_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    sidebar_layout->addWidget(close_button);
    connect(close_button, &QToolButton::clicked, this, &QWidget::close);

    root_layout->addWidget(sidebar);
    root_layout->addWidget(content, 1);
    setCentralWidget(root);

    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        notification_tray_ = new QSystemTrayIcon(windowIcon(), this);
        notification_tray_->setToolTip(QStringLiteral("NLSI Exclusive Logbook"));
        notification_tray_->show();
    }

    refresh_timer_ = new QTimer(this);
    refresh_timer_->setObjectName(QStringLiteral("historyRefreshTimer"));
    refresh_timer_->setInterval(250);
    connect(refresh_timer_, &QTimer::timeout, this, [this] {
        RefreshState();
    });
    clock_timer_ = new QTimer(this);
    clock_timer_->setTimerType(Qt::PreciseTimer);
    clock_timer_->setInterval(1000);
    connect(clock_timer_, &QTimer::timeout, this, &MainWindow::RefreshClock);
    shutdown_timer_ = new QTimer(this);
    shutdown_timer_->setInterval(200);
    connect(shutdown_timer_, &QTimer::timeout, this, &MainWindow::PollShutdown);
    ActivatePage(QStringLiteral("dashboard"));
    RefreshClock();
    RefreshState();
    refresh_timer_->start();
    clock_timer_->start();
}

void MainWindow::ActivatePage(const QString& key) {
    StatePage* page = pages_.value(key, nullptr);
    if (!page) {
        return;
    }
    page_stack_->setCurrentWidget(page_scroll_areas_.value(key));
    active_page_title_->setText(navigation_buttons_.value(key)->text().toUpper());
    active_page_title_->setProperty(
        "dashboardActive", key == QStringLiteral("dashboard"));
    active_page_title_->style()->unpolish(active_page_title_);
    active_page_title_->style()->polish(active_page_title_);
    RefreshClock();
    for (auto it = navigation_buttons_.cbegin(); it != navigation_buttons_.cend(); ++it) {
        const bool selected = it.key() == key;
        it.value()->setChecked(selected);
        it.value()->setIcon(NavigationIcon(it.key(), selected));
    }
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    RefreshClock();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (shutdown_complete_) {
        event->accept();
        return;
    }
    if (!shutdown_started_) {
        QMessageBox confirmation(QMessageBox::Question,
            QStringLiteral("Exit NLSI Exclusive Logbook"),
            QStringLiteral("Are you sure you want to exit NLSI Exclusive Logbook?"),
            QMessageBox::NoButton,
            this);
        QPushButton* exit_button = confirmation.addButton(
            QStringLiteral("Exit"), QMessageBox::AcceptRole);
        QPushButton* cancel_button = confirmation.addButton(
            QStringLiteral("Cancel"), QMessageBox::RejectRole);
        confirmation.setDefaultButton(cancel_button);
        confirmation.setEscapeButton(cancel_button);
        confirmation.exec();
        if (confirmation.clickedButton() != exit_button) {
            event->ignore();
            return;
        }
        shutdown_started_ = true;
        telemetry_core_.BeginShutdown();
    }
    event->ignore();
    if (!shutdown_dialog_) {
        shutdown_dialog_ = new QProgressDialog(
            QStringLiteral("Stopping telemetry producers and saving accepted local records."),
            QStringLiteral("Keep app open"), 0, 0, this);
        shutdown_dialog_->setWindowTitle(QStringLiteral("Saving local data"));
        shutdown_dialog_->setWindowModality(Qt::WindowModal);
        shutdown_dialog_->setMinimumDuration(0);
        shutdown_dialog_->setAutoClose(false);
        shutdown_dialog_->setAutoReset(false);
        connect(shutdown_dialog_, &QProgressDialog::canceled, this, [this] {
            shutdown_timer_->stop();
            shutdown_dialog_->hide();
        });
    }
    shutdown_dialog_->show();
    shutdown_wait_.start();
    shutdown_timer_->start();
    PollShutdown();
}

void MainWindow::RefreshClock() {
    const QDateTime manila_time = nlsi::time::NowLocal();
    const bool compact = header_clock_->width() < 400;
    const QLocale english(QLocale::English, QLocale::UnitedStates);
    const QString text = manila_time.isValid()
        ? QStringLiteral("%1 - %2 - ASIA/MANILA | PING: --")
            .arg(english.toString(manila_time, compact
                    ? QStringLiteral("MMM d, yyyy")
                    : QStringLiteral("MMMM d, yyyy"))
                    .toUpper(),
                manila_time.toString(QStringLiteral("HH:mm:ss")))
        : QStringLiteral("TIME UNAVAILABLE - ASIA/MANILA | PING: --");
    if (header_clock_->text() != text) {
        header_clock_->setText(text);
    }
}

void MainWindow::PollShutdown() {
    const auto progress = telemetry_core_.PollShutdown();
    if (progress.state == telemetry::TelemetryCore::ShutdownState::Completed) {
        shutdown_timer_->stop();
        shutdown_complete_ = true;
        if (shutdown_dialog_) {
            shutdown_dialog_->hide();
        }
        QTimer::singleShot(0, this, &QWidget::close);
        return;
    }
    if (progress.state == telemetry::TelemetryCore::ShutdownState::Failed) {
        shutdown_timer_->stop();
        if (shutdown_dialog_) {
            shutdown_dialog_->hide();
        }
        QMessageBox prompt(QMessageBox::Critical,
            QStringLiteral("Local data could not be fully saved"),
            QStringLiteral("%1\n\n%2 accepted local write(s) remain queued. "
                "Writes already copied to recovery files will be retried at next start; "
                "an in-memory write that could not be copied may be lost if you exit. "
                "Exiting now will not be reported as a clean shutdown.")
                .arg(QString::fromStdWString(progress.error))
                .arg(progress.queued_writes),
            QMessageBox::NoButton,
            this);
        QPushButton* exit_button = prompt.addButton(
            QStringLiteral("Exit with write error"), QMessageBox::DestructiveRole);
        QPushButton* keep_button = prompt.addButton(
            QStringLiteral("Keep app open"), QMessageBox::RejectRole);
        prompt.setDefaultButton(keep_button);
        prompt.exec();
        if (prompt.clickedButton() == exit_button) {
            shutdown_complete_ = true;
            QTimer::singleShot(0, this, &QWidget::close);
        }
        return;
    }

    if (shutdown_dialog_) {
        shutdown_dialog_->setLabelText(
            QStringLiteral("Draining accepted local telemetry writes: %1 queued; "
                "%2 local-only records pending synchronization.")
                .arg(progress.queued_writes)
                .arg(progress.pending_records));
    }
    if (shutdown_wait_.elapsed() >= 5000) {
        shutdown_wait_.restart();
        QMessageBox prompt(QMessageBox::Warning,
            QStringLiteral("Telemetry shutdown is taking longer than expected"),
            QStringLiteral("%1 accepted write(s) remain in progress. You may keep waiting "
                "or leave the application open. The writer will not be forcibly terminated.")
                .arg(progress.queued_writes),
            QMessageBox::NoButton,
            this);
        QPushButton* wait_button = prompt.addButton(
            QStringLiteral("Keep waiting"), QMessageBox::AcceptRole);
        QPushButton* open_button = prompt.addButton(
            QStringLiteral("Keep app open"), QMessageBox::RejectRole);
        prompt.setDefaultButton(wait_button);
        prompt.exec();
        if (prompt.clickedButton() == open_button) {
            shutdown_timer_->stop();
            if (shutdown_dialog_) {
                shutdown_dialog_->hide();
            }
        }
    }
}

void MainWindow::RefreshState() {
    const auto state = telemetry_core_.UiState();
    const auto history = telemetry_core_.History();
    for (const std::wstring& event_id : telemetry_core_.TakeNewlyCompletedJobNotifications()) {
        if (!notification_tray_) {
            qWarning("Desktop notification unavailable: Windows system tray is not available.");
            continue;
        }
        Q_UNUSED(event_id);
        notification_tray_->showMessage(
            QStringLiteral("NLSI Exclusive Logbook"),
            QStringLiteral("Job has finished"),
            QSystemTrayIcon::Information,
            5000);
    }
    for (const std::wstring& message : telemetry_core_.TakeNewExpenseNotifications()) {
        if (!notification_tray_) {
            qWarning("Expense notification unavailable: Windows system tray is not available.");
            continue;
        }
        notification_tray_->showMessage(
            QStringLiteral("NLSI Exclusive Logbook · Travel expense"),
            QString::fromStdWString(message),
            QSystemTrayIcon::Information,
            5000);
    }

    for (StatePage* page : pages_) {
        page->UpdateState(state);
        page->UpdateHistory(history);
    }

    const auto& sync = state.providers;
    const QString connection_text = !sync.sync_configured
        ? QStringLiteral("LOCAL MODE")
        : (sync.api_connected
            ? (sync.sync_pending_count > 0
                ? QStringLiteral("SYNC PENDING")
                : QStringLiteral("CONNECTED"))
            : QStringLiteral("SYNC UNAVAILABLE"));
    if (connection_indicator_->text() != connection_text) {
        connection_indicator_->setText(connection_text);
    }
    const QString last_check = sync.last_api_check.empty()
        ? QStringLiteral("Never")
        : QString::fromStdWString(sync.last_api_check);
    const QString last_attempt = sync.last_sync_attempt.empty()
        ? QStringLiteral("Never")
        : QString::fromStdWString(sync.last_sync_attempt);
    const QString last_sync = sync.last_successful_sync.empty()
        ? QStringLiteral("Never")
        : QString::fromStdWString(sync.last_successful_sync);
    QString tooltip = QStringLiteral(
        "Sync mode: %1\nPending local records: %2\nLast successful API heartbeat: %3\nLast successful sync: %4\nLast sync attempt: %5")
        .arg(!sync.sync_configured
                ? QStringLiteral("Local only; configure a device key to upload")
                : (sync.api_connected
                    ? QStringLiteral("Device authenticated; API and database connected")
                    : QStringLiteral("Device/API unavailable")),
            QString::number(sync.sync_pending_count),
            last_check,
            last_sync,
            last_attempt);
    if (!sync.sync_error.empty()) {
        tooltip += QStringLiteral("\nDetails: %1").arg(
            QString::fromStdWString(sync.sync_error));
    }
    connection_indicator_->setToolTip(tooltip);
    sync_now_button_->setEnabled(sync.sync_configured && !sync.sync_in_progress);
    sync_now_button_->setToolTip(sync.sync_configured
        ? (sync.sync_in_progress
            ? QStringLiteral("Synchronization is in progress.")
            : QStringLiteral("Attempt synchronization of %1 locally queued record(s).")
                .arg(sync.sync_pending_count))
        : QStringLiteral(
            "Local mode is active. Use Enter Your Key in Settings > Connect to configure the HTTPS endpoint and device key."));
    if (!sync.sync_error.empty()) {
        statusBar()->showMessage(
            QStringLiteral("Synchronization issue: %1")
                .arg(QString::fromStdWString(sync.sync_error)));
    } else if (sync.sync_pending_count > 0) {
        statusBar()->showMessage(
            QStringLiteral("%1 local record(s) are waiting for server confirmation.")
                .arg(sync.sync_pending_count));
    } else if (sync.sync_configured && sync.api_connected) {
        statusBar()->clearMessage();
    }
}

} // namespace nlsi::gui
