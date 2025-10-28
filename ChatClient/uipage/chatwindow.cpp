#include "chatwindow.h"
#include "ui_chatwindow.h"
#include "iteminfo.h"

#include "global.h"
#include "unit.h"
#include "myapp.h"
#include "databasemagr.h"

#include <QDateTime>
#include <QTimer>
#include <QTime>

#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QKeyEvent>
#include <QToolTip>

#include <QHostAddress>

#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#define DATE_TIME       QDateTime::currentDateTime().toString("yyyy/MM/dd hh:mm:ss")

ChatWindow::ChatWindow(QWidget *parent) :
    CustomMoveWidget(parent),
    ui(new Ui::ChatWindow)
{
    ui->setupUi(this);
    this->setAttribute(Qt::WA_DeleteOnClose);
    ui->widgetFileBoard->setVisible(false);

    m_nFileType = 0;

    m_model = new QStandardItemModel(this);
    m_model->setRowCount(3);
    ui->tableViewGroups->setModel(m_model);

    // 文件服务器
    m_tcpFileSocket = new ClientFileSocket(this);    

    connect(m_tcpFileSocket, SIGNAL(signamFileRecvOk(quint8,QString)), this, SLOT(SltFileRecvFinished(quint8,QString)));
    connect(m_tcpFileSocket, SIGNAL(signalUpdateProgress(quint64,quint64)),
            this, SLOT(SltUpdateProgress(quint64,quint64)));

    // 语音录制器
    m_audioRecorder = new AudioRecorder(this);
    m_bRecording = false;
    m_bSendingVoice = false;
    connect(m_audioRecorder, SIGNAL(signalFinished()), this, SLOT(SltVoiceRecordFinished()));

    QMenu *sendMenu = new QMenu(this);
    QAction *actionEnter     = sendMenu->addAction(QIcon(""), tr("按Enter键发送消息"));
    QAction *actionCtrlEnter = sendMenu->addAction(QIcon(""), tr("按Ctrl+Enter键发送消息"));

    // 设置互斥
    QActionGroup *actionGroup = new QActionGroup(this);
    actionGroup->addAction(actionEnter);
    actionGroup->addAction(actionCtrlEnter);

    // 设置可选
    actionEnter->setCheckable(true);
    actionCtrlEnter->setCheckable(true);

    // 默认配置
    actionCtrlEnter->setChecked(true);

    // 设置菜单
    ui->btnAction->setMenu(sendMenu);

    // 设置快捷键
    ui->btnSendMsg->setShortcut(QKeySequence("alt+s"));
    ui->btnClose->setShortcut(QKeySequence("alt+c"));

    // 关联信号槽
    connect(ui->btnWinClose, SIGNAL(clicked(bool)), this, SLOT(SltCloseWindow()));
    connect(ui->btnClose, SIGNAL(clicked(bool)), this, SLOT(SltCloseWindow()));
    connect(ui->widgetBubble, SIGNAL(signalDownloadFile(QString)), this, SLOT(SltDownloadFiles(QString)));
    connect(ui->widgetBubble, SIGNAL(signalRetryMessage(int,quint8,QString)), this, SLOT(SltRetryMessage(int,quint8,QString)));

    ui->textEditMsg->setFocus();
}

ChatWindow::~ChatWindow()
{
    delete ui;
}

/**
 * @brief ChatWindow::SetCell
 * 设置聊天属性
 * @param cell
 * @param type
 */
void ChatWindow::SetCell(QQCell *cell, const quint8 &type)
{
    m_cell = cell;
    ui->labelWinTitle->setText(QString("与 %1 聊天中").arg(cell->name));
    this->setWindowTitle(QString("与 %1 聊天中").arg(cell->name));

    m_nChatType = type;

    if (0 == type) {
        // 加载历史
        ui->widgetBubble->addItems(DataBaseMagr::Instance()->QueryHistory(m_cell->id, 10));
        ui->tableViewGroups->setVisible(false);
        ui->widgetFileInfo->setVisible(true);
        ui->widgetFileBoard->setVisible(false);
        // 链接文件服务器,方便下载文件
        m_tcpFileSocket->ConnectToServer(MyApp::m_strHostAddr, MyApp::m_nFilePort, m_cell->id);
    }
    else {
        ui->tableViewGroups->setVisible(true);
        ui->widgetFileInfo->setVisible(false);
        ui->widgetFileBoard->setVisible(true);
        // 添加群组人员
        m_model->setColumnCount(2);
        m_model->setRowCount(3);
        m_model->setHorizontalHeaderLabels(QStringList() << "好友" << "状态");
    }
}

