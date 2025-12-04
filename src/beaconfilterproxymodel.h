#ifndef BEACONFILTERPROXYMODEL_H
#define BEACONFILTERPROXYMODEL_H

#include <QObject>
#include <QSortFilterProxyModel>

class BeaconFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString macFilter READ macFilter WRITE setMacFilter NOTIFY macFilterChanged)
    Q_PROPERTY(QString ssidFilter READ ssidFilter WRITE setSSIDFilter NOTIFY ssidFilterChanged)
public:
    explicit BeaconFilterProxyModel(QObject *parent = nullptr);
    QString macFilter() const { return m_macFilter; }
    Q_SLOT void setMacFilter(const QString &filter);

    QString ssidFilter() const { return m_ssidFilter; }
    Q_SLOT void setSSIDFilter(const QString &filter);
signals:
    void macFilterChanged();
    void ssidFilterChanged();

protected:
    // This is where all filter checks are performed
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_macFilter;
    QString m_ssidFilter;
};

#endif // BEACONFILTERPROXYMODEL_H
