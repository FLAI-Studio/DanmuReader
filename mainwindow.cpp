#include "mainwindow.h"

#include <QDir>
#include <QThread>
#include <QGroupBox>
#include <QGridLayout>
#include <QSettings>
#include <QCoreApplication>

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
    // 先释放旧的，防止多次 new 泄漏
    stopGateway();

    QString gatewayDir = QCoreApplication::applicationDirPath() + "/gateway";
    QDir dir(gatewayDir);
    QStringList exes = dir.entryList(QStringList() << "*.exe", QDir::Files);

    if (exes.isEmpty()) {
        log("⚠️ gateway 文件夹下没有找到任何 exe 文件");
        log("请确认已将网关程序解压到: " + gatewayDir);
        return;
    }

    QString gatewayExe;
    for (const QString& exe : exes) {
        if (exe.contains("Wss", Qt::CaseInsensitive) ||
            exe.contains("Barrage", Qt::CaseInsensitive) ||
            exe.contains("Grab", Qt::CaseInsensitive)) {
            gatewayExe = gatewayDir + "/" + exe;
            break;
        }
    }
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
    if (gatewayProcess) {
        if (gatewayProcess->state() == QProcess::Running) {
            gatewayProcess->kill();
            gatewayProcess->waitForFinished(3000);
        }
        gatewayProcess->deleteLater();
        gatewayProcess = nullptr;
    }
}

/**
 * @brief 入队朗读（防刷屏）
 */
void MainWindow::enqueueSpeak(const QString& text) {
    ttsQueue.enqueue(text);
    if (ttsQueue.size() > 10) ttsQueue.dequeue();
}

void MainWindow::onTtsTimerTimeout() {
    if (!ttsQueue.isEmpty() && tts->state() != QTextToSpeech::Speaking) {
        tts->say(ttsQueue.dequeue());
    }
}

void MainWindow::onReconnectTimerTimeout() {
    if (connected) { reconnectTimer->stop(); return; }
    reconnectAttempts++;
    if (reconnectAttempts > 10) {
        log("重连次数过多，停止自动重连");
        reconnectTimer->stop();
        return;
    }
    log(QString("尝试第 %1 次重连...").arg(reconnectAttempts));
    webSocket->open(QUrl(urlInput->text().trimmed()));
}

void MainWindow::onGatewayWatchdogTimeout() {
    if (!gatewayProcess) return;
    if (gatewayProcess->state() != QProcess::Running) {
        log("检测到网关进程已退出，正在重新启动...");
        startGateway();
    }
}

void MainWindow::onAlwaysOnTopToggled(bool checked) {
    setWindowFlag(Qt::WindowStaysOnTopHint, checked);
    show();
    log(checked ? "窗口已置顶" : "窗口已取消置顶");
}

void MainWindow::onOpacityChanged(int value) {
    setWindowOpacity(value / 100.0);
    opacityLabel->setText(QString::number(value) + "%");
}

void MainWindow::saveConfig() {
    QSettings settings("Byjsmc", "DouyinDanmuTTS");
    settings.setValue("url", urlInput->text());
    settings.setValue("rate", rateSlider->value());
    settings.setValue("volume", volumeSlider->value());
    settings.setValue("voiceIndex", voiceCombo->currentIndex());
    settings.setValue("danmuEnabled", danmuSwitch->isChecked());
    settings.setValue("giftEnabled", giftSwitch->isChecked());
    settings.setValue("enterEnabled", enterSwitch->isChecked());
    settings.setValue("alwaysOnTop", alwaysOnTopCheck->isChecked());
    settings.setValue("opacity", opacitySlider->value());
    settings.setValue("danmuTemplate", danmuTemplateInput->text());
    settings.setValue("giftTemplate", giftTemplateInput->text());
}

void MainWindow::loadConfig() {
    QSettings settings("Byjsmc", "DouyinDanmuTTS");
    urlInput->setText(settings.value("url", "ws://127.0.0.1:8888").toString());
    rateSlider->setValue(settings.value("rate", 50).toInt());
    volumeSlider->setValue(settings.value("volume", 50).toInt());

    int savedVoice = settings.value("voiceIndex", 0).toInt();
    if (savedVoice >= 0 && savedVoice < voices.size())
        voiceCombo->setCurrentIndex(savedVoice);
    else
        voiceCombo->setCurrentIndex(0);

    danmuSwitch->setChecked(settings.value("danmuEnabled", true).toBool());
    giftSwitch->setChecked(settings.value("giftEnabled", true).toBool());
    enterSwitch->setChecked(settings.value("enterEnabled", true).toBool());
    alwaysOnTopCheck->setChecked(settings.value("alwaysOnTop", false).toBool());
    opacitySlider->setValue(settings.value("opacity", 100).toInt());
    danmuTemplateInput->setText(settings.value("danmuTemplate", "%1说：%2").toString());
    giftTemplateInput->setText(settings.value("giftTemplate", "感谢%1送出的%2%3个").toString());

    tts->setRate(rateSlider->value() / 100.0);
    tts->setVolume(volumeSlider->value() / 100.0);
    if (voiceCombo->currentIndex() >= 0 && voiceCombo->currentIndex() < voices.size())
        tts->setVoice(voices[voiceCombo->currentIndex()]);
    setWindowOpacity(opacitySlider->value() / 100.0);

    danmuTpl = danmuTemplateInput->text();
    giftTpl = giftTemplateInput->text();
}

/**
 * @brief 主窗口构造函数
 * 初始化 UI、WebSocket 连接和 TTS 引擎
 */
MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), connected(false), gatewayProcess(nullptr),
    waitingForGateway(false), reconnectAttempts(0), manualDisconnect(false) {

    // ========== 窗口基本设置 ==========
    setWindowTitle("抖音弹幕朗读助手");
    resize(550, 750);

    // ========== TTS 引擎初始化 ==========
    tts = new QTextToSpeech(this);
    voices = tts->availableVoices();

    // ========== WebSocket 初始化 ==========
    webSocket = new QWebSocket();

    // 定时器初始化
    ttsTimer = new QTimer(this);
    ttsTimer->setInterval(2000);
    reconnectTimer = new QTimer(this);
    reconnectTimer->setInterval(5000);
    gatewayWatchdogTimer = new QTimer(this);
    gatewayWatchdogTimer->setInterval(10000);

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
    enterSwitch = new QCheckBox("进房播报");  // 新增
    danmuSwitch->setChecked(true);
    giftSwitch->setChecked(true);
    enterSwitch->setChecked(true);             // 新增
    switchLayout->addWidget(danmuSwitch);
    switchLayout->addWidget(giftSwitch);
    switchLayout->addWidget(enterSwitch);      // 新增
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
    // 新增：音量
    ttsLayout->addWidget(new QLabel("音量："), 2, 0);
    volumeSlider = new QSlider(Qt::Horizontal);
    volumeSlider->setRange(0, 100);
    volumeSlider->setValue(50);
    volumeLabel = new QLabel("50");
    volumeLabel->setFixedWidth(30);
    ttsLayout->addWidget(volumeSlider, 2, 1);
    ttsLayout->addWidget(volumeLabel, 2, 2);
    layout->addWidget(ttsGroup);

    // 朗读模板
    QGroupBox* tplGroup = new QGroupBox("朗读模板");
    QGridLayout* tplLayout = new QGridLayout(tplGroup);
    tplLayout->addWidget(new QLabel("弹幕模板："), 0, 0);
    danmuTemplateInput = new QLineEdit("%1说：%2");
    danmuTemplateInput->setPlaceholderText("%1=昵称 %2=内容");
    tplLayout->addWidget(danmuTemplateInput, 0, 1);
    tplLayout->addWidget(new QLabel("礼物模板："), 1, 0);
    giftTemplateInput = new QLineEdit("感谢%1送出的%2%3个");
    giftTemplateInput->setPlaceholderText("%1=昵称 %2=礼物名 %3=数量");
    tplLayout->addWidget(giftTemplateInput, 1, 1);
    layout->addWidget(tplGroup);

    // 窗口设置
    QGroupBox* winGroup = new QGroupBox("窗口设置");
    QGridLayout* winLayout = new QGridLayout(winGroup);
    alwaysOnTopCheck = new QCheckBox("窗口置顶");
    winLayout->addWidget(alwaysOnTopCheck, 0, 0);
    winLayout->addWidget(new QLabel("透明度："), 0, 1);
    opacitySlider = new QSlider(Qt::Horizontal);
    opacitySlider->setRange(30, 100);
    opacitySlider->setValue(100);
    opacityLabel = new QLabel("100%");
    opacityLabel->setFixedWidth(40);
    winLayout->addWidget(opacitySlider, 0, 2);
    winLayout->addWidget(opacityLabel, 0, 3);
    layout->addWidget(winGroup);

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
        "版本：v0.2.0\n"
        "开发者：Byjsmc\n"
        "最后更新于：2026/09/05"
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
    // 音量滑块
    connect(volumeSlider, &QSlider::valueChanged, this, [this](int v) {
        volumeLabel->setText(QString::number(v));
        tts->setVolume(v / 100.0);
    });
    // 音色选择
    connect(voiceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx >= 0 && idx < voices.size()) tts->setVoice(voices[idx]);
    });
    // 窗口设置
    connect(alwaysOnTopCheck, &QCheckBox::toggled, this, &MainWindow::onAlwaysOnTopToggled);
    connect(opacitySlider, &QSlider::valueChanged, this, &MainWindow::onOpacityChanged);

    // 防刷屏定时器
    connect(ttsTimer, &QTimer::timeout, this, &MainWindow::onTtsTimerTimeout);
    ttsTimer->start();
    // 重连定时器
    connect(reconnectTimer, &QTimer::timeout, this, &MainWindow::onReconnectTimerTimeout);
    // 网关守护定时器
    connect(gatewayWatchdogTimer, &QTimer::timeout, this, &MainWindow::onGatewayWatchdogTimeout);
    gatewayWatchdogTimer->start();

    // 加载配置
    loadConfig();
    setWindowFlag(Qt::WindowStaysOnTopHint, alwaysOnTopCheck->isChecked());

    // 模板字符串
    danmuTpl = danmuTemplateInput->text();
    giftTpl = giftTemplateInput->text();
    connect(danmuTemplateInput, &QLineEdit::editingFinished, this, [this]() {
        danmuTpl = danmuTemplateInput->text();
    });
    connect(giftTemplateInput, &QLineEdit::editingFinished, this, [this]() {
        giftTpl = giftTemplateInput->text();
    });

    log("程序已启动，请先启动弹幕网关再点击连接");
    show();
}

