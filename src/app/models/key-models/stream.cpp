#include "stream.h"
#include <QJsonDocument>

StreamKeyModel::StreamKeyModel(
    QSharedPointer<RedisClient::Connection> connection, QByteArray fullPath,
    int dbIndex, long long ttl)
    : KeyModel(connection, fullPath, dbIndex, ttl, "XLEN", QByteArray()) {}

QString StreamKeyModel::type() { return "stream"; }

QStringList StreamKeyModel::getColumnNames() {
  return QStringList() << "row"
                       << "id"
                       << "value";
}

QHash<int, QByteArray> StreamKeyModel::getRoles() {
  QHash<int, QByteArray> roles;
  roles[Roles::RowNumber] = "row";
  roles[Roles::ID] = "id";
  roles[Roles::Value] = "value";
  return roles;
}

QVariant StreamKeyModel::getData(int rowIndex, int dataRole) {
  if (!isRowLoaded(rowIndex)) return QVariant();

  switch (dataRole) {
    case Value:
      return QJsonDocument::fromVariant(m_rowsCache[rowIndex].second)
          .toJson(QJsonDocument::Compact);
    case ID:
      return m_rowsCache[rowIndex].first;
    case RowNumber:
      return rowIndex + 1;
  }

  return QVariant();
}

void StreamKeyModel::addRow(const QVariantMap &row,
                            ValueEditor::Model::Callback c) {
  if (!isRowValid(row)) {
    c(QCoreApplication::translate("RDM", "Invalid row"));
    return;
  }

  QList<QByteArray> cmd = {"XADD", m_keyFullPath, row["id"].toByteArray()};

  QJsonParseError err;
  QJsonDocument jsonValues =
      QJsonDocument::fromJson(row["value"].toByteArray(), &err);

  if (err.error != QJsonParseError::NoError || !jsonValues.isObject()) {
    return c(QCoreApplication::translate("RDM", "Invalid row"));
  }

  auto valuesObject = jsonValues.object();

  for (auto key : valuesObject.keys()) {
    cmd.append(key.toUtf8());
    cmd.append(valuesObject[key].toVariant().toString().toUtf8());
  }

  executeCmd(cmd, c);
}

void StreamKeyModel::updateRow(int rowIndex, const QVariantMap &,
                               ValueEditor::Model::Callback) {
    //NOTE(u_glide): Redis Streams doesn't support editing (yet?)
}

void StreamKeyModel::removeRow(int i, ValueEditor::Model::Callback c) {
  if (!isRowLoaded(i)) return;

  executeCmd({"XDEL", m_keyFullPath, m_rowsCache[i].first}, c);
}

int StreamKeyModel::addLoadedRowsToCache(const QVariantList &rows,
                                         QVariant rowStartId) {
  QList<QPair<QByteArray, QVariant>> result;

  for (QVariantList::const_iterator item = rows.begin(); item != rows.end();
       ++item) {
    QPair<QByteArray, QVariant> value;
    auto rowValues = item->toList();
    value.first = rowValues[0].toByteArray();

    QVariantList valuesList = rowValues[1].toList();
    QVariantMap mappedVal;

    for (QVariantList::const_iterator valItem = valuesList.begin();
         valItem != valuesList.end(); ++valItem) {
      auto valKey = valItem->toByteArray();
      valItem++;

      mappedVal[valKey] = valItem->toByteArray();
    }

    value.second = mappedVal;
    result.push_back(value);
  }

  auto rowStart = rowStartId.toLongLong();
  m_rowsCache.addLoadedRange({rowStart, rowStart + result.size() - 1}, result);

  return result.size();
}

QList<QByteArray> StreamKeyModel::getRangeCmd(QVariant rowStartId,
                                              unsigned long count) {
  QList<QByteArray> cmd;

  unsigned long rowStart = rowStartId.toULongLong();

  QByteArray startFrom = "+";

  if (isRowLoaded(rowStart - 1)) {
    startFrom = m_rowsCache[rowStart - 1].first;
  }

  return (cmd << "XREVRANGE" << m_keyFullPath << startFrom << "-"
              << "COUNT" << QString::number(count).toLatin1());
}
