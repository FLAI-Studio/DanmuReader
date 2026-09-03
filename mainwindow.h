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
#include <QJsonDocument>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QMessageBox>
#include <QDateTime>

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

    // 弹幕
    QWebSocket* webSocket;
    bool connected;

    // TTS
    QTextToSpeech* tts;
    QVector<QVoice> voices;

    void log(const QString& msg);
    void speak(const QString& text);
    void parseDanmu(const QString& json);
};

#endif // MAINWINDOW_H