MainWindow::~MainWindow() {
    saveConfig();
    reconnectTimer->stop();
    ttsTimer->stop();
    gatewayWatchdogTimer->stop();
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
        waitingForGateway = true;
        // 异步等待，不阻塞 UI
        QTimer::singleShot(3000, this, [this, url]() {
            waitingForGateway = false;
            log(QString("正在连接 %1...").arg(url));
            webSocket->open(QUrl(url));
        });
        return;
    }

    log(QString("正在连接 %1...").arg(url));
    webSocket->open(QUrl(url));
}

void MainWindow::onDisconnectClicked() {
    manualDisconnect = true;
    reconnectTimer->stop();
    webSocket->close();
    log("已断开连接");
}

void MainWindow::onWebSocketConnected() {
    connected = true;
    reconnectAttempts = 0;
    reconnectTimer->stop();
    connectBtn->setEnabled(false);
    disconnectBtn->setEnabled(true);
    log("✅ 已连接到弹幕服务");
    enqueueSpeak("已连接到弹幕服务");
}

void MainWindow::onWebSocketDisconnected() {
    connected = false;
    connectBtn->setEnabled(true);
    disconnectBtn->setEnabled(false);

    if (manualDisconnect) {
        manualDisconnect = false;
        log("已手动断开，不自动重连");
        return;
    }

    log("❌ 连接已断开");
    enqueueSpeak("连接已断开");
    reconnectAttempts = 0;
    reconnectTimer->start();
}

void MainWindow::onWebSocketError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error);
    QString errMsg = QString("连接错误: %1 ，请检查网关是否正常运行以及程序是否为管理员身份运行").arg(webSocket->errorString());
    log(errMsg);
    enqueueSpeak("连接错误，请检查网关是否正常运行以及程序是否为管理员身份运行");
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

        if (danmuSwitch->isChecked()) {
            QString text = danmuTpl;
            text.replace("%1", nickname);
            text.replace("%2", content);
            enqueueSpeak(text);
        }

    } else if (type == 5) {  // 礼物
        QString nickname = getNick(data);
        QString gift = data.value("GiftName").toString();
        if (gift.isEmpty()) gift = "礼物";
        int count = data.value("GiftCount").toInt();
        if (count == 0) count = data.value("RepeatCount").toInt();
        if (count == 0) count = 1;

        QString text = giftTpl;
        text.replace("%1", nickname);
        text.replace("%2", gift);
        text.replace("%3", QString::number(count));
        danmuList->addItem(QString("[礼物] %1").arg(text));

        if (giftSwitch->isChecked()) {
            enqueueSpeak(text);
        }

    } else if (type == 3) {  // 进入直播间
        QString nickname = getNick(data);
        if (nickname.isEmpty() || nickname == "某人") return;

        danmuList->addItem(QString("[进房] %1 进入了直播间").arg(nickname));
        if (danmuList->count() > 80) danmuList->takeItem(0);
        danmuList->scrollToBottom();

        if (enterSwitch->isChecked()) {
            enqueueSpeak(QString("%1 进入了直播间").arg(nickname));
        }
    }
}

void MainWindow::speak(const QString& text) {
    tts->say(text);
}

void MainWindow::log(const QString& msg) {
    QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    logView->append(QString("[%1] %2").arg(ts).arg(msg));
    logView->verticalScrollBar()->setValue(logView->verticalScrollBar()->maximum());
}
