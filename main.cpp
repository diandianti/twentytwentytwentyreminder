#include <QApplication>
#include <QWidget>
#include <QPainter>
#include <QTimer>
#include <QTime>
#include <QPropertyAnimation>
#include <QScreen>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QDir>
#include <QRandomGenerator>
#include <QFileInfo>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QDebug>
#include <QAction>
#include <QStandardPaths>


#ifndef QT_DEBUG
void dummyMessageHandler(QtMsgType, const QMessageLogContext &, const QString &) {
}
#endif


struct AppConfig {
    int intervalMinutes = 20;
    int displaySeconds = 20;
    int fadeDurationMs = 1000;
    QString imageSource; 
    QString textColor = "#FFFFFF";
    int fontSize = 100;
    QString position = "Center"; 
    bool showMask = true;       

    Qt::Alignment getAlignment() const {
        QString pos = position.toLower();
        if (pos == "topleft") return Qt::AlignTop | Qt::AlignLeft;
        if (pos == "topcenter") return Qt::AlignTop | Qt::AlignHCenter;
        if (pos == "topright") return Qt::AlignTop | Qt::AlignRight;
        if (pos == "bottomleft") return Qt::AlignBottom | Qt::AlignLeft;
        if (pos == "bottomcenter") return Qt::AlignBottom | Qt::AlignHCenter;
        if (pos == "bottomright") return Qt::AlignBottom | Qt::AlignRight;
        return Qt::AlignCenter;
    }

    void load(const QString &configPath) {
        qDebug() << "[Config] Loading via Rugged Parser:" << configPath;
        QFile file(configPath);
        
        if (!file.exists()) {
            QFileInfo fileInfo(configPath);
            QDir().mkpath(fileInfo.absolutePath());

            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << "[General]\n";
                out << "interval_minutes=20\n";
                out << "display_seconds=20\n";
                out << "fade_ms=1000\n";
                out << "image_source=/home/\n"; 
                out << "\n[Theme]\n";
                out << "text_color=#FFFFFF\n";
                out << "font_size=100\n";
                out << "position=Center\n";
                out << "show_mask=true\n";
                file.close();
                qDebug() << "[Config] Created default file at:" << configPath;
            } else {
                qWarning() << "[Config] Failed to create default file:" << file.errorString();
            }
            return;
        }

        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                
                if (line.isEmpty() || line.startsWith(";") || line.startsWith("#") || line.startsWith("[")) {
                    continue;
                }

                int equalPos = line.indexOf('=');
                if (equalPos == -1) continue;

                QString key = line.left(equalPos).trimmed();
                QString value = line.mid(equalPos + 1).trimmed();

                if (value.startsWith('"') && value.endsWith('"')) {
                    value = value.mid(1, value.length() - 2);
                }

                qDebug() << "   Key:" << key << "Value:" << value;

                if (key == "interval_minutes") intervalMinutes = value.toInt();
                else if (key == "display_seconds") displaySeconds = value.toInt();
                else if (key == "fade_ms") fadeDurationMs = value.toInt();
                else if (key == "image_source") imageSource = value;
                else if (key == "text_color") textColor = value;
                else if (key == "font_size") fontSize = value.toInt();
                else if (key == "position") position = value;
                else if (key == "show_mask") showMask = (value.toLower() == "true");
            }
            file.close();
        }

        qDebug() << "------ Final Loaded Values ------";
        qDebug() << "Image Source:" << imageSource;
        qDebug() << "Interval:" << intervalMinutes;
        qDebug() << "---------------------------------";
    }
};