QString ChatWindow::GetIpAddr() const
{
    return m_cell->ipaddr;
}

int ChatWindow::GetUserId() const
{
    return m_cell->id;
}

/**
 * @brief ChatWindow::AddMessage
 * 接受服务器转发过来的消息
 * @param json
 */
void ChatWindow::AddMessage(const QJsonValue &json)
{
    if (json.isObject()) {
        QJsonObject dataObj = json.toObject();
        int type = dataObj.value("type").toInt();
        QString strText = dataObj.value("msg").toString();
        QString strHead = dataObj.value("head").toString();

        // 如果有头像，则用自己的头像(群组消息的时候会附带头像图片)
        strHead = GetHeadPixmap(strHead);

        ItemInfo *itemInfo = new ItemInfo();
        itemInfo->SetName(0 == m_nChatType ? m_cell->name : dataObj.value("name").toString());
        itemInfo->SetDatetime(DATE_TIME);
        itemInfo->SetHeadPixmap(strHead.isEmpty() ? m_cell->iconPath : strHead);
        itemInfo->SetMsgType(type);
        itemInfo->SetText(strText);

        // 接收的文件（含语音识别）
        if (Files == type) {
            QString strSize = dataObj.value("size").toString();
            QString fileName = strText;
            QString fullPath = MyApp::m_strRecvPath + fileName;

            // 如果是语音文件，转换为 Audio 类型并保存路径
            if (fileName.endsWith(".wav", Qt::CaseInsensitive)) {
                itemInfo->SetMsgType(Audio);
                itemInfo->SetText("[语音消息]");
                itemInfo->SetFilePath(fullPath);
            } else {
                itemInfo->SetText(fullPath);
                itemInfo->SetFileSizeString(strSize);
            }
        }

        // 加入聊天窗口
        ui->widgetBubble->addItem(itemInfo);
        // 群组的聊天消息不保存
        if (0 != m_nChatType) return;

        // 添加聊天消息到历史记录
        DataBaseMagr::Instance()->AddHistoryMsg(m_cell->id, itemInfo);
    }
}

/**
 * @brief ChatWindow::UpdateUserStatus
 * 更新列表状态
 * @param json
 */
void ChatWindow::UpdateUserStatus(const QJsonValue &dataVal)
{
    if (ui->tableViewGroups->isVisible()) {
        // data 的 value 是数组
        if (dataVal.isArray()) {
            QJsonArray array = dataVal.toArray();
            int nSize = array.size();
            m_model->clear();
            m_model->setColumnCount(2);
            m_model->setRowCount(nSize - 1);
            m_model->setHorizontalHeaderLabels(QStringList() << "好友" << "状态");

            for (int i = 1; i < nSize; ++i) {
                QJsonObject jsonObj = array.at(i).toObject();

                int nStatus = jsonObj.value("status").toInt();

                m_model->setData(m_model->index(i - 1, 0), jsonObj.value("name").toString());
                m_model->setData(m_model->index(i - 1, 1), nStatus ==  OnLine ? QString("在线") : QString("离线"));
            }

            ui->tableViewGroups->setColumnWidth(0, 90);
            ui->tableViewGroups->setColumnWidth(1, 50);
        }

    }
}

/**
 * @brief ChatWindow::changeEvent
 * 翻译切换
 * @param e
 */
void ChatWindow::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

void ChatWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Return:
    {
        if (Qt::ControlModifier == event->modifiers()) {
            on_btnSendMsg_clicked();
        }
    }
        break;
    default:
        break;
    }

    QWidget::keyPressEvent(event);
}

/**
 * @brief ChatWindow::SltChatMessage
 * @param name
 * @param text
 */
void ChatWindow::SltChatMessage(const QString &text)
{
    if (text.isEmpty()) return;

    // 组织消息
    ItemInfo *itemInfo = new ItemInfo();
    itemInfo->SetName(m_cell->name);
    itemInfo->SetDatetime(DATE_TIME);
    itemInfo->SetHeadPixmap(m_cell->iconPath);
    itemInfo->SetText(text);

    // 加入聊天界面
    ui->widgetBubble->addItem(itemInfo);
    // 加入聊天记录
    DataBaseMagr::Instance()->AddHistoryMsg(m_cell->id, itemInfo);
}

