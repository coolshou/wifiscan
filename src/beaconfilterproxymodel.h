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
    Q_SLOT void setRSSIFilter(const QString &filter);
    Q_SLOT void setFreq2Filter(bool filted); //2.4G
    Q_SLOT void setFreq5Filter(bool filted); //5G
    Q_SLOT void setFreq6Filter(bool filted); //6G
signals:
    void macFilterChanged();
    void ssidFilterChanged();
    void rssiFilterChanged();
    void freqFilterChanged();

protected:
    // This is where all filter checks are performed
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_macFilter;
    QString m_ssidFilter;
    double m_rssiFilter;
    QString m_freqFilter;
    bool m_freq2Filter;
    bool m_freq5Filter;
    bool m_freq6Filter;
};

#endif // BEACONFILTERPROXYMODEL_H
