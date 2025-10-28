#include "databasemagr.h"
#include "unit.h"
#include "iteminfo.h"

#include <QDebug>

DataBaseMagr *DataBaseMagr::self = NULL;

DataBaseMagr::DataBaseMagr(QObject *parent) :
    QObject(parent)
{

}

DataBaseMagr::~DataBaseMagr()
{
    if (userdb.isOpen()) {
        userdb.close();
    }

    if (msgdb.isOpen()) {
        msgdb.close();
    }
}

/**
 * @brief DataBaseMagr::OpenDb
 * 打开数据库
 */
bool DataBaseMagr::OpenUserDb(const QString &dataName)
{
    userdb = QSqlDatabase::addDatabase("QSQLITE", "connectionUser");
    userdb.setDatabaseName(dataName);
    if (!userdb.open()) {
        qDebug() << "Open sql failed";
        return false;
    }

    // 添加数据表
    QSqlQuery query(userdb);

    // 创建我的好友表 id为好友id，userid为当前用户id
    query.exec("CREATE TABLE FRIEND (id INT, userId INT, name varchar(50))");

    // 创建群组表 id为群组id，userid为当前用户id
    query.exec("CREATE TABLE MYGROUP (id INT, userId INT, name varchar(50))");

    // 用户数据保存
    query.exec("CREATE TABLE USERINFO (id INT, name varchar(50), passwd varchar(50))");

    return true;
}

/**
 * @brief DataBaseMagr::OpenMessageDb
 * 消息历史记录数据库
 * @param dataName
 * @return
 */
bool DataBaseMagr::OpenMessageDb(const QString &dataName)
{
    msgdb = QSqlDatabase::addDatabase("QSQLITE", "connectionMsg");
    msgdb.setDatabaseName(dataName);
    if (!msgdb.open()) {
        qDebug() << "Open sql failed";
        return false;
    }

    // 添加数据表
    QSqlQuery query(msgdb);
    // 创建历史聊天表（新增 msgId 与 status 字段用于持久化消息状态）
    query.exec("CREATE TABLE MSGINFO (id INT PRIMARY KEY, userId INT, name varchar(20),"
               "head varchar(50), datetime varchar(20), filesize varchar(30),"
               "content varchar(500), type INT, direction INT, msgId INT, status INT)");

    // 为常用查询添加索引，加速根据 userId + msgId 的定位
    query.exec("CREATE INDEX IF NOT EXISTS idx_msginfo_user_msgid ON MSGINFO(userId, msgId)");

    // 迁移：为已有表补充列（重复执行无害，失败可忽略）
    QSqlQuery alterQuery(msgdb);
    alterQuery.exec("ALTER TABLE MSGINFO ADD COLUMN msgId INT DEFAULT 0;");
    alterQuery.exec("ALTER TABLE MSGINFO ADD COLUMN status INT DEFAULT 0;");

    // 迁移后再次确保索引存在
    alterQuery.exec("CREATE INDEX IF NOT EXISTS idx_msginfo_user_msgid ON MSGINFO(userId, msgId)");

    return true;
}

/**
 * @brief DataBaseMagr::AddHistoryMsg
 * 添加历史记录
 * @param userId
 * @param itemInfo
 */
void DataBaseMagr::AddHistoryMsg(const int &userId, ItemInfo *itemInfo)
{
    // 查询数据库
    QSqlQuery query("SELECT [id] FROM MSGINFO ORDER BY id DESC;", msgdb);
    int nId = 0;
    // 查询最高ID
    if (query.next()) {
        nId = query.value(0).toInt();
    }

    // 根据新ID重新创建用户
    query.prepare("INSERT INTO MSGINFO (id, userId, name, head, datetime, filesize, content, type, direction, msgId, status) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);");

    query.bindValue(0, nId + 1);
    query.bindValue(1, userId);
    query.bindValue(2, itemInfo->GetName());
    query.bindValue(3, itemInfo->GetStrPixmap());
    query.bindValue(4, itemInfo->GetDatetime());
    // 注意字段顺序：filesize, content, type, direction
    query.bindValue(5, itemInfo->GetFileSizeString());
    query.bindValue(6, itemInfo->GetText());
    query.bindValue(7, itemInfo->GetMsgType());
    query.bindValue(8, itemInfo->GetOrientation());
    query.bindValue(9, itemInfo->GetMsgId());
    query.bindValue(10, itemInfo->GetStatus());

    query.exec();
}