/**
 * @brief ChatWindow::on_btnSendMsg_clicked
 */
void ChatWindow::on_btnSendMsg_clicked()
{
    QString text = ui->textEditMsg->toPlainText();
    // 把最后一个回车换行符删掉
    while (text.endsWith("\n")) {
        text.remove(text.length() - 2, 2);
    }

    // 判断消息是否为空
    if (text.isEmpty()) {
        QPoint point = ui->widgetMsg->mapToGlobal(ui->btnSendMsg->pos());

        QToolTip::showText(point, tr("发送消息内容不能为空，请重新输入！"));
        return;
    }

    // 构建json数据
    QJsonObject json;
    json.insert("id", MyApp::m_nId);
    json.insert("to", m_cell->id);
    json.insert("msg", text);
    json.insert("type", Text);
    // 生成本地消息ID与时间戳，便于送达匹配
    int msgId = int(QDateTime::currentMSecsSinceEpoch() % 2147483647);
    json.insert("msgId", msgId);
    json.insert("ts", QDateTime::currentMSecsSinceEpoch());

    // 发送消息
    Q_EMIT signalSendMessage(0 == m_nChatType ? SendMsg : SendGroupMsg, json);

    // 构建气泡消息
    ItemInfo *itemInfo = new ItemInfo();
    itemInfo->SetName(MyApp::m_strUserName);
    itemInfo->SetDatetime(DATE_TIME);
    itemInfo->SetHeadPixmap(MyApp::m_strHeadFile);
    itemInfo->SetText(text);
    itemInfo->SetOrientation(Right);
    itemInfo->SetMsgId(msgId);
    itemInfo->SetStatus(MsgPending);

    // 加入聊天界面
    ui->widgetBubble->addItem(itemInfo);
    // 清除输入框
    ui->textEditMsg->clear();

    // 群组消息不记录
    if (0 != m_nChatType) return;
    // 保存消息记录到数据库
    DataBaseMagr::Instance()->AddHistoryMsg(m_cell->id, itemInfo);
    // 启动ACK超时计时器
    StartAckTimer(msgId);
}

// 延迟关闭
void ChatWindow::SltCloseWindow()
{
    Q_EMIT signalClose();
    QTimer::singleShot(100, this, SLOT(close()));
}

/**
 * @brief ChatWindow::on_toolButton_7_clicked
 * 添加图片
 */
void ChatWindow::on_toolButton_7_clicked()
{
    // 群组消息不记录
    if (0 != m_nChatType) {
        QMessageBox::information(this, tr("提示"), tr("不支持群组文件传输"));
        return;
    }

    static QString s_strPath = MyApp::m_strAppPath;
    QString strFileName = QFileDialog::getOpenFileName(this,
                                                       tr("选择图片"),
                                                       s_strPath,
                                                       tr("图片(*.jpg;*.png;*.bmp)"));
    // 文件选择检测
    if (strFileName.isEmpty()) return;
    s_strPath = strFileName;

    // 构建json数据
    QJsonObject json;
    json.insert("id", MyApp::m_nId);
    json.insert("to", m_cell->id);
    json.insert("msg", strFileName);
    json.insert("type", Picture);
    int msgId = int(QDateTime::currentMSecsSinceEpoch() % 2147483647);
    json.insert("msgId", msgId);
    json.insert("ts", QDateTime::currentMSecsSinceEpoch());

    m_tcpFileSocket->StartTransferFile(strFileName);
    m_nFileType = SendPicture;

    // 发送消息
    Q_EMIT signalSendMessage(SendPicture, json);

    // 构建气泡消息
    ItemInfo *itemInfo = new ItemInfo();
    itemInfo->SetName(MyApp::m_strUserName);
    itemInfo->SetDatetime(DATE_TIME);
    itemInfo->SetHeadPixmap(MyApp::m_strHeadFile);
    itemInfo->SetText(strFileName);
    itemInfo->SetOrientation(Right);
    itemInfo->SetMsgType(Picture);
    itemInfo->SetMsgId(msgId);
    itemInfo->SetStatus(MsgPending);

    // 加入聊天界面
    ui->widgetBubble->addItem(itemInfo);

    // 群组消息不记录
    if (0 != m_nChatType) return;
    // 保存消息记录到数据库
    DataBaseMagr::Instance()->AddHistoryMsg(m_cell->id, itemInfo);
    // 启动ACK超时计时器
    StartAckTimer(msgId);
}

