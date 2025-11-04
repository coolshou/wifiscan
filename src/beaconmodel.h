#ifndef BEACONMODEL_H
#define BEACONMODEL_H

// beaconmodel.h
#pragma once
#include <QAbstractListModel>
#include <QModelIndex>

#include "beacondetail.h"

class BeaconModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        SsidRole = Qt::UserRole + 1,
        BssidRole,
        FrequencyRole,
        ChannelRole,
        SignalRole,
        TransmitpowerRole,
        Is11nRole,
        Is11acRole,
        Is11axRole,
        Is11beRole,
        Is11bnRole,
        BSSColorRole,
        BSSColorPartialRole,
        BSSColorDisableRole,
        CapabilitiesRole,
        elmRole,
        elmExtRole
    };

    explicit BeaconModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setBeacons(const QList<BeaconDetail> &beacons);
    Q_INVOKABLE QVariantMap getElementIDsRoleMap(int row) const;
    Q_INVOKABLE QVariantMap getElementExtIDsRoleMap(int row) const;
    Q_INVOKABLE int count();
public slots:
    // 💡 The new sorting slot to be called from QML
    void sortByRole(const QString &roleName);
private:
    QList<BeaconDetail> m_beacons;
    QString m_currentSortRole;
    Qt::SortOrder m_currentSortOrder = Qt::AscendingOrder;

    // Helper function to convert role name string to the internal role enum
    int roleNameToInt(const QString& roleName) const;
};

#endif // BEACONMODEL_H
