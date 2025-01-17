/*
 * Copyright (c) 2010 Network Security Lab, University of Washington, Seattle.
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Authors: Sidharth Nabar <snabar@uw.edu>, He Wu <mdzz@u.washington.edu>
 */

#include "rv-battery-model.h"

#include "ns3/assert.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/trace-source-accessor.h"

#include <cmath>

namespace ns3
{
namespace energy
{

NS_LOG_COMPONENT_DEFINE("RvBatteryModel");
NS_OBJECT_ENSURE_REGISTERED(RvBatteryModel);

TypeId
RvBatteryModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::energy::RvBatteryModel")
            .AddDeprecatedName("ns3::RvBatteryModel")
            .SetParent<EnergySource>()
            .SetGroupName("Energy")
            .AddConstructor<RvBatteryModel>()
            .AddAttribute("RvBatteryModelPeriodicEnergyUpdateInterval",
                          "RV battery model sampling interval.",
                          TimeValue(Seconds(1)),
                          MakeTimeAccessor(&RvBatteryModel::SetSamplingInterval,
                                           &RvBatteryModel::GetSamplingInterval),
                          MakeTimeChecker())
            .AddAttribute("RvBatteryModelLowBatteryThreshold",
                          "Low battery threshold.",
                          DoubleValue(0.10), // as a fraction of the initial energy
                          MakeDoubleAccessor(&RvBatteryModel::m_lowBatteryTh),
                          MakeDoubleChecker<double>())
            .AddAttribute("RvBatteryModelOpenCircuitVoltage",
                          "RV battery model open circuit voltage.",
                          DoubleValue(4.1),
                          MakeDoubleAccessor(&RvBatteryModel::SetOpenCircuitVoltage,
                                             &RvBatteryModel::GetOpenCircuitVoltage),
                          MakeDoubleChecker<double>())
            .AddAttribute("RvBatteryModelCutoffVoltage",
                          "RV battery model cutoff voltage.",
                          DoubleValue(3.0),
                          MakeDoubleAccessor(&RvBatteryModel::SetCutoffVoltage,
                                             &RvBatteryModel::GetCutoffVoltage),
                          MakeDoubleChecker<double>())
            .AddAttribute("RvBatteryModelAlphaValue",
                          "RV battery model alpha value.",
                          DoubleValue(35220.0),
                          MakeDoubleAccessor(&RvBatteryModel::SetAlpha, &RvBatteryModel::GetAlpha),
                          MakeDoubleChecker<double>())
            .AddAttribute("RvBatteryModelBetaValue",
                          "RV battery model beta value.",
                          DoubleValue(0.637),
                          MakeDoubleAccessor(&RvBatteryModel::SetBeta, &RvBatteryModel::GetBeta),
                          MakeDoubleChecker<double>())
            .AddAttribute(
                "RvBatteryModelNumOfTerms",
                "The number of terms of the infinite sum for estimating battery level.",
                IntegerValue(10), // value used in paper
                MakeIntegerAccessor(&RvBatteryModel::SetNumOfTerms, &RvBatteryModel::GetNumOfTerms),
                MakeIntegerChecker<int>())
            .AddTraceSource("RvBatteryModelBatteryLevel",
                            "RV battery model battery level.",
                            MakeTraceSourceAccessor(&RvBatteryModel::m_batteryLevel),
                            "ns3::TracedValueCallback::Double")
            .AddTraceSource("RvBatteryModelBatteryLifetime",
                            "RV battery model battery lifetime.",
                            MakeTraceSourceAccessor(&RvBatteryModel::m_lifetime),
                            "ns3::TracedValueCallback::Time");
    return tid;
}

RvBatteryModel::RvBatteryModel()
{
    NS_LOG_FUNCTION(this);
    m_lastSampleTime = Simulator::Now();
    m_timeStamps.push_back(m_lastSampleTime);
    m_previousLoad = -1.0;
    m_batteryLevel = 1; // fully charged
    m_lifetime = Seconds(0);
}

RvBatteryModel::~RvBatteryModel()
{
    NS_LOG_FUNCTION(this);
}

double
RvBatteryModel::GetInitialEnergy() const
{
    NS_LOG_FUNCTION(this);
    return m_alpha * GetSupplyVoltage();
}

double
RvBatteryModel::GetSupplyVoltage() const
{
    NS_LOG_FUNCTION(this);
    // average of Voc and Vcutoff
    return (m_openCircuitVoltage - m_cutoffVoltage) / 2 + m_cutoffVoltage;
}

double
RvBatteryModel::GetRemainingEnergy()
{
    NS_LOG_FUNCTION(this);
    UpdateEnergySource();
    return m_alpha * GetSupplyVoltage() * m_batteryLevel;
}

double
RvBatteryModel::GetEnergyFraction()
{
    NS_LOG_FUNCTION(this);
    return GetBatteryLevel();
}

void
RvBatteryModel::UpdateEnergySource()
{
    NS_LOG_FUNCTION(this);

    // do not update if battery is already dead
    if (m_batteryLevel <= 0)
    {
        NS_LOG_DEBUG("RvBatteryModel:Battery is dead!");
        return;
    }

    // do not update if simulation has finished
    if (Simulator::IsFinished())
    {
        return;
    }

    NS_LOG_DEBUG("RvBatteryModel:Updating remaining energy!");

    m_currentSampleEvent.Cancel();

    double currentLoad = CalculateTotalCurrent() * 1000; // must be in mA
    double calculatedAlpha = Discharge(currentLoad, Simulator::Now());

    NS_LOG_DEBUG("RvBatteryModel:Calculated alpha = " << calculatedAlpha << " time = "
                                                      << Simulator::Now().As(Time::S));

    // calculate battery level
    m_batteryLevel = 1 - (calculatedAlpha / m_alpha);
    if (m_batteryLevel < 0)
    {
        m_batteryLevel = 0;
    }

    // check if battery level is below the low battery threshold.
    if (m_batteryLevel <= m_lowBatteryTh)
    {
        m_lifetime = Simulator::Now() - m_timeStamps[0];
        NS_LOG_DEBUG("RvBatteryModel:Battery level below threshold!");
        HandleEnergyDrainedEvent();
    }

    m_previousLoad = currentLoad;
    m_lastSampleTime = Simulator::Now();
    m_currentSampleEvent =
        Simulator::Schedule(m_samplingInterval, &RvBatteryModel::UpdateEnergySource, this);
}

void
RvBatteryModel::SetSamplingInterval(Time interval)
{
    NS_LOG_FUNCTION(this << interval);
    m_samplingInterval = interval;
}

Time
RvBatteryModel::GetSamplingInterval() const
{
    NS_LOG_FUNCTION(this);
    return m_samplingInterval;
}

void
RvBatteryModel::SetOpenCircuitVoltage(double voltage)
{
    NS_LOG_FUNCTION(this << voltage);
    NS_ASSERT(voltage >= 0);
    m_openCircuitVoltage = voltage;
}

double
RvBatteryModel::GetOpenCircuitVoltage() const
{
    NS_LOG_FUNCTION(this);
    return m_openCircuitVoltage;
}

void
RvBatteryModel::SetCutoffVoltage(double voltage)
{
    NS_LOG_FUNCTION(this << voltage);
    NS_ASSERT(voltage <= m_openCircuitVoltage);
    m_cutoffVoltage = voltage;
}

double
RvBatteryModel::GetCutoffVoltage() const
{
    NS_LOG_FUNCTION(this);
    return m_cutoffVoltage;
}

void
RvBatteryModel::SetAlpha(double alpha)
{
    NS_LOG_FUNCTION(this << alpha);
    NS_ASSERT(alpha >= 0);
    m_alpha = alpha;
}

double
RvBatteryModel::GetAlpha() const
{
    NS_LOG_FUNCTION(this);
    return m_alpha;
}

void
RvBatteryModel::SetBeta(double beta)
{
    NS_LOG_FUNCTION(this << beta);
    NS_ASSERT(beta >= 0);
    m_beta = beta;
}

double
RvBatteryModel::GetBeta() const
{
    NS_LOG_FUNCTION(this);
    return m_beta;
}

double
RvBatteryModel::GetBatteryLevel()
{
    NS_LOG_FUNCTION(this);
    UpdateEnergySource();
    return m_batteryLevel;
}

Time
RvBatteryModel::GetLifetime() const
{
    NS_LOG_FUNCTION(this);
    return m_lifetime;
}

void
RvBatteryModel::SetNumOfTerms(int num)
{
    NS_LOG_FUNCTION(this << num);
    m_numOfTerms = num;
}

int
RvBatteryModel::GetNumOfTerms() const
{
    NS_LOG_FUNCTION(this);
    return m_numOfTerms;
}

/*
 * Private functions start here.
 */

void
RvBatteryModel::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    NS_LOG_DEBUG("RvBatteryModel:Starting battery level update!");
    UpdateEnergySource(); // start periodic sampling of load (total current)
}

