#pragma once

#include <QObject>
#include <QString>

class AiBrain;

// SelectedBrainConnectionViewModel
//
// Serves one small, important truth to the view: WHICH brain the user picked,
// and whether that brain is genuinely connected and active right now. Both
// the Settings page (checkboxes and the banner) and the Brain Chat header
// read from this one place, so the two screens can never disagree.
//
// Named for the data it serves — the selected brain's connection — not for
// the screen that shows it. Translation and organization only: the selection
// rules and the verification plumbing live below, in AiBrain (ModelView).
class SelectedBrainConnectionViewModel : public QObject {
    Q_OBJECT

    // The loud line at the top of Settings, already shouting:
    // "OPENROUTER IS SELECTED AND ACTIVE" / "NO BRAIN SELECTED AND ACTIVE".
    Q_PROPERTY(QString bannerText READ bannerText NOTIFY connectionChanged)

    // True only when a brain is verified live — the banner goes green on
    // this and on nothing else.
    Q_PROPERTY(bool brainIsConnectedAndActive READ brainIsConnectedAndActive
                   NOTIFY connectionChanged)

    // True while a verification request is in flight. The view pulses the
    // banner text on this — never a spinner, by law.
    Q_PROPERTY(bool connectionIsBeingChecked READ connectionIsBeingChecked
                   NOTIFY connectionChanged)

    // The quieter second line: what to fix when a check failed, or the
    // receipt ("checked at 2:15 PM") when it succeeded. Empty when silent.
    Q_PROPERTY(QString statusDetailText READ statusDetailText NOTIFY connectionChanged)

    // "openrouter" — which provider's checkbox is ticked. Empty when none.
    Q_PROPERTY(QString selectedProviderKindName READ selectedProviderKindName
                   NOTIFY connectionChanged)

    // Bumped on every change, so QML bindings that call the per-provider
    // invokables below re-evaluate (same technique as the credential roster).
    Q_PROPERTY(int connectionRevision READ connectionRevision NOTIFY connectionChanged)

    // --- "That brain would not connect" ----------------------------------
    //
    // A tick that quietly undoes itself is the app shrugging at the user. So
    // when a check the user started comes back refused, these four carry the
    // explanation to the screen: what happened, in whose words, and what to
    // do about it. Showing is true only until the user says they have read it.
    Q_PROPERTY(bool connectionRefusalIsShowing READ connectionRefusalIsShowing
                   NOTIFY connectionRefusalChanged)
    Q_PROPERTY(QString connectionRefusalTitle READ connectionRefusalTitle
                   NOTIFY connectionRefusalChanged)
    Q_PROPERTY(QString connectionRefusalReason READ connectionRefusalReason
                   NOTIFY connectionRefusalChanged)
    Q_PROPERTY(QString connectionRefusalNextStep READ connectionRefusalNextStep
                   NOTIFY connectionRefusalChanged)

public:
    SelectedBrainConnectionViewModel(AiBrain &aiBrain, QObject *parent = nullptr);

    QString bannerText() const;
    bool brainIsConnectedAndActive() const;
    bool connectionIsBeingChecked() const;
    QString statusDetailText() const;
    QString selectedProviderKindName() const;
    int connectionRevision() const;

    bool connectionRefusalIsShowing() const;
    QString connectionRefusalTitle() const;
    QString connectionRefusalReason() const;
    QString connectionRefusalNextStep() const;

    // The user has read it. Nothing else is undone — the brain stays
    // unselected because it genuinely did not answer.
    Q_INVOKABLE void dismissConnectionRefusal();

    // --- Per-provider, for the Settings checkboxes -----------------------

    // Can this brain be chosen at all right now? False when its client isn't
    // written yet or it holds no key — the checkbox is then unavailable,
    // because Job Crush never offers a brain that cannot work.
    Q_INVOKABLE bool providerCanBeSelected(const QString &providerKindName) const;

    // Is this the chosen brain? Goes false again if a check fails, so a tick
    // never claims more than the truth.
    Q_INVOKABLE bool providerIsSelected(const QString &providerKindName) const;

    // Chosen AND verified live — the tick turns green on this.
    Q_INVOKABLE bool providerIsSelectedAndActive(const QString &providerKindName) const;

    // Plain speech for the line beside an unavailable checkbox: what this
    // brain is waiting on. Facts about the situation, never about the user.
    Q_INVOKABLE QString reasonProviderCannotBeSelected(const QString &providerKindName) const;

    // The checkbox itself: ticking chooses this brain and immediately checks
    // it; un-ticking leaves Job Crush with no active brain, which is a
    // legitimate choice and not an error.
    Q_INVOKABLE void setProviderSelected(const QString &providerKindName, bool shouldBeSelected);

    // A key moment has arrived (a screen that shows this state just opened).
    // A cached result answers instantly; only a real change costs a request.
    Q_INVOKABLE void checkConnectionNow();

signals:
    void connectionChanged();
    void connectionRefusalChanged();

private:
    AiBrain &brain;
    int connectionChangeCounter = 0;

    bool refusalIsShowing = false;
    QString refusalVendorName;
    QString refusalReasonText;
    QString refusalNextStepText;
};
