#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWebSocket>
#include <QTextToSpeech>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QComboBox>
#include <QSlider>
#include <QTextEdit>
#include <QGroupBox>
#include <QCheckBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollBar>
#include <QMessageBox>
#include <QDateTime>
#include <QProcess>
#include <QQueue>
#include <QTimer>
#include <QSettings>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onConnectClicked();
    void onDisconnectClicked();
    void onTextMessageReceived(const QString& message);
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onStateChanged(QTextToSpeech::State state);
    void onTtsTimerTimeout();
    void onReconnectTimerTimeout();
    void onGatewayWatchdogTimeout();
    void onAlwaysOnTopToggled(bool checked);
    void onOpacityChanged(int value);
    void saveConfig();
    void loadConfig();

private:
    // UI
    QLineEdit* urlInput;
    QPushButton* connectBtn;
    QPushButton* disconnectBtn;
    QListWidget* danmuList;
    QComboBox* voiceCombo;
    QSlider* rateSlider;
    QLabel* rateLabel;
    QTextEdit* logView;
    QCheckBox* danmuSwitch;
    QCheckBox* giftSwitch;
    QCheckBox* enterSwitch;
    QSlider* volumeSlider;
    QLabel* volumeLabel;
    QCheckBox* alwaysOnTopCheck;
    QSlider* opacitySlider;
    QLabel* opacityLabel;
    QLineEdit* danmuTemplateInput;
    QLineEdit* giftTemplateInput;

    // 弹幕
    QWebSocket* webSocket;
    bool connected;

    // TTS
    QTextToSpeech* tts;
    QVector<QVoice> voices;

    QProcess* gatewayProcess;
    QQueue<QString> ttsQueue;
    QTimer* ttsTimer;
    QTimer* reconnectTimer;
    QTimer* gatewayWatchdogTimer;
    int reconnectAttempts;
    bool waitingForGateway;
    bool manualDisconnect;
    QString danmuTpl;
    QString giftTpl;

    void log(const QString& msg);
    void speak(const QString& text);
    void enqueueSpeak(const QString& text);
    void parseDanmu(const QString& json);

    bool ensureGatewayRunning();
    void startGateway();
    void stopGateway();
};

#endif // MAINWINDOW_H
