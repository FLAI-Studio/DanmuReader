#include "mainwindow.h"

#include <QCheckBox>
#include <QEventLoop>
#include <QTimer>
#include <QFile>
#include <QApplication>
#include <QThread>
#include <QDir>


/**
 * @brief 检查网关是否已经在运行（通过尝试连接 ws 端口）
 * @return true 表示网关已就绪
 */
bool MainWindow::ensureGatewayRunning() {

    QWebSocket probe;
    QEventLoop loop;
    bool ok = false;

    QObject::connect(&probe, &QWebSocket::connected, [&]() {
        ok = true;
        probe.close();
    });
    QObject::connect(&probe, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
                     [&loop]() { loop.quit(); });

    probe.open(QUrl(urlInput->text().trimmed()));
    QTimer::singleShot(1500, &loop, &QEventLoop::quit);
    loop.exec();

    return ok;
}

/**
 * @brief 启动网关子进程（后台常驻）
 */
void MainWindow::startGateway() {
    QString gatewayDir = QCoreApplication::applicationDirPath() + "/gateway";

    // 列出 gateway 目录下所有 exe，自动找
    QDir dir(gatewayDir);
    QStringList exes = dir.entryList(QStringList() << "*.exe", QDir::Files);

    if (exes.isEmpty()) {
        log("⚠️ gateway 文件夹下没有找到任何 exe 文件");
        log("请确认已将网关程序解压到: " + gatewayDir);
        return;
    }

    // 优先找包含 "Wss" 或 "Barrage" 或 "Grab" 的
    QString gatewayExe;
    for (const QString& exe : exes) {
        if (exe.contains("Wss", Qt::CaseInsensitive) ||
            exe.contains("Barrage", Qt::CaseInsensitive) ||
            exe.contains("Grab", Qt::CaseInsensitive)) {
            gatewayExe = gatewayDir + "/" + exe;
            break;
        }
    }
    // 如果没匹配到，就用第一个 exe
    if (gatewayExe.isEmpty()) gatewayExe = gatewayDir + "/" + exes.first();

    log("找到网关: " + gatewayExe);

    gatewayProcess = new QProcess(this);
    gatewayProcess->setWorkingDirectory(gatewayDir);
    gatewayProcess->start(gatewayExe, QStringList());
    log("正在启动弹幕网关...");
}

/**
 * @brief 停止网关子进程
 */
void MainWindow::stopGateway() {
    if (gatewayProcess && gatewayProcess->state() == QProcess::Running) {
        gatewayProcess->kill();
        gatewayProcess->waitForFinished(3000);
        gatewayProcess->deleteLater();
        gatewayProcess = nullptr;
        log("弹幕网关已停止");
    }
}

/**
 * @brief 主窗口构造函数
 * 初始化 UI、WebSocket 连接和 TTS 引擎
 */
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), connected(false) {

    // ========== 窗口基本设置 ==========
    setWindowTitle("抖音弹幕朗读助手");
    resize(500, 600);

    // ========== TTS 引擎初始化 ==========
    tts = new QTextToSpeech(this);
    voices = tts->availableVoices();

    // ========== WebSocket 初始化 ==========
    webSocket = new QWebSocket();

    // ========== 中央部件和主布局 ==========
    QWidget* central = new QWidget();
    setCentralWidget(central);
    QVBoxLayout* layout = new QVBoxLayout(central);
    layout->setSpacing(8);
    layout->setContentsMargins(12, 10, 12, 10);

    // ========== 标题 ==========
    QLabel* title = new QLabel("🎙️ 抖音弹幕朗读助手");
    title->setStyleSheet("font-size: 20px; font-weight: bold; color: #5b8def;");
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    // ========== 连接设置 ==========
    QGroupBox* connGroup = new QGroupBox("连接");
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

    // ========== 功能开关 ==========
    QGroupBox* switchGroup = new QGroupBox("朗读开关");
    QHBoxLayout* switchLayout = new QHBoxLayout(switchGroup);
    danmuSwitch = new QCheckBox("弹幕朗读");
    giftSwitch = new QCheckBox("礼物朗读");
    danmuSwitch->setChecked(true);   // 默认开启
    giftSwitch->setChecked(true);    // 默认开启
    switchLayout->addWidget(danmuSwitch);
    switchLayout->addWidget(giftSwitch);
    switchLayout->addStretch();
    layout->addWidget(switchGroup);

    // ========== TTS 设置 ==========
    QGroupBox* ttsGroup = new QGroupBox("语音设置");
    QGridLayout* ttsLayout = new QGridLayout(ttsGroup);
    ttsLayout->addWidget(new QLabel("音色："), 0, 0);
    voiceCombo = new QComboBox();
    for (const auto& v : voices) voiceCombo->addItem(v.name());
    ttsLayout->addWidget(voiceCombo, 0, 1);
    ttsLayout->addWidget(new QLabel("语速："), 1, 0);
    rateSlider = new QSlider(Qt::Horizontal);
    rateSlider->setRange(0, 100);
    rateSlider->setValue(50);
    rateLabel = new QLabel("50");
    rateLabel->setFixedWidth(30);
    ttsLayout->addWidget(rateSlider, 1, 1);
    ttsLayout->addWidget(rateLabel, 1, 2);
    layout->addWidget(ttsGroup);

    // ========== 弹幕列表 ==========
    QGroupBox* listGroup = new QGroupBox("弹幕");
    QVBoxLayout* listLayout = new QVBoxLayout(listGroup);
    danmuList = new QListWidget();
    danmuList->setMaximumHeight(200);
    listLayout->addWidget(danmuList);
    layout->addWidget(listGroup);

    // ========== 日志 ==========
    QGroupBox* logGroup = new QGroupBox("日志");
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
    logView = new QTextEdit();
    logView->setReadOnly(true);
    logView->setMaximumHeight(80);
    logLayout->addWidget(logView);
    layout->addWidget(logGroup);

    // ========== 底部版本信息 ==========
    QLabel* footer = new QLabel(
        "本程序仅供个人自用，用于主播本人监听自己直播间弹幕，请遵守相关法律法规，违者后果自负\n\n"
        "版本：v0.1.0\n"
        "开发者：Byjsmc\n"
        "最后更新于：2026/09/04"
        );
    footer->setStyleSheet("color: gray; font-size: 11px;");
    footer->setWordWrap(true);
    layout->addWidget(footer);

    // ========== 信号连接 ==========
    connect(connectBtn, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(disconnectBtn, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(webSocket, &QWebSocket::connected, this, &MainWindow::onWebSocketConnected);
    connect(webSocket, &QWebSocket::disconnected, this, &MainWindow::onWebSocketDisconnected);
    connect(webSocket, &QWebSocket::textMessageReceived, this, &MainWindow::onTextMessageReceived);
    connect(webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &MainWindow::onWebSocketError);
    connect(tts, &QTextToSpeech::stateChanged, this, &MainWindow::onStateChanged);

    // 语速滑块
    connect(rateSlider, &QSlider::valueChanged, this, [this](int v) {
        rateLabel->setText(QString::number(v));
        tts->setRate(v / 100.0);
    });

    // 音色选择
    connect(voiceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx >= 0 && idx < voices.size()) tts->setVoice(voices[idx]);
    });

    log("程序已启动，请先启动弹幕网关再点击连接");
}