void
RvBatteryModel::DoDispose()
{
    NS_LOG_FUNCTION(this);
    BreakDeviceEnergyModelRefCycle(); // break reference cycle
}

void
RvBatteryModel::HandleEnergyDrainedEvent()
{
    NS_LOG_FUNCTION(this);
    NS_LOG_DEBUG("RvBatteryModel:Energy depleted!");
    NotifyEnergyDrained(); // notify DeviceEnergyModel objects
}

double
RvBatteryModel::Discharge(double load, Time t)
{
    NS_LOG_FUNCTION(this << load << t);

    // record only when load changes
    if (load != m_previousLoad)
    {
        m_load.push_back(load);
        m_previousLoad = load;
        m_timeStamps[m_timeStamps.size() - 1] = m_lastSampleTime;
        m_timeStamps.push_back(t);
    }
    else
    {
        if (!m_timeStamps.empty())
        {
            m_timeStamps[m_timeStamps.size() - 1] = t;
        }
    }

    m_lastSampleTime = t;

    // calculate alpha for new t
    NS_ASSERT(m_load.size() == m_timeStamps.size() - 1); // size must be equal
    double calculatedAlpha = 0.0;
    if (m_timeStamps.size() == 1)
    {
        // constant load
        calculatedAlpha = m_load[0] * RvModelAFunction(t, t, Seconds(0), m_beta);
    }
    else
    {
        // changing load
        for (uint64_t i = 1; i < m_timeStamps.size(); i++)
        {
            calculatedAlpha +=
                m_load[i - 1] * RvModelAFunction(t, m_timeStamps[i], m_timeStamps[i - 1], m_beta);
        }
    }

    return calculatedAlpha;
}

double
RvBatteryModel::RvModelAFunction(Time t, Time sk, Time sk_1, double beta)
{
    NS_LOG_FUNCTION(this << t << sk << sk_1 << beta);

    // everything is in minutes
    double firstDelta = (t - sk).GetMinutes();
    double secondDelta = (t - sk_1).GetMinutes();
    double delta = (sk - sk_1).GetMinutes();

    double sum = 0.0;
    for (int m = 1; m <= m_numOfTerms; m++)
    {
        double square = beta * beta * m * m;
        sum += (std::exp(-square * (firstDelta)) - std::exp(-square * (secondDelta))) / square;
    }
    return delta + 2 * sum;
}

} // namespace energy
} // namespace ns3
