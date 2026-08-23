#include "SelectedBrainConnectionViewModel.h"

#include "../modelview/aibrain/AiBrain.h"

SelectedBrainConnectionViewModel::SelectedBrainConnectionViewModel(
    AiBrain &aiBrain, QObject *parent)
    : QObject(parent)
    , brain(aiBrain)
{
    // Selection changes and connection results both land in the same place
    // for the view: one signal, one truth.
    const auto announceChange = [this]() {
        ++connectionChangeCounter;
        emit connectionChanged();
    };
    connect(&brain, &AiBrain::selectedProviderChanged, this, announceChange);
    connect(&brain, &AiBrain::connectionStateChanged, this, announceChange);
}

QString SelectedBrainConnectionViewModel::bannerText() const
{
    // The banner says one of exactly two things when it is settled: which
    // brain is live, or that none is. Anything in between says so honestly
    // rather than guessing on the user's behalf.
    const QString vendorName = brain.selectedProviderDisplayName().toUpper();

    switch (brain.connectionState()) {
    case AiBrainConnectionState::ConnectedAndActive:
        return QStringLiteral("%1 IS SELECTED AND ACTIVE").arg(vendorName);
    case AiBrainConnectionState::Checking:
        return QStringLiteral("CONFIRMING %1…").arg(vendorName);
    case AiBrainConnectionState::NotYetChecked:
        return QStringLiteral("%1 IS SELECTED — NOT CONFIRMED YET").arg(vendorName);
    case AiBrainConnectionState::ConnectionFailed:
    case AiBrainConnectionState::NoBrainSelected:
        break;
    }
    return QStringLiteral("NO BRAIN SELECTED AND ACTIVE");
}

bool SelectedBrainConnectionViewModel::brainIsConnectedAndActive() const
{
    return brain.connectionState() == AiBrainConnectionState::ConnectedAndActive;
}

bool SelectedBrainConnectionViewModel::connectionIsBeingChecked() const
{
    return brain.connectionState() == AiBrainConnectionState::Checking;
}

QString SelectedBrainConnectionViewModel::statusDetailText() const
{
    switch (brain.connectionState()) {
    case AiBrainConnectionState::ConnectedAndActive:
        return brain.lastVerifiedAtText();
    case AiBrainConnectionState::ConnectionFailed:
        // The vendor's own plain-speech reason, already written for humans.
        return brain.connectionStatusText();
    case AiBrainConnectionState::NoBrainSelected:
        return QStringLiteral("Tick a brain below to put it to work.");
    case AiBrainConnectionState::Checking:
    case AiBrainConnectionState::NotYetChecked:
        break;
    }
    return QString();
}

QString SelectedBrainConnectionViewModel::selectedProviderKindName() const
{
    if (brain.connectionState() == AiBrainConnectionState::ConnectionFailed) {
        // A brain that just refused to answer is not the selected brain as
        // far as the user's eyes are concerned — the tick comes back off.
        return QString();
    }
    return brain.selectedProviderKindName();
}

int SelectedBrainConnectionViewModel::connectionRevision() const
{
    return connectionChangeCounter;
}

bool SelectedBrainConnectionViewModel::providerCanBeSelected(
    const QString &providerKindName) const
{
    return brain.providerIsSelectable(aiProviderKindFromStorageText(providerKindName));
}

bool SelectedBrainConnectionViewModel::providerIsSelected(
    const QString &providerKindName) const
{
    const QString currentSelection = selectedProviderKindName();
    return !currentSelection.isEmpty() && currentSelection == providerKindName;
}

bool SelectedBrainConnectionViewModel::providerIsSelectedAndActive(
    const QString &providerKindName) const
{
    return providerIsSelected(providerKindName) && brainIsConnectedAndActive();
}

QString SelectedBrainConnectionViewModel::reasonProviderCannotBeSelected(
    const QString &providerKindName) const
{
    return brain.reasonProviderCannotBeSelected(
        aiProviderKindFromStorageText(providerKindName));
}

void SelectedBrainConnectionViewModel::setProviderSelected(
    const QString &providerKindName, bool shouldBeSelected)
{
    if (!shouldBeSelected) {
        brain.clearSelectedProvider();
        return;
    }
    brain.selectProviderKind(aiProviderKindFromStorageText(providerKindName));
}

void SelectedBrainConnectionViewModel::checkConnectionNow()
{
    brain.checkConnectionNow();
}
