#include "listlikekey.h"

ListLikeKeyModel::ListLikeKeyModel(
    QSharedPointer<RedisClient::Connection> connection, QByteArray fullPath,
    int dbIndex, long long ttl, QByteArray rowsCountCmd, QByteArray rowsLoadCmd)
    : KeyModel(connection, fullPath, dbIndex, ttl, rowsCountCmd, rowsLoadCmd) {}

QStringList ListLikeKeyModel::getColumnNames() {
  return QStringList() << "row"
                       << "value";
}

QHash<int, QByteArray> ListLikeKeyModel::getRoles() {
  QHash<int, QByteArray> roles;
  roles[Roles::Value] = "value";
  roles[Roles::RowNumber] = "row";
  return roles;
}

QVariant ListLikeKeyModel::getData(int rowIndex, int dataRole) {
  if (!isRowLoaded(rowIndex)) return QVariant();

  switch (dataRole) {
    case Value:
      return m_rowsCache[rowIndex];
    case RowNumber:
      return rowIndex + 1;
  }

  return QVariant();
}

int ListLikeKeyModel::addLoadedRowsToCache(const QVariantList &rows,
                                           QVariant rowStartId) {
  QList<QByteArray> result;
  auto rowStart = rowStartId.toLongLong();

  foreach (QVariant row, rows) { result.push_back(row.toByteArray()); }

  m_rowsCache.addLoadedRange({rowStart, rowStart + result.size() - 1}, result);
  return result.size();
}