void ChatWindow::UpdateMessageStatus(int msgId, quint8 status)
{
    // 取消并清理该消息的超时计时器
    if (m_ackTimers.contains(msgId)) {
        QTimer *t = m_ackTimers.value(msgId);
        if (t) {
            t->stop();
            t->deleteLater();
        }
        m_ackTimers.remove(msgId);
    }
    ui->widgetBubble->updateMessageStatus(msgId, status);
    // 私聊消息持久化状态更新
    if (0 == m_nChatType) {
        DataBaseMagr::Instance()->UpdateMsgStatus(m_cell->id, msgId, status);
    }
}

/**
 * @brief ChatWindow::SltUpdateProgress
 * @param currSize
 * @param total
 */
void ChatWindow::SltUpdateProgress(quint64 bytes, quint64 total)
{
    if (SendPicture == m_nFileType) return;

    // 总时间
    int nTime = m_updateTime.elapsed();

    // 下载速度
    double dBytesSpeed = (bytes * 1000.0) / nTime;

   ui->lineEditCurrSize->setText(myHelper::CalcSize(bytes));
   ui->lineEditTotalSize->setText(myHelper::CalcSize(total));
   ui->lineEditRate->setText(myHelper::CalcSpeed(dBytesSpeed));

   ui->progressBar->setMaximum(total);
   ui->progressBar->setValue(bytes);

   ui->widgetFileBoard->setVisible(bytes < total);

   // 文件发送完成，发送消息给服务器，转发至对端
   if (bytes >= total && SendFile == m_nFileType) {
       // 生成本地消息ID与时间戳，便于送达匹配
       int msgId = int(QDateTime::currentMSecsSinceEpoch() % 2147483647);

       // 统一构建并发送 JSON（复用文件协议）
       QJsonObject json;
       json.insert("id", MyApp::m_nId);
       json.insert("to", m_cell->id);
       json.insert("msg", myHelper::GetFileNameWithExtension(m_strFileName));
       json.insert("size", "文件大小：" + myHelper::CalcSize(total));
       json.insert("type", Files);
       json.insert("msgId", msgId);
       json.insert("ts", QDateTime::currentMSecsSinceEpoch());
       Q_EMIT signalSendMessage(SendFile, json);

       // 构建气泡：根据是否为语音决定渲染类型
       ItemInfo *itemInfo = new ItemInfo();
       itemInfo->SetName(MyApp::m_strUserName);
       itemInfo->SetDatetime(DATE_TIME);
       itemInfo->SetHeadPixmap(MyApp::m_strHeadFile);
       itemInfo->SetOrientation(Right);
       itemInfo->SetMsgId(msgId);
       itemInfo->SetStatus(MsgPending);

       if (m_bSendingVoice) {
           // 语音消息气泡
           itemInfo->SetMsgType(Audio);
           itemInfo->SetText("[语音消息]");
           itemInfo->SetFilePath(m_strFileName);
       } else {
           // 普通文件气泡
           itemInfo->SetMsgType(Files);
           itemInfo->SetText(m_strFileName);
           itemInfo->SetFileSizeString(myHelper::CalcSize(total));
       }

       // 加入聊天界面
       ui->widgetBubble->addItem(itemInfo);

       // 保存消息记录到数据库（群组不记录）
       if (0 == m_nChatType) {
           DataBaseMagr::Instance()->AddHistoryMsg(m_cell->id, itemInfo);
           StartAckTimer(msgId);
       }

       // 复位语音发送标记
       m_bSendingVoice = false;
   }
}

/**
 * @brief ChatWindow::SltFileRecvFinished
 * @param type
 * @param filePath
 */
void ChatWindow::SltFileRecvFinished(const quint8 &type, const QString &filePath)
{
    if (filePath.isEmpty()) return;

    // 图片不显示
    if (filePath.endsWith(".png") || filePath.endsWith(".bmp") || filePath.endsWith(".jpg") || filePath.endsWith(".jpeg"))
    {
        qDebug() << "file" << filePath ;
        return;
    }

    // 构建气泡消息
    ItemInfo *itemInfo = new ItemInfo();
    itemInfo->SetName(m_cell->name);
    itemInfo->SetDatetime(DATE_TIME);
    itemInfo->SetHeadPixmap(m_cell->iconPath);
    itemInfo->SetText(QString("文件接收完成:\n") + filePath);
    itemInfo->SetMsgType(type);

    // 加入聊天界面
    ui->widgetBubble->addItem(itemInfo);
    // 这个表示接受文件完成，给用户提示的，不需要进行数据库保存
}