class ImageUtils {
public:
    static QPixmap getRandomPixmap(const QString &sourcePath) {
        if (sourcePath.isEmpty()) {
            qWarning() << "[ImageUtils] Path is empty!";
            return QPixmap();
        }

        QFileInfo info(sourcePath);
        QString cleanPath = QDir::cleanPath(sourcePath); 
        
        QString finalPath;

        if (info.isDir()) {
            QDir dir(cleanPath);
            QStringList filters;
            filters << "*.jpg" << "*.jpeg" << "*.png" << "*.bmp";
            dir.setNameFilters(filters);
            QFileInfoList list = dir.entryInfoList(filters, QDir::Files);
            
            if (!list.isEmpty()) {
                int index = QRandomGenerator::global()->bounded(list.size());
                finalPath = list[index].absoluteFilePath();
                qDebug() << "[ImageUtils] Selected:" << finalPath;
            } else {
                qWarning() << "[ImageUtils] Directory found but no images inside:" << cleanPath;
            }
        } else if (info.isFile()) {
            finalPath = cleanPath;
        } else {
            qWarning() << "[ImageUtils] Path is neither a file nor a directory:" << sourcePath;
        }

        if (finalPath.isEmpty() || !QFile::exists(finalPath)) {
            return QPixmap(); 
        }
        return QPixmap(finalPath);
    }
};


class ReminderOverlay : public QWidget {
    Q_OBJECT
    Q_PROPERTY(double paintOpacity READ getPaintOpacity WRITE setPaintOpacity)

public:
    explicit ReminderOverlay(const AppConfig &config, const QPixmap &pixmap, QScreen *screen, QWidget *parent = nullptr)
        : QWidget(parent), m_config(config), m_bgPixmap(pixmap), m_currentOpacity(0.0) {
        
        setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground); 
        setAttribute(Qt::WA_DeleteOnClose);

        if (screen) {
            setGeometry(screen->geometry());
            setScreen(screen);
        }

        m_clockTimer = new QTimer(this);
        connect(m_clockTimer, &QTimer::timeout, this, QOverload<>::of(&QWidget::update));
        m_clockTimer->start(1000);

        m_fadeAnimation = new QPropertyAnimation(this, "paintOpacity");
        m_fadeAnimation->setDuration(m_config.fadeDurationMs);

        m_hideTimer = new QTimer(this);
        m_hideTimer->setSingleShot(true);
        connect(m_hideTimer, &QTimer::timeout, this, &ReminderOverlay::startFadeOut);
    }

    double getPaintOpacity() const { return m_currentOpacity; }
    void setPaintOpacity(double opacity) { m_currentOpacity = opacity; update(); }

    void startShow() {
        showFullScreen();
        m_fadeAnimation->stop();
        m_fadeAnimation->setStartValue(0.0);
        m_fadeAnimation->setEndValue(1.0);
        m_fadeAnimation->start();

        connect(m_fadeAnimation, &QPropertyAnimation::finished, this, [this](){
            disconnect(m_fadeAnimation, &QPropertyAnimation::finished, nullptr, nullptr);
            m_hideTimer->start(m_config.displaySeconds * 1000);
        });
    }

protected:
    void paintEvent(QPaintEvent *event) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setOpacity(m_currentOpacity);

        if (m_bgPixmap.isNull()) {
            painter.fillRect(rect(), Qt::black);
            painter.setPen(Qt::white);
            painter.drawText(rect(), Qt::AlignCenter, "No Image Found\nCheck Logs");
        } else {
            QImage scaledImg = m_bgPixmap.toImage().scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            int x = (size().width() - scaledImg.width()) / 2;
            int y = (size().height() - scaledImg.height()) / 2;
            painter.drawImage(x, y, scaledImg);
        }

        if (m_config.showMask) {
            painter.fillRect(rect(), QColor(0, 0, 0, 80));
        }

        QString timeText = QTime::currentTime().toString("HH:mm");
        painter.setPen(QColor(m_config.textColor));
        QFont font = painter.font();
        font.setPixelSize(m_config.fontSize);
        font.setBold(true);
        painter.setFont(font);

        QRect drawRect = rect().adjusted(50, 50, -50, -50); 
        painter.drawText(drawRect, m_config.getAlignment(), timeText);
    }

