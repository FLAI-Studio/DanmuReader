#include "mainwindow.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), connected(false) {

    setWindowTitle("抖音弹幕朗读助手");
    resize(500, 600);

    // TTS
    tts = new QTextToSpeech(this);
    voices = tts->availableVoices();

    // WebSocket
    webSocket = new QWebSocket();

    // UI
    QWidget* central = new QWidget();
    setCentralWidget(central);
    QVBoxLayout* layout = new QVBoxLayout(central);
    layout->setSpacing(10);
    layout->setContentsMargins(14, 12, 14, 12);

    QLabel* title = new QLabel("🎙️ 抖音弹幕朗读助手");
    title->setStyleSheet("font-size: 22px; font-weight: bold; color: #5b8def;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // 连接
    QGroupBox* connGroup = new QGroupBox("连接设置");
    QHBoxLayout* connLayout = new QHBoxLayout(connGroup);
    urlInput = new QLineEdit("ws://127.0.0.1:8888");
    urlInput->setPlaceholderText("弹幕网关地址");
    connectBtn = new QPushButton("连接");
    disconnectBtn = new QPushButton("断开");
    disconnectBtn->setEnabled(false);
    connLayout->addWidget(urlInput);
    connLayout->addWidget(connectBtn);
    connLayout->addWidget(disconnectBtn);
    layout->addWidget(connGroup);

    // TTS
    QGroupBox* ttsGroup = new QGroupBox("TTS 设置");
    QVBoxLayout* ttsLayout = new QVBoxLayout(ttsGroup);
    QHBoxLayout* voiceLayout = new QHBoxLayout();
    voiceLayout->addWidget(new QLabel("音色："));
    voiceCombo = new QComboBox();
    for (const auto& v : voices) voiceCombo->addItem(v.name());
    voiceLayout->addWidget(voiceCombo);
    ttsLayout->addLayout(voiceLayout);
    QHBoxLayout* rateLayout = new QHBoxLayout();
    rateLayout->addWidget(new QLabel("语速："));
    rateSlider = new QSlider(Qt::Horizontal);
    rateSlider->setRange(0, 100);
    rateSlider->setValue(50);
    rateLabel = new QLabel("50");
    rateLabel->setFixedWidth(40);
    rateLayout->addWidget(rateSlider);
    rateLayout->addWidget(rateLabel);
    ttsLayout->addLayout(rateLayout);
    layout->addWidget(ttsGroup);

    // 弹幕列表
    QGroupBox* listGroup = new QGroupBox("弹幕");
    QVBoxLayout* listLayout = new QVBoxLayout(listGroup);
    danmuList = new QListWidget();
    danmuList->setMaximumHeight(220);
    listLayout->addWidget(danmuList);
    layout->addWidget(listGroup);

    // 日志
    QGroupBox* logGroup = new QGroupBox("日志");
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
    logView = new QTextEdit();
    logView->setReadOnly(true);
    logView->setMaximumHeight(80);
    logLayout->addWidget(logView);
    layout->addWidget(logGroup);

    // ---------- 底部版本信息 ----------
    QLabel* footer = new QLabel(
        "本程序仅供个人自用，用于主播本人监听自己直播间弹幕；请遵守相关法律法规，相信科学\n\n"
        "版本：v0.0.1-rc1\n"
        "开发者：Byjsmc\n"
        "最后更新于：2026/09/04"
        );
    footer->setStyleSheet("color: gray; font-size: 11px;");
    footer->setWordWrap(true);
    layout->addWidget(footer);

    // 信号
    connect(connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(webSocket, &QWebSocket::connected, this, &MainWindow::onWebSocketConnected);
    connect(webSocket, &QWebSocket::disconnected, this, &MainWindow::onWebSocketDisconnected);
    connect(webSocket, &QWebSocket::textMessageReceived, this, &MainWindow::onTextMessageReceived);
    connect(webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &MainWindow::onWebSocketError);
    connect(tts, &QTextToSpeech::stateChanged, this, &MainWindow::onStateChanged);

    connect(rateSlider, &QSlider::valueChanged, this, [this](int v) {
        rateLabel->setText(QString::number(v));
        tts->setRate(v / 100.0);
    });
    connect(voiceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx >= 0 && idx < voices.size()) tts->setVoice(voices[idx]);
    });

    log("程序已启动，请先启动弹幕网关再点击连接");
}

MainWindow::~MainWindow() {
    if (webSocket->state() == QAbstractSocket::ConnectedState) webSocket->close();
}

void MainWindow::onConnectClicked() {
    QString url = urlInput->text().trimmed();
    if (url.isEmpty()) { QMessageBox::warning(this, "错误", "请输入弹幕网关地址"); return; }
    log(QString("正在连接 %1...").arg(url));
    webSocket->open(QUrl(url));
}

void MainWindow::onDisconnectClicked() {
    webSocket->close();
    log("已断开连接");
}

void MainWindow::onWebSocketConnected() {
    connected = true;
    connectBtn->setEnabled(false);
    disconnectBtn->setEnabled(true);
    log("✅ 已连接到弹幕服务");
}

void MainWindow::onWebSocketDisconnected() {
    connected = false;
    connectBtn->setEnabled(true);
    disconnectBtn->setEnabled(false);
    log("❌ 连接已断开");
}

void MainWindow::onWebSocketError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    log(QString("连接错误: %1").arg(webSocket->errorString()));
}

void MainWindow::onTextMessageReceived(const QString& message) {
    parseDanmu(message);
}

void MainWindow::onStateChanged(QTextToSpeech::State state) {
    Q_UNUSED(state);
}

void MainWindow::parseDanmu(const QString& json) {
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isNull() || !doc.isObject()) return;
    QJsonObject root = doc.object();
    int type = root.value("Type").toInt();
    QJsonObject data = root.value("Data").toObject();

    if (type == 1) {
        QString nickname = data.value("User").toObject().value("Nickname").toString("?");
        QString content = data.value("Content").toString().trimmed();
        if (content.isEmpty()) return;
        danmuList->addItem(QString("%1: %2").arg(nickname).arg(content));
        if (danmuList->count() > 80) danmuList->takeItem(0);
        danmuList->scrollToBottom();
        speak(QString("%1说：%2").arg(nickname).arg(content));
    } else if (type == 5) {
        QString nickname = data.value("User").toObject().value("Nickname").toString("?");
        QString gift = data.value("GiftName").toString("礼物");
        int count = data.value("GiftCount").toInt(1);
        QString text = QString("感谢%1送出的%2%3个").arg(nickname).arg(gift).arg(count);
        danmuList->addItem(QString("[礼物] %1").arg(text));
        speak(text);
    }
}

void MainWindow::speak(const QString& text) {
    if (!connected) return;
    tts->say(text);
}

void MainWindow::log(const QString& msg) {
    QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    logView->append(QString("[%1] %2").arg(ts).arg(msg));
    logView->verticalScrollBar()->setValue(logView->verticalScrollBar()->maximum());
}