/**
 * @brief ChatWindow::on_toolButton_6_clicked
 */
void ChatWindow::on_toolButton_6_clicked()
{
}

// 查看历史记录
void ChatWindow::on_toolButton_clicked()
{
    ui->widgetBubble->addItems(DataBaseMagr::Instance()->QueryHistory(m_cell->id));
}

// 发送文件
void ChatWindow::on_btnSendFile_clicked()
{
    // 群组消息不记录
    if (0 != m_nChatType) {
        QMessageBox::information(this, tr("提示"), tr("不支持群组文件传输"));
        return;
    }

    // 选择文件
    static QString s_strPath = MyApp::m_strAppPath;
    QString strFileName = QFileDialog::getOpenFileName(this,
                                                       tr("选择文件"),
                                                       s_strPath,
                                                       tr("文件(*)"));

    if (strFileName.isEmpty()) return;
    s_strPath = strFileName;

    // 获取名字
    QFileInfo fileInfo(strFileName);
    m_strFileName = strFileName;
    QString strSize = "文件大小： ";
    strSize += myHelper::CalcSize(fileInfo.size());

    // 文件上传限制，不能超过1G
    if (fileInfo.size() > (1024 * 1024 * 1024)) {
        CMessageBox::Infomation(this, tr("上传文件过大！"));
        return;
    }

    // 开始传输文件
    m_tcpFileSocket->StartTransferFile(strFileName);

    // 开始计时
    m_updateTime.restart();
    m_nFileType = SendFile;
}

// 服务器下载文件
void ChatWindow::SltDownloadFiles(const QString &fileName)
{
    QJsonObject json;
    json.insert("from", MyApp::m_nId);
    json.insert("id", m_cell->id);
    json.insert("msg", fileName);

    m_tcpFileSocket->ConnectToServer(MyApp::m_strHostAddr, MyApp::m_nFilePort, m_cell->id);

    m_nFileType = GetFile;
    Q_EMIT signalSendMessage(GetFile, json);
}

void ChatWindow::on_toolButton_4_clicked()
{

}

/**
 * @brief ChatWindow::GetHeadPixmap
 * 获取用户头像
 * @param name
 * @return
 */
QString ChatWindow::GetHeadPixmap(const QString &name) const
{
    if (name.isEmpty()) return name;

    if (QFile::exists(MyApp::m_strHeadPath + name)) {
        return MyApp::m_strHeadPath + name;
    }

    return ":/resource/head/1.bmp";
}

void ChatWindow::StartAckTimer(int msgId)
{
    QTimer *timer = new QTimer(this);
    timer->setSingleShot(true);
    timer->setInterval(10000); // 10秒未收到ACK则视为失败
    m_ackTimers.insert(msgId, timer);
    connect(timer, SIGNAL(timeout()), this, SLOT(SltAckTimeout()));
    timer->start();
}

void ChatWindow::SltAckTimeout()
{
    // 通过 sender() 找到对应的 msgId
    QTimer *timer = qobject_cast<QTimer*>(sender());
    if (!timer) return;
    int targetMsgId = -1;
    QHash<int, QTimer*>::iterator it = m_ackTimers.begin();
    for (; it != m_ackTimers.end(); ++it) {
        if (it.value() == timer) {
            targetMsgId = it.key();
            break;
        }
    }

    if (targetMsgId != -1) {
        ui->widgetBubble->updateMessageStatus(targetMsgId, MsgFailed);
        if (0 == m_nChatType) {
            DataBaseMagr::Instance()->UpdateMsgStatus(m_cell->id, targetMsgId, MsgFailed);
        }
        m_ackTimers.remove(targetMsgId);
    }
    timer->deleteLater();
}

