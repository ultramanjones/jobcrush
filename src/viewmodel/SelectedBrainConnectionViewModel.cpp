#include "SelectedBrainConnectionViewModel.h"

#include "../logic/aibrain/AiBrain.h"

SelectedBrainConnectionViewModel::SelectedBrainConnectionViewModel(
    AiBrain &aiBrain, QObject *parent)
    : QObject(parent)
    , brain(aiBrain)
{
    // A new selection and a new connection result both change what the view
    // shows, so both send the same signal.
    const auto announceChange = [this]() {
        ++connectionChangeCounter;
        emit connectionChanged();
    };
    connect(&brain, &AiBrain::selectedProviderChanged, this, announceChange);
    connect(&brain, &AiBrain::connectionStateChanged, this, announceChange);

    connect(&brain, &AiBrain::brainLevelsChanged, this, [this]() {
        ++brainLevelChangeCounter;
        emit brainLevelsChanged();
    });

    // A refusal the user must be told about. The Logic layer reports what
    // happened. This class holds it until the user dismisses it, and the
    // view decides how to show it.
    connect(&brain, &AiBrain::connectionAttemptRefused, this,
            [this](const QString &vendorName, const QString &plainReason,
                   const QString &whatToDoNext) {
        refusalVendorName = vendorName;
        refusalReasonText = plainReason;
        refusalNextStepText = whatToDoNext;
        refusalIsShowing = true;
        emit connectionRefusalChanged();
    });
}

bool SelectedBrainConnectionViewModel::connectionRefusalIsShowing() const
{
    return refusalIsShowing;
}

QString SelectedBrainConnectionViewModel::connectionRefusalTitle() const
{
    if (refusalVendorName.isEmpty()) {
        return QStringLiteral("That brain would not connect");
    }
    return QStringLiteral("%1 would not connect").arg(refusalVendorName);
}

QString SelectedBrainConnectionViewModel::connectionRefusalReason() const
{
    return refusalReasonText;
}

QString SelectedBrainConnectionViewModel::connectionRefusalNextStep() const
{
    return refusalNextStepText;
}

void SelectedBrainConnectionViewModel::dismissConnectionRefusal()
{
    if (!refusalIsShowing) {
        return;
    }
    refusalIsShowing = false;
    emit connectionRefusalChanged();
}

QString SelectedBrainConnectionViewModel::bannerText() const
{
    // Once the state is settled, the banner says one of two things: which
    // brain is live, or that none is. While a check is running or has not
    // run yet, it says that instead of guessing.
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
    // Ticking the box IS the user asking. A refusal from here is announced.
    brain.selectProviderKind(aiProviderKindFromStorageText(providerKindName));
}

void SelectedBrainConnectionViewModel::checkConnectionNow()
{
    brain.checkConnectionNow();
}

// --- How much brain ----------------------------------------------------------

QStringList SelectedBrainConnectionViewModel::brainLevelNames() const
{
    QStringList levelNames;
    for (const AiBrainLevel brainLevel : everyAiBrainLevel()) {
        levelNames.append(aiBrainLevelName(brainLevel));
    }
    return levelNames;
}

int SelectedBrainConnectionViewModel::brainLevelRevision() const
{
    return brainLevelChangeCounter;
}

QString SelectedBrainConnectionViewModel::brainLevelFor(const QString &providerKindName) const
{
    return aiBrainLevelName(brain.brainLevelFor(aiProviderKindFromStorageText(providerKindName)));
}

void SelectedBrainConnectionViewModel::setBrainLevelFor(const QString &providerKindName,
                                                        const QString &brainLevelName)
{
    brain.setBrainLevelFor(aiProviderKindFromStorageText(providerKindName),
                           aiBrainLevelFromName(brainLevelName));
}

QString SelectedBrainConnectionViewModel::modelNameFor(const QString &providerKindName,
                                                       const QString &brainLevelName) const
{
    return brain.modelNameFor(aiProviderKindFromStorageText(providerKindName),
                              aiBrainLevelFromName(brainLevelName));
}

QString SelectedBrainConnectionViewModel::brainLevelExplanation(
    const QString &brainLevelName) const
{
    return aiBrainLevelExplanation(aiBrainLevelFromName(brainLevelName));
}