MainWindow::~MainWindow() {
    if (webSocket->state() == QAbstractSocket::ConnectedState) webSocket->close();
    stopGateway();
}

// ==================== 连接控制 ====================

void MainWindow::onConnectClicked() {
    QString url = urlInput->text().trimmed();
    if (url.isEmpty()) { QMessageBox::warning(this, "错误", "请输入弹幕网关地址"); return; }

    if (!ensureGatewayRunning()) {
        log("网关未运行，正在尝试启动...");
        startGateway();

        QString gatewayDir = QCoreApplication::applicationDirPath() + "/gateway";
        QDir dir(gatewayDir);
        if (dir.entryList(QStringList() << "*.exe", QDir::Files).isEmpty()) {
            log("❌ gateway 文件夹下未找到任何 exe，请确认已部署网关");
            return;
        }

        log("等待网关初始化（约 3 秒）...");
        QApplication::processEvents();
        QThread::sleep(3);
    }

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

// ==================== 消息处理 ====================

void MainWindow::onTextMessageReceived(const QString& message) {
    parseDanmu(message);
}

void MainWindow::onStateChanged(QTextToSpeech::State state) {
    Q_UNUSED(state);
}

/**
 * @brief 解析弹幕网关推送的 JSON 消息
 * Data 字段是嵌套的 JSON 字符串，需要二次解析
 */
void MainWindow::parseDanmu(const QString& json) {
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (doc.isNull() || !doc.isObject()) return;
    QJsonObject root = doc.object();

    int type = root.value("Type").toInt();

    // Data 是 JSON 字符串，需要再解析一层
    QJsonObject data;
    QJsonValue dataVal = root.value("Data");
    if (dataVal.isString()) {
        QJsonDocument dataDoc = QJsonDocument::fromJson(dataVal.toString().toUtf8());
        if (dataDoc.isObject()) data = dataDoc.object();
    } else if (dataVal.isObject()) {
        data = dataVal.toObject();
    }

    // 提取用户昵称
    auto getNick = [&](const QJsonObject& d) -> QString {
        QJsonObject u = d.value("User").toObject();
        QString nick = u.value("Nickname").toString();
        if (nick.isEmpty()) nick = u.value("DisplayName").toString();
        if (nick.isEmpty()) nick = u.value("DisplayId").toString();
        return nick.isEmpty() ? "某人" : nick;
    };

    if (type == 1) {  // 弹幕
        QString nickname = getNick(data);
        QString content = data.value("Content").toString().trimmed();
        if (content.isEmpty()) return;

        danmuList->addItem(QString("%1: %2").arg(nickname).arg(content));
        if (danmuList->count() > 80) danmuList->takeItem(0);
        danmuList->scrollToBottom();

        // 弹幕朗读开关
        if (danmuSwitch->isChecked()) {
            speak(QString("%1说：%2").arg(nickname).arg(content));
        }

    } else if (type == 5) {  // 礼物
        QString nickname = getNick(data);
        QString gift = data.value("GiftName").toString();
        if (gift.isEmpty()) gift = "礼物";
        int count = data.value("GiftCount").toInt();
        if (count == 0) count = data.value("RepeatCount").toInt();
        if (count == 0) count = 1;

        QString text = QString("感谢%1送出的%2%3个").arg(nickname).arg(gift).arg(count);
        danmuList->addItem(QString("[礼物] %1").arg(text));

        // 礼物朗读开关
        if (giftSwitch->isChecked()) {
            speak(text);
        }
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
