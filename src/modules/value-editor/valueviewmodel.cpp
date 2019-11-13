#include "valueviewmodel.h"
#include <qredisclient/utils/text.h>
#include <QCoreApplication>
#include <QDebug>
#include <QQmlEngine>
#include <QSettings>

ValueEditor::ValueViewModel::ValueViewModel(QSharedPointer<Model> model)
    : BaseListModel(),
      m_model(model),
      m_startFramePosition(0),
      m_lastLoadedRowFrameSize(0),
      m_singlePageMode(false) {}

int ValueEditor::ValueViewModel::rowCount(const QModelIndex& parent) const {
  Q_UNUSED(parent);

  if (m_singlePageMode) {
    return m_model->rowsCount();
  } else {
    return m_lastLoadedRowFrameSize;
  }
}

QVariant ValueEditor::ValueViewModel::data(const QModelIndex& index,
                                           int role) const {
  if (!isIndexValid(index)) return QVariant();

  return m_model->getData(m_startFramePosition + index.row(), role);
}

QHash<int, QByteArray> ValueEditor::ValueViewModel::roleNames() const {
  return m_model->getRoles();
}

QSharedPointer<ValueEditor::Model> ValueEditor::ValueViewModel::model() {
  return m_model;
}

void ValueEditor::ValueViewModel::renameKey(const QString& newKeyName) {
  m_model->setKeyName(printableStringToBinary(newKeyName),
                      [this](const QString& err) {
                        if (err.size() > 0) {
                          emit error(err);
                          return;
                        }

                        emit keyRenamed();
                      });
}

void ValueEditor::ValueViewModel::setTTL(const QString& newTTL) {
  m_model->setTTL(newTTL.toLong(), [this](const QString& err) {
    if (err.size() > 0) {
      emit error(err);
      return;
    }

    emit keyTTLChanged();
  });
}

void ValueEditor::ValueViewModel::removeKey() {
  m_model->removeKey([this](const QString& err) {
    if (err.size() > 0) {
      emit error(err);
      return;
    }

    emit keyRemoved();
  });
}

int ValueEditor::ValueViewModel::mapRowIndex(int i) {
  return m_startFramePosition + i;
}

QVariantList ValueEditor::ValueViewModel::columnNames() {
  QVariantList result;

  foreach (QString str, m_model->getColumnNames()) {
    result.append(QVariant(str));
  }

  return result;
}

void ValueEditor::ValueViewModel::reload() {
  m_model->clearRowCache();
  m_model->loadRowsCount([this](const QString& err) {
    if (err.size() > 0 || m_model->rowsCount() <= 0) {
      emit error(
          QCoreApplication::translate("RDM", "Cannot reload key value: %1")
              .arg(err));
      return;
    }

    emit totalRowCountChanged();
    emit pageSizeChanged();

    loadRows(m_startFramePosition, m_model->rowsCount() < pageSize()
                                       ? m_model->rowsCount()
                                       : pageSize());
  });
}

void ValueEditor::ValueViewModel::setSinglePageMode(bool v) {
  m_singlePageMode = v;
  emit singlePageModeChanged();
}

bool ValueEditor::ValueViewModel::singlePageMode() const
{
    return m_singlePageMode;
}

bool ValueEditor::ValueViewModel::isRowLoaded(int i) {
  return m_model->isRowLoaded(i);
}

void ValueEditor::ValueViewModel::loadRows(int start, int limit) {
  int rowsLeft = totalRowCount() - start;
  int loaded = (rowsLeft > limit) ? limit : rowsLeft;

  // frame already loaded
  if (m_model->isRowLoaded(start) && m_model->isRowLoaded(start + loaded - 1)) {
    m_startFramePosition = start;
    m_lastLoadedRowFrameSize = loaded;

    emit layoutAboutToBeChanged();
    emit rowsLoaded(start, loaded);
    emit layoutChanged();
    return;
  }

  QString msg = QCoreApplication::translate("RDM", "Cannot load key value: %1");

  m_model->loadRows(
      start, limit,
      [this, start, limit, msg](const QString& err, unsigned long rowsCount) {
        if (!err.isEmpty()) {
          emit error(msg.arg(err));
          return;
        }

        m_lastLoadedRowFrameSize = rowsCount > limit ? limit : rowsCount;
        m_startFramePosition = start;

        emit layoutAboutToBeChanged();
        emit rowsLoaded(start, m_lastLoadedRowFrameSize);
        emit layoutChanged();
      });
}

void ValueEditor::ValueViewModel::addRow(const QVariantMap& row) {
  m_model->addRow(row, [this](const QString& err) {
    if (err.size() > 0) {
      emit error(err);
      return;
    }
    emit layoutChanged();
  });
}

void ValueEditor::ValueViewModel::updateRow(int i, const QVariantMap& row) {
  int targetRow = mapRowIndex(i);
  if (targetRow < 0 || !m_model->isRowLoaded(targetRow)) return;

  m_model->updateRow(targetRow, row, [this, i](const QString& err) {
    if (err.size() > 0) {
      emit error(err);
      return;
    }
    emit dataChanged(index(i, 0), index(i, 0));
  });
}

void ValueEditor::ValueViewModel::deleteRow(int i) {
  int targetRow = mapRowIndex(i);

  if (targetRow < 0 || !m_model->isRowLoaded(targetRow)) return;

  m_model->removeRow(targetRow, [this, i, targetRow](const QString& err) {
    if (err.size() > 0) {
      emit error(err);
      return;
    }

    emit beginRemoveRows(QModelIndex(), i, i);
    emit endRemoveRows();

    if (targetRow < m_model->rowsCount())
      emit dataChanged(index(i, 0), index(m_model->rowsCount() - 1, 0));

    if (m_model->rowsCount() == 0) emit keyRemoved();
  });
}

int ValueEditor::ValueViewModel::totalRowCount() {
  return m_model->rowsCount();
}

int ValueEditor::ValueViewModel::pageSize() {
  QSettings settings;

  return settings.value("app/valueEditorPageSize", 1000).toInt();
}

QVariantMap ValueEditor::ValueViewModel::getRow(int row) {
  int targetRow = mapRowIndex(row);
  if (targetRow < 0 || !m_model->isRowLoaded(targetRow)) return QVariantMap();

  QVariantMap res = getRowRaw(row);
  return res;
}

void ValueEditor::ValueViewModel::loadRowsCount() {
  m_model->loadRowsCount([this](const QString& err) {
    if (err.size() > 0) {
      emit error(err);
      return;
    }

    emit totalRowCountChanged();
  });
}