void ChatWindow::SltRetryMessage(int oldMsgId, quint8 msgType, const QString &content)
{
    // 仅处理私聊的重发逻辑
    if (0 != m_nChatType) {
        return;
    }

    // 新的本地消息ID
    int newMsgId = int(QDateTime::currentMSecsSinceEpoch() % 2147483647);

    // 统一将旧气泡状态改为Pending并替换msgId
    ui->widgetBubble->updateMessageStatus(oldMsgId, MsgPending);
    ui->widgetBubble->updateMessageId(oldMsgId, newMsgId);

    // 取消旧的ACK计时器（如果存在），并为新的msgId启动计时器
    if (m_ackTimers.contains(oldMsgId)) {
        QTimer *t = m_ackTimers.value(oldMsgId);
        if (t) {
            t->stop();
            t->deleteLater();
        }
        m_ackTimers.remove(oldMsgId);
    }

    // 按消息类型重发
    QJsonObject json;
    json.insert("id", MyApp::m_nId);
    json.insert("to", m_cell->id);
    json.insert("msgId", newMsgId);
    json.insert("ts", QDateTime::currentMSecsSinceEpoch());

    switch (msgType) {
    case Text:
    {
        json.insert("msg", content);
        json.insert("type", Text);
        Q_EMIT signalSendMessage(SendMsg, json);
        break;
    }
    case Picture:
    {
        // 重新上传图片并重发通知
        json.insert("msg", content);
        json.insert("type", Picture);
        m_tcpFileSocket->StartTransferFile(content);
        m_nFileType = SendPicture;
        Q_EMIT signalSendMessage(SendPicture, json);
        break;
    }
    case Files:
    {
        // 直接重发文件消息（假设文件已存在于服务器）
        QFileInfo fi(content);
        QString sizeStr = QString("文件大小：") + myHelper::CalcSize(fi.size());
        json.insert("msg", myHelper::GetFileNameWithExtension(content));
        json.insert("size", sizeStr);
        json.insert("type", Files);
        Q_EMIT signalSendMessage(SendFile, json);
        break;
    }
    default:
        return;
    }

    // 启动新的ACK超时计时器
    StartAckTimer(newMsgId);

    // 数据库：更新旧记录的msgId为新的msgId，并重置状态为Pending
    DataBaseMagr::Instance()->UpdateMsgId(m_cell->id, oldMsgId, newMsgId, MsgPending);
}

// 插入表情
void ChatWindow::on_toolButton_3_clicked()
{

}

// 语音录制按钮按下
void ChatWindow::on_btnVoiceRecord_pressed()
{
    if (m_bRecording) return;
    
    // 生成语音文件名
    QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
    m_strVoiceFile = MyApp::m_strRecordPath + "/voice_" + timestamp + ".wav";
    
    // 开始录音
    m_audioRecorder->startRecord(m_strVoiceFile);
    m_bRecording = true;
    
    // 更新按钮状态
    ui->btnVoiceRecord->setToolTip("松开发送");
    ui->btnVoiceRecord->setStyleSheet("background-color: #ff6b6b;");
}

// 语音录制按钮释放
void ChatWindow::on_btnVoiceRecord_released()
{
    if (!m_bRecording) return;
    
    // 停止录音
    m_audioRecorder->sltStopRecord();
    m_bRecording = false;
    
    // 恢复按钮状态
    ui->btnVoiceRecord->setToolTip("按住录音");
    ui->btnVoiceRecord->setStyleSheet("");
}

// 语音录制完成
void ChatWindow::SltVoiceRecordFinished()
{
    if (m_strVoiceFile.isEmpty()) return;
    
    // 检查文件是否存在且有效
    QFileInfo fileInfo(m_strVoiceFile);
    if (!fileInfo.exists() || fileInfo.size() < 1024) { // 小于1KB认为无效
        QToolTip::showText(ui->btnVoiceRecord->mapToGlobal(QPoint(0, 0)), "录音时间太短，请重新录制");
        return;
    }
    
    // 发送语音文件（复用文件发送机制）
    SendVoiceMessage(m_strVoiceFile);
}

// 发送语音消息
void ChatWindow::SendVoiceMessage(const QString &voiceFilePath)
{
    QFileInfo fileInfo(voiceFilePath);
    if (!fileInfo.exists()) return;

    // 设置发送环境并启动传输（进度回调统一创建气泡与发送消息）
    m_strFileName = voiceFilePath;
    m_updateTime.start();
    m_nFileType = SendFile;
    m_bSendingVoice = true;

    // 启动文件传输
    m_tcpFileSocket->SendFile(voiceFilePath, GetIpAddr(), Files);
}   //

}
