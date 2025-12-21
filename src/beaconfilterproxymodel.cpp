#include "beaconfilterproxymodel.h"

#include "beaconmodel.h"

BeaconFilterProxyModel::BeaconFilterProxyModel(QObject *parent)
    :QSortFilterProxyModel(parent)
{

}

void BeaconFilterProxyModel::setMacFilter(const QString &filter)
{
    if (m_macFilter == filter)
        return;

    m_macFilter = filter;
    invalidateFilter();

    emit macFilterChanged();
}

void BeaconFilterProxyModel::setSSIDFilter(const QString &filter)
{
    if (m_ssidFilter == filter)
        return;

    m_ssidFilter = filter;
    invalidateFilter();

    emit ssidFilterChanged();
}

void BeaconFilterProxyModel::setRSSIFilter(const QString &filter)
{
    bool ok = false;
    double val = filter.toDouble(&ok);
    if (!ok){
        return;
    }
    if (qFuzzyIsNull(val)){
        return;
    }
    if (qFuzzyCompare(m_rssiFilter, val)){
        return;
    }

    m_rssiFilter = val;
    invalidateFilter();
    emit rssiFilterChanged();

}

void BeaconFilterProxyModel::setFreqFilter(const QString &filter)
{

}

bool BeaconFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    // --- 1. Mac Filter Check ---
    bool macFilterMatches = true;
    if (!m_macFilter.isEmpty()) {
        QModelIndex macIndex = sourceModel()->index(sourceRow, 0, sourceParent);
        QVariant macData = sourceModel()->data(macIndex, BeaconModel::Roles::BssidRole);
        if (!macData.toString().contains(m_macFilter, filterCaseSensitivity())) {
            macFilterMatches = false;
        }
    }

    // --- 2. SSID Filter Check ---
    bool ssidFilterMatches = true;
    if (!m_ssidFilter.isEmpty()) {
        QModelIndex ssidIndex = sourceModel()->index(sourceRow, 0, sourceParent);
        QVariant ssidData = sourceModel()->data(ssidIndex, BeaconModel::Roles::SsidRole);
        if (!ssidData.toString().contains(m_ssidFilter, filterCaseSensitivity())) {
            ssidFilterMatches = false;
        }
    }
    // --- 3. RSSI Filter Check ---
    bool rssiFilterMatches = true;
    if (!qFuzzyIsNull(m_rssiFilter)) {
        QModelIndex rssiIndex = sourceModel()->index(sourceRow, 0, sourceParent);
        QVariant rssiData = sourceModel()->data(rssiIndex, BeaconModel::Roles::SignalRole);
        bool ok = false;
        double rssi = rssiData.toDouble(&ok);
        if (ok){
            if (rssi <= m_rssiFilter){
                rssiFilterMatches = false;
            }
        }
    }
    // 3. Return the combined result
    return macFilterMatches && ssidFilterMatches && rssiFilterMatches;
}