/**
 * @brief DataBaseMagr::AddFriend
 * 添加好友
 * @param id
 * @param name
 */
void DataBaseMagr::AddFriend(const int &id, const int &userId, const QString &name)
{
    QString strQuery = "SELECT [id] FROM FRIEND ";
    strQuery.append("WHERE id=");
    strQuery.append(QString::number(id));
    strQuery.append(" AND userId=");
    strQuery.append(QString::number(userId));

    QSqlQuery query(strQuery, userdb);
    if (query.next()) {
        // 查询到有该用户，不添加
        qDebug() << "yes" << query.value(0).toString();
        return;
    }

    // 根据新ID重新创建用户
    query.prepare("INSERT INTO FRIEND (id, userid, name) "
                  "VALUES (?, ?, ?);");
    query.bindValue(0, id);
    query.bindValue(1, userId);
    query.bindValue(2, name);

    query.exec();
}

/**
 * @brief DataBaseMagr::AddGroup
 * 添加我的群组
 * @param id
 * @param name
 */
void DataBaseMagr::AddGroup(const int &id, const int &userId, const QString &name)
{
    QString strQuery = "SELECT [id] FROM MYGROUP ";
    strQuery.append("WHERE id=");
    strQuery.append(QString::number(id));
    strQuery.append(" AND userId=");
    strQuery.append(QString::number(userId));

    QSqlQuery query(strQuery, userdb);
    if (query.next()) {
        // 查询到有该用户，不添加
        qDebug() << "yes" << query.value(0).toString();
        return;
    }

    // 根据新ID重新创建用户
    query.prepare("INSERT INTO MYGROUP (id, userId, name) "
                  "VALUES (?, ?, ?);");
    query.bindValue(0, id);
    query.bindValue(1, userId);
    query.bindValue(2, name);

    query.exec();
}

/**
 * @brief DataBaseMagr::DeleteMyFriend
 * 删除好友
 * @param id
 * @param userId
 * @return
 */
bool DataBaseMagr::DeleteMyFriend(const int &id, const int &userId)
{
    QString strQuery = "SELECT [id] FROM FRIEND ";
    strQuery.append("WHERE id=");
    strQuery.append(QString::number(id));
    strQuery.append(" AND userId=");
    strQuery.append(QString::number(userId));

    QSqlQuery query(strQuery, userdb);
    // 删除
    if (query.next()) {
        strQuery = "DELETE FROM FRIEND WHERE id=";
        strQuery.append(QString::number(id));
        strQuery.append(" AND userId=");
        strQuery.append(QString::number(userId));
        query = QSqlQuery(strQuery, userdb);
        return query.exec();
    }

    // 没有查询到有该用户
    return false;
}

/**
 * @brief DataBaseMagr::GetMyFriend
 * 获取我的好友
 * @return
 */
QJsonArray DataBaseMagr::GetMyFriend(const int &userId) const
{
    QJsonArray  myFriends;

    QString strQuery = "SELECT [id] FROM FRIEND ";
    strQuery.append("WHERE userId=");
    strQuery.append(QString::number(userId));

    QSqlQuery query(strQuery, userdb);
    while (query.next()) {
        myFriends.append(query.value("id").toInt());
    }

    return myFriends;
}

/**
 * @brief DataBaseMagr::GetMyGroup
 * 获取我所在的群组
 * @return
 */
QJsonArray DataBaseMagr::GetMyGroup(const int &userId) const
{
    QJsonArray  myGroup;

    QString strQuery = "SELECT [id],[name] FROM MYGROUP ";
    strQuery.append("WHERE userId=");
    strQuery.append(QString::number(userId));

    QSqlQuery query(strQuery, userdb);
    while (query.next()) {
        QJsonObject jsonObj;
        jsonObj.insert("id", query.value(0).toInt());
        jsonObj.insert("name", query.value(1).toString());
        myGroup.append(jsonObj);
    }

    return myGroup;
}