private slots:
    void startFadeOut() {
        m_fadeAnimation->stop();
        m_fadeAnimation->setStartValue(1.0);
        m_fadeAnimation->setEndValue(0.0);
        m_fadeAnimation->start();
        connect(m_fadeAnimation, &QPropertyAnimation::finished, this, [this](){
            m_clockTimer->stop();
            close(); 
        });
    }

private:
    AppConfig m_config;
    QPixmap m_bgPixmap;
    QTimer *m_clockTimer;
    QTimer *m_hideTimer;
    QPropertyAnimation *m_fadeAnimation;
    double m_currentOpacity;
};


class AppController : public QObject {
    Q_OBJECT
public:
    explicit AppController(const QString &configPath) : m_configPath(configPath) {
        qDebug() << "[Controller] Initializing...";
        
        m_schedulerTimer = new QTimer(this);
        connect(m_schedulerTimer, &QTimer::timeout, this, &AppController::showAllReminders);

        reloadConfig();
        setupTray();

        qDebug() << "[Controller] Started. Config:" << m_configPath;
    }

private slots:
    void showAllReminders() {
        m_config.load(m_configPath);
        
        QPixmap currentPix = ImageUtils::getRandomPixmap(m_config.imageSource);
        const auto screens = QGuiApplication::screens();
        for (QScreen *screen : screens) {
            ReminderOverlay *overlay = new ReminderOverlay(m_config, currentPix, screen);
            overlay->startShow();
        }
    }

    void reloadConfig() {
        m_config.load(m_configPath);
        startTimer(); 
    }

    void startTimer() {
        if (!m_schedulerTimer) return;
        m_schedulerTimer->stop();
        if (m_config.intervalMinutes > 0) {
            m_schedulerTimer->start(m_config.intervalMinutes * 60 * 1000);
            qDebug() << "[Timer] Next reminder in" << m_config.intervalMinutes << "mins";
        }
    }

private:
    void setupTray() {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) return;

        m_trayIcon = new QSystemTrayIcon(this);
        QIcon icon = QIcon::fromTheme("alarm-symbolic");
        if (icon.isNull()) icon = QIcon::fromTheme("appointment-new");
        m_trayIcon->setIcon(icon);
        m_trayIcon->setToolTip("TwentyTwentyTwentyReminder"); 

        QMenu *menu = new QMenu();
        
        QAction *showNowAction = menu->addAction("立即显示 (Test)");
        connect(showNowAction, &QAction::triggered, this, &AppController::showAllReminders);

        QAction *reloadAction = menu->addAction("重载配置");
        connect(reloadAction, &QAction::triggered, this, &AppController::reloadConfig);

        menu->addSeparator();

        QAction *quitAction = menu->addAction("退出");
        connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

        m_trayIcon->setContextMenu(menu);
        m_trayIcon->show();
    }

    QString m_configPath;
    AppConfig m_config;
    QTimer *m_schedulerTimer = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;
};

#include "main.moc"

int main(int argc, char *argv[]) {
    #ifdef QT_DEBUG
        qputenv("QT_LOGGING_RULES", "*.debug=true");
    #else
        qInstallMessageHandler(dummyMessageHandler);
    #endif

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setApplicationName("TwentyTwentyTwentyReminder");

    QCommandLineParser parser;
    parser.setApplicationDescription("Twenty-Twenty-Twenty Reminder");
    parser.addHelpOption();
    QCommandLineOption configOption(QStringList() << "c" << "config", "Path to config", "path");
    parser.addOption(configOption);
    parser.process(app);

    QString configPath;
    if (parser.isSet(configOption)) {
        configPath = parser.value(configOption);
    } else {
        QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
        configPath = QDir(configDir).filePath("tttreminder.ini");
    }

    QFileInfo fileInfo(configPath);
    QString absConfigPath = fileInfo.absoluteFilePath();
    
    qDebug() << "========================================";
    qDebug() << "[Main] Target config path:" << absConfigPath;
    qDebug() << "[Main] File exists?" << fileInfo.exists();
    qDebug() << "========================================";

    AppController controller(absConfigPath);
    return app.exec();
}