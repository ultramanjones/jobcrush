#pragma once

#include <QString>
#include <QStringList>

// KnownWorkLocations
//
// The places the location box suggests as the user types. Deliberately a
// plain built-in list rather than a lookup service: it works offline, costs
// nothing, answers instantly, and sends nowhere what someone is telling this
// app about their own life.
//
// It does not need to be exhaustive. Anything typed that isn't here is still
// accepted as-is — the suggestions are a convenience, never a gate. That
// distinction matters: an autocomplete that refuses unfamiliar input is a
// form that tells the user they live in the wrong place.
inline QStringList knownWorkLocations()
{
    return {
        // How people describe not going anywhere.
        QStringLiteral("Remote"),
        QStringLiteral("Remote (US)"),
        QStringLiteral("Remote (Worldwide)"),
        QStringLiteral("Hybrid"),
        QStringLiteral("Anywhere"),

        // Countries and regions that show up most on these boards.
        QStringLiteral("United States"),
        QStringLiteral("Canada"),
        QStringLiteral("United Kingdom"),
        QStringLiteral("Ireland"),
        QStringLiteral("Germany"),
        QStringLiteral("France"),
        QStringLiteral("Netherlands"),
        QStringLiteral("Spain"),
        QStringLiteral("Portugal"),
        QStringLiteral("Poland"),
        QStringLiteral("Sweden"),
        QStringLiteral("Norway"),
        QStringLiteral("Denmark"),
        QStringLiteral("Switzerland"),
        QStringLiteral("Austria"),
        QStringLiteral("Italy"),
        QStringLiteral("Australia"),
        QStringLiteral("New Zealand"),
        QStringLiteral("India"),
        QStringLiteral("Japan"),
        QStringLiteral("Singapore"),
        QStringLiteral("Mexico"),
        QStringLiteral("Brazil"),
        QStringLiteral("Europe"),

        // US states.
        QStringLiteral("Alabama"), QStringLiteral("Alaska"), QStringLiteral("Arizona"),
        QStringLiteral("Arkansas"), QStringLiteral("California"), QStringLiteral("Colorado"),
        QStringLiteral("Connecticut"), QStringLiteral("Delaware"), QStringLiteral("Florida"),
        QStringLiteral("Georgia"), QStringLiteral("Hawaii"), QStringLiteral("Idaho"),
        QStringLiteral("Illinois"), QStringLiteral("Indiana"), QStringLiteral("Iowa"),
        QStringLiteral("Kansas"), QStringLiteral("Kentucky"), QStringLiteral("Louisiana"),
        QStringLiteral("Maine"), QStringLiteral("Maryland"), QStringLiteral("Massachusetts"),
        QStringLiteral("Michigan"), QStringLiteral("Minnesota"), QStringLiteral("Mississippi"),
        QStringLiteral("Missouri"), QStringLiteral("Montana"), QStringLiteral("Nebraska"),
        QStringLiteral("Nevada"), QStringLiteral("New Hampshire"), QStringLiteral("New Jersey"),
        QStringLiteral("New Mexico"), QStringLiteral("New York"), QStringLiteral("North Carolina"),
        QStringLiteral("North Dakota"), QStringLiteral("Ohio"), QStringLiteral("Oklahoma"),
        QStringLiteral("Oregon"), QStringLiteral("Pennsylvania"), QStringLiteral("Rhode Island"),
        QStringLiteral("South Carolina"), QStringLiteral("South Dakota"), QStringLiteral("Tennessee"),
        QStringLiteral("Texas"), QStringLiteral("Utah"), QStringLiteral("Vermont"),
        QStringLiteral("Virginia"), QStringLiteral("Washington"), QStringLiteral("Washington, DC"),
        QStringLiteral("West Virginia"), QStringLiteral("Wisconsin"), QStringLiteral("Wyoming"),

        // US cities people actually job-hunt in, written the way boards write
        // them — with the state, comma and all. The whole point of chips is
        // that a comma inside an entry is no longer a problem.
        QStringLiteral("Atlanta, GA"), QStringLiteral("Austin, TX"),
        QStringLiteral("Baltimore, MD"), QStringLiteral("Boston, MA"),
        QStringLiteral("Boulder, CO"), QStringLiteral("Charlotte, NC"),
        QStringLiteral("Chicago, IL"), QStringLiteral("Cincinnati, OH"),
        QStringLiteral("Cleveland, OH"), QStringLiteral("Columbus, OH"),
        QStringLiteral("Dallas, TX"), QStringLiteral("Denver, CO"),
        QStringLiteral("Detroit, MI"), QStringLiteral("Durham, NC"),
        QStringLiteral("Fort Worth, TX"), QStringLiteral("Houston, TX"),
        QStringLiteral("Indianapolis, IN"), QStringLiteral("Jacksonville, FL"),
        QStringLiteral("Kansas City, MO"), QStringLiteral("Las Vegas, NV"),
        QStringLiteral("Los Angeles, CA"), QStringLiteral("Madison, WI"),
        QStringLiteral("Miami, FL"), QStringLiteral("Milwaukee, WI"),
        QStringLiteral("Minneapolis, MN"), QStringLiteral("Nashville, TN"),
        QStringLiteral("New Orleans, LA"), QStringLiteral("New York, NY"),
        QStringLiteral("Oakland, CA"), QStringLiteral("Oklahoma City, OK"),
        QStringLiteral("Omaha, NE"), QStringLiteral("Orlando, FL"),
        QStringLiteral("Philadelphia, PA"), QStringLiteral("Phoenix, AZ"),
        QStringLiteral("Pittsburgh, PA"), QStringLiteral("Portland, OR"),
        QStringLiteral("Raleigh, NC"), QStringLiteral("Richmond, VA"),
        QStringLiteral("Sacramento, CA"), QStringLiteral("Salt Lake City, UT"),
        QStringLiteral("San Antonio, TX"), QStringLiteral("San Diego, CA"),
        QStringLiteral("San Francisco, CA"), QStringLiteral("San Jose, CA"),
        QStringLiteral("Seattle, WA"), QStringLiteral("St. Louis, MO"),
        QStringLiteral("Tampa, FL"), QStringLiteral("Tucson, AZ"),

        // Elsewhere, for the boards that reach past the US.
        QStringLiteral("Amsterdam"), QStringLiteral("Barcelona"), QStringLiteral("Berlin"),
        QStringLiteral("Dublin"), QStringLiteral("Edinburgh"), QStringLiteral("Hamburg"),
        QStringLiteral("Lisbon"), QStringLiteral("London"), QStringLiteral("Madrid"),
        QStringLiteral("Manchester"), QStringLiteral("Melbourne"), QStringLiteral("Milan"),
        QStringLiteral("Montreal"), QStringLiteral("Munich"), QStringLiteral("Paris"),
        QStringLiteral("Stockholm"), QStringLiteral("Sydney"), QStringLiteral("Toronto"),
        QStringLiteral("Vancouver"), QStringLiteral("Vienna"), QStringLiteral("Warsaw"),
        QStringLiteral("Zurich"),
    };
}

// The suggestions for what has been typed so far, best first.
//
// Ranking rule: entries that START with what was typed come before entries
// that merely contain it, because someone typing "Pit" means Pittsburgh, not
// a job in "Pit Crew Operations". Already-chosen places are left out.
inline QStringList workLocationSuggestionsFor(const QString &typedPrefix,
                                              const QStringList &alreadyChosenLocations,
                                              int maximumSuggestions = 8)
{
    const QString loweredPrefix = typedPrefix.trimmed().toLower();
    if (loweredPrefix.isEmpty()) {
        return QStringList();
    }

    QStringList startsWithMatches;
    QStringList containsMatches;

    for (const QString &knownLocation : knownWorkLocations()) {
        if (alreadyChosenLocations.contains(knownLocation, Qt::CaseInsensitive)) {
            continue;
        }
        const QString loweredLocation = knownLocation.toLower();
        if (loweredLocation.startsWith(loweredPrefix)) {
            startsWithMatches.append(knownLocation);
        } else if (loweredLocation.contains(loweredPrefix)) {
            containsMatches.append(knownLocation);
        }
    }

    return (startsWithMatches + containsMatches).mid(0, maximumSuggestions);
}