/**
 * @brief DataBaseMagr::isMyFriend
 * 添加好友时进行判断，该用户是否已经是我的好友
 * @param name
 * @return
 */
bool DataBaseMagr::isMyFriend(const int &userId, const QString &name)
{
    QString strQuery = "SELECT [id] FROM FRIEND ";
    strQuery.append("WHERE name='");
    strQuery.append(name);
    strQuery.append("' AND userId='");
    strQuery.append(QString::number(userId));
    strQuery.append("';");

    QSqlQuery query(strQuery, userdb);
    return query.next();
}

/**
 * @brief DataBaseMagr::isInGroup
 * 我是否已经添加该群组了
 * @param name
 * @return
 */
bool DataBaseMagr::isInGroup(const QString &name)
{
    QString strQuery = "SELECT [id] FROM MYGROUP ";
    strQuery.append("WHERE name='");
    strQuery.append(name);
    strQuery.append("';");

    QSqlQuery query(strQuery, userdb);
    return query.next();
}

/**
 * @brief DataBaseMagr::QueryHistory
 * 查询指定用户的历史记录
 * @param id
 */
QVector<ItemInfo*> DataBaseMagr::QueryHistory(const int &id, const int &count)
{
    QString strQuery = "SELECT * FROM MSGINFO ";
    strQuery.append("WHERE userId=");
    strQuery.append(QString::number(id));
    strQuery.append(" ORDER BY id ASC ");
    // 查询前10条
    if (0 != count)
    {
        strQuery.append(" LIMIT ");
        strQuery.append(QString::number(count));
    }
    strQuery.append(";");

    QVector<ItemInfo*> items;

    QSqlQuery query(strQuery, msgdb);
    while (query.next()) {
        // 查看历史记录
        ItemInfo *info = new ItemInfo(
                             query.value(2).toString(),        // name
                             query.value(4).toString(),        // datetime
                             query.value(3).toString(),        // head
                             query.value(6).toString(),        // content(text)
                             query.value(5).toString(),        // filesize
                             query.value(8).toInt(),           // direction(orientation)
                             query.value(7).toInt());          // type(msgType)
        info->SetMsgId(query.value(9).toInt());
        info->SetStatus((quint8)query.value(10).toInt());
        items.push_front(info);
    }

    return items;
}

/**
 * @brief DataBaseMagr::QueryAll
 * 打印数据库信息
 */
void DataBaseMagr::QueryAll() {
    QSqlQuery query("SELECT * FROM FRIEND ORDER BY id;", userdb);

    while (query.next()) {
        qDebug() << query.value(0).toInt() << query.value(1).toString() << query.value(2).toString();
    }

    query = QSqlQuery("SELECT * FROM MSGINFO ORDER BY id", msgdb);

    while (query.next()) {
        qDebug() << query.value(0).toInt() << query.value(1).toInt()
                 << query.value(2).toString() << query.value(3).toString()
                 << query.value(4).toString() << query.value(5).toString()
                 << query.value(6).toString() << query.value(7).toInt()
                 << query.value(8).toInt() << query.value(9).toInt()
                 << query.value(10).toInt();

    }

    // 输出结束
}

void DataBaseMagr::UpdateMsgStatus(const int &userId, const int &msgId, const quint8 &status)
{
    QString strSql = "UPDATE MSGINFO SET status=" + QString::number(status) +
                     " WHERE userId=" + QString::number(userId) +
                     " AND msgId=" + QString::number(msgId) + ";";
    QSqlQuery query(strSql, msgdb);
    query.exec();
}

void DataBaseMagr::UpdateMsgId(const int &userId, const int &oldMsgId, const int &newMsgId, const quint8 &status)
{
    QString strSql = "UPDATE MSGINFO SET msgId=" + QString::number(newMsgId) +
                     ", status=" + QString::number(status) +
                     " WHERE userId=" + QString::number(userId) +
                     " AND msgId=" + QString::number(oldMsgId) + ";";
    QSqlQuery query(strSql, msgdb);
    query.exec();
}
}
