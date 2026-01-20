// beaconmodel.cpp
#include "beaconmodel.h"
#include <QVariant>

#include <QDebug>

BeaconModel::BeaconModel(QObject *parent) : QAbstractListModel(parent)
{
}

int BeaconModel::rowCount(const QModelIndex &) const {
    return m_beacons.size();
}

int BeaconModel::columnCount(const QModelIndex &parent) const
{
    return 7; // for QML show's column number
}

QVariant BeaconModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_beacons.size())
        return QVariant();

    const BeaconDetail &b = m_beacons.at(index.row());
    switch (role) {
    case SsidRole: return b.ssid;
    case BssidRole: return b.bssid;
    case FrequencyRole: return b.frequency;
    case ChannelRole: return b.channel;
    case SignalRole: return b.signal;
    case TransmitpowerRole: return b.transmitpower;
    case Is11nRole: return QVariant::fromValue(b.is11n);
    case Is11acRole: return QVariant::fromValue(b.is11ac);
    case Is11axRole: return QVariant::fromValue(b.is11ax);
    case Is11beRole: return QVariant::fromValue(b.is11be);
    case Is11bnRole: return QVariant::fromValue(b.is11bn);
    case BSSColorRole: return b.bss_color;
    case BSSColorPartialRole: return QVariant::fromValue(b.bss_color_partial);
    case BSSColorDisableRole: return QVariant::fromValue(b.bss_color_disable);
    case CapabilitiesRole: return b.capabilities;
    case elmRole: return QVariant::fromValue(b.elmIDs);
    case elmExtRole: return QVariant::fromValue(b.elmExtIDs);
    default: return QVariant();
    }
}

QHash<int, QByteArray> BeaconModel::roleNames() const {
    //rule name
    return {
        { SsidRole, "ssid" },
        { BssidRole, "bssid" },
        { FrequencyRole, "frequency" },
        { ChannelRole, "channel" },
        { SignalRole, "signal" },
        { TransmitpowerRole, "transmitpower"},
        { Is11nRole, "is11n"},
        { Is11acRole, "is11ac"},
        { Is11axRole, "is11ax"},
        { Is11beRole, "is11be"},
        { Is11bnRole, "is11bn"},
        { BSSColorRole, "bsscolor"},
        { BSSColorPartialRole, "bsscolorpartial"},
        { BSSColorDisableRole, "bsscolordisable"},
        { CapabilitiesRole, "capabilities" },
        { elmRole, "elementIDs" },
        { elmExtRole, "elementExtIDs" }
    };
}

void BeaconModel::setBeacons(const QList<BeaconDetail> &beacons) {
    beginResetModel();
    m_beacons = beacons;
    endResetModel();
}

QVariantMap BeaconModel::getElementIDsRoleMap(int row) const
{
    QVariantMap result;
    // 將 int 鍵轉換為 QString 鍵，QByteArray 值轉換為 QString/QVariant 值
    const BeaconDetail &b = m_beacons.at(row);
    QMapIterator<int, QByteArray> i(b.elmIDs);
    while (i.hasNext()) {
        i.next();
        // 將 int 鍵轉換為字串 (例如 "1", "2", "3")
        // 將 QByteArray 轉換為 QString
        result.insert(QString::number(i.key()), QString(i.value()));
    }
    qDebug() << "getElementIDsRoleMap:" << result;
    return result;
}

QVariantMap BeaconModel::getElementExtIDsRoleMap(int row) const
{
    QVariantMap result;
    return result;
}

int BeaconModel::count()
{
    if (!m_beacons.isEmpty()){
        qDebug() << "m_beacons:" << m_beacons.count();
        return m_beacons.count();
    }
    return 0;
}

int BeaconModel::roleNameToInt(const QString& roleName) const
{
    QHash<int, QByteArray> roles = roleNames();
    QHashIterator<int, QByteArray> i(roles);
    while (i.hasNext()) {
        i.next();
        if (i.value() == roleName.toUtf8()) {
            return i.key();
        }
    }
    return -1; // Role not found
}

// The Core Sorting Logic
void BeaconModel::sortByRole(const QString &roleName)
{
    int role = roleNameToInt(roleName);
    if (role == -1) {
        qWarning() << "Unknown role for sorting:" << roleName;
        return;
    }

    // Toggle sort order if clicking the same column again
    if (roleName == m_currentSortRole) {
        m_currentSortOrder = (m_currentSortOrder == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder;
    } else {
        m_currentSortRole = roleName;
        m_currentSortOrder = Qt::AscendingOrder; // Default to ascending for a new column
    }

    // Inform the view that the data is about to change
    beginResetModel();

    // Use std::sort with a lambda function as the custom comparator
    std::sort(m_beacons.begin(), m_beacons.end(),
              [this, role](const BeaconDetail& a, const BeaconDetail& b) -> bool {
                  bool isAscending = (m_currentSortOrder == Qt::AscendingOrder);

                  // Comparator implementation based on the role
                  switch (role) {
                  case BssidRole:
                      return isAscending ? (a.bssid < b.bssid) : (a.bssid > b.bssid);
                  case SsidRole:
                      return isAscending ? (a.ssid < b.ssid) : (a.ssid > b.ssid);
                  case FrequencyRole:
                      return isAscending ? (a.frequency < b.frequency) : (a.frequency > b.frequency);
                  case SignalRole:
                      // Signal is typically a negative number (e.g., -50 is stronger than -90)
                      // For Ascending (weaker signal first), we want -90 < -50 to be true
                      // For Descending (stronger signal first), we want -50 < -90 to be true
                      return isAscending ? (a.signal < b.signal) : (a.signal > b.signal);
                  case BSSColorRole:
                      return isAscending ? (a.bss_color < b.bss_color) : (a.bss_color > b.bss_color);
                  default:
                      // Fallback comparison (e.g., by BSSID)
                      return isAscending ? (a.bssid < b.bssid) : (a.bssid > b.bssid);
                  }
              });
    // Inform the view that the data has changed
    endResetModel();
    // qDebug() << "Model sorted by" << roleName << (m_currentSortOrder == Qt::AscendingOrder ? "Ascending" : "Descending");
}
