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

void BeaconFilterProxyModel::setFreq2Filter(bool filted)
{
    m_freq2Filter = filted;
    invalidateFilter();

    emit freqFilterChanged();
}

void BeaconFilterProxyModel::setFreq5Filter(bool filted)
{
    m_freq5Filter = filted;
    invalidateFilter();

    emit freqFilterChanged();
}

void BeaconFilterProxyModel::setFreq6Filter(bool filted)
{
    m_freq6Filter = filted;
    invalidateFilter();

    emit freqFilterChanged();
}

bool BeaconFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    bool ok = false;
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
        double rssi = rssiData.toDouble(&ok);
        if (ok){
            if (rssi <= m_rssiFilter){
                rssiFilterMatches = false;
            }
        }
    }
    // --- 4. freq filter check ---
    bool freqFilterMatches = true;
    if (!m_freq2Filter && !m_freq5Filter && !m_freq6Filter){
        //show all
    }else{
        QModelIndex freqIndex = sourceModel()->index(sourceRow, 0, sourceParent);
        QVariant freqData = sourceModel()->data(freqIndex, BeaconModel::Roles::FrequencyRole);
        int freq = freqData.toInt(&ok);
        if (ok){
            if (m_freq2Filter){
                if (freq <= 2412 || freq >= 2484){
                    freqFilterMatches = false;
                }
            }
            if (m_freq5Filter){
                if (freq <= 5170 || freq >= 5825){
                    freqFilterMatches = false;
                }
            }
            if (m_freq6Filter){
                if (freq <= 5925 || freq >= 7125){
                    freqFilterMatches = false;
                }
            }
        }
    }
    // Return the combined result
    return macFilterMatches && ssidFilterMatches
           && rssiFilterMatches && freqFilterMatches;
}
