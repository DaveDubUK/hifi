//
//  AddressManager.cpp
//  libraries/networking/src
//
//  Created by Stephen Birarda on 2014-09-10.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QJsonDocument>
#include <QRegExp>
#include <QStringList>

#include <GLMHelpers.h>
#include <SettingHandle.h>
#include <UUID.h>

#include "NodeList.h"
#include "NetworkLogging.h"
#include "AddressManager.h"

const QString ADDRESS_MANAGER_SETTINGS_GROUP = "AddressManager";
const QString SETTINGS_CURRENT_ADDRESS_KEY = "address";

Setting::Handle<QUrl> currentAddressHandle(QStringList() << ADDRESS_MANAGER_SETTINGS_GROUP << "address", DEFAULT_HIFI_ADDRESS);

AddressManager::AddressManager() :
    _host(),
    _rootPlaceID(),
    _positionGetter(NULL),
    _orientationGetter(NULL)
{

}

bool AddressManager::isConnected() {
    return DependencyManager::get<NodeList>()->getDomainHandler().isConnected();
}

const QUrl AddressManager::currentAddress() const {
    QUrl hifiURL;

    hifiURL.setScheme(HIFI_URL_SCHEME);
    hifiURL.setHost(_host);
    hifiURL.setPath(currentPath());

    return hifiURL;
}

void AddressManager::loadSettings(const QString& lookupString) {
    if (lookupString.isEmpty()) {
        handleUrl(currentAddressHandle.get().toString(), LookupTrigger::StartupFromSettings);
    } else {
        handleUrl(lookupString, LookupTrigger::StartupFromSettings);
    }
}

void AddressManager::goBack() {
    if (_backStack.size() > 0) {
        // go to that address
        handleUrl(_backStack.pop(), LookupTrigger::Back);

        if (_backStack.size() == 0) {
            // the back stack is now empty so it is no longer possible to go back - emit that signal
            emit goBackPossible(false);
        }
    }
}

void AddressManager::goForward() {
    if (_forwardStack.size() > 0) {
        // pop a URL from the forwardStack and go to that address
        handleUrl(_forwardStack.pop(), LookupTrigger::Forward);

        if (_forwardStack.size() == 0) {
            // the forward stack is empty so it is no longer possible to go forwards - emit that signal
            emit goForwardPossible(false);
        }
    }
}

void AddressManager::storeCurrentAddress() {
    currentAddressHandle.set(currentAddress());
}

const QString AddressManager::currentPath(bool withOrientation) const {

    if (_positionGetter) {
        QString pathString = "/" + createByteArray(_positionGetter());

        if (withOrientation) {
            if (_orientationGetter) {
                QString orientationString = createByteArray(_orientationGetter());
                pathString += "/" + orientationString;
            } else {
                qCDebug(networking) << "Cannot add orientation to path without a getter for position."
                    << "Call AddressManager::setOrientationGetter to pass a function that will return a glm::quat";
            }

        }

        return pathString;
    } else {
        qCDebug(networking) << "Cannot create address path without a getter for position."
            << "Call AddressManager::setPositionGetter to pass a function that will return a const glm::vec3&";
        return QString();
    }
}

const JSONCallbackParameters& AddressManager::apiCallbackParameters() {
    static bool hasSetupParameters = false;
    static JSONCallbackParameters callbackParams;

    if (!hasSetupParameters) {
        callbackParams.jsonCallbackReceiver = this;
        callbackParams.jsonCallbackMethod = "handleAPIResponse";
        callbackParams.errorCallbackReceiver = this;
        callbackParams.errorCallbackMethod = "handleAPIError";
    }

    return callbackParams;
}

bool AddressManager::handleUrl(const QUrl& lookupUrl, LookupTrigger trigger) {
    if (lookupUrl.scheme() == HIFI_URL_SCHEME) {

        qCDebug(networking) << "Trying to go to URL" << lookupUrl.toString();

        DependencyManager::get<NodeList>()->flagTimeForConnectionStep(LimitedNodeList::ConnectionStep::LookupAddress);

        // there are 4 possible lookup strings

        // 1. global place name (name of domain or place) - example: sanfrancisco
        // 2. user name (prepended with @) - example: @philip
        // 3. location string (posX,posY,posZ/eulerX,eulerY,eulerZ)
        // 4. domain network address (IP or dns resolvable hostname)

        // use our regex'ed helpers to figure out what we're supposed to do with this
        if (!handleUsername(lookupUrl.authority())) {
            // we're assuming this is either a network address or global place name
            // check if it is a network address first
            if (handleNetworkAddress(lookupUrl.host()
                                      + (lookupUrl.port() == -1 ? "" : ":" + QString::number(lookupUrl.port())), trigger)) {
                // we may have a path that defines a relative viewpoint - if so we should jump to that now
                handlePath(lookupUrl.path(), trigger);
            } else if (handleDomainID(lookupUrl.host())){
                // no place name - this is probably a domain ID
                // try to look up the domain ID on the metaverse API
                attemptDomainIDLookup(lookupUrl.host(), lookupUrl.path(), trigger);
            } else {
                // wasn't an address - lookup the place name
                // we may have a path that defines a relative viewpoint - pass that through the lookup so we can go to it after
                attemptPlaceNameLookup(lookupUrl.host(), lookupUrl.path(), trigger);
            }
        }

        return true;
    } else if (lookupUrl.toString().startsWith('/')) {
        qCDebug(networking) << "Going to relative path" << lookupUrl.path();

        // if this is a relative path then handle it as a relative viewpoint
        handlePath(lookupUrl.path(), trigger, true);
        emit lookupResultsFinished();

        return true;
    }

    return false;
}

void AddressManager::handleLookupString(const QString& lookupString) {
    if (!lookupString.isEmpty()) {
        // make this a valid hifi URL and handle it off to handleUrl
        QString sanitizedString = lookupString.trimmed();
        QUrl lookupURL;

        if (!lookupString.startsWith('/')) {
            const QRegExp HIFI_SCHEME_REGEX = QRegExp(HIFI_URL_SCHEME + ":\\/{1,2}", Qt::CaseInsensitive);
            sanitizedString = sanitizedString.remove(HIFI_SCHEME_REGEX);

            lookupURL = QUrl(HIFI_URL_SCHEME + "://" + sanitizedString);
        } else {
            lookupURL = QUrl(lookupString);
        }

        handleUrl(lookupURL);
    }
}

const QString DATA_OBJECT_DOMAIN_KEY = "domain";


void AddressManager::handleAPIResponse(QNetworkReply& requestReply) {
    QJsonObject responseObject = QJsonDocument::fromJson(requestReply.readAll()).object();
    QJsonObject dataObject = responseObject["data"].toObject();

    if (!dataObject.isEmpty()) {
        goToAddressFromObject(dataObject.toVariantMap(), requestReply);
    } else if (responseObject.contains(DATA_OBJECT_DOMAIN_KEY)) {
        goToAddressFromObject(responseObject.toVariantMap(), requestReply);
    }

    emit lookupResultsFinished();
}

const char OVERRIDE_PATH_KEY[] = "override_path";
const char LOOKUP_TRIGGER_KEY[] = "lookup_trigger";

void AddressManager::goToAddressFromObject(const QVariantMap& dataObject, const QNetworkReply& reply) {

    const QString DATA_OBJECT_PLACE_KEY = "place";
    const QString DATA_OBJECT_USER_LOCATION_KEY = "location";

    QVariantMap locationMap;
    if (dataObject.contains(DATA_OBJECT_PLACE_KEY)) {
        locationMap = dataObject[DATA_OBJECT_PLACE_KEY].toMap();
    } else if (dataObject.contains(DATA_OBJECT_DOMAIN_KEY)) {
        locationMap = dataObject;
    } else {
        locationMap = dataObject[DATA_OBJECT_USER_LOCATION_KEY].toMap();
    }

    if (!locationMap.isEmpty()) {
        const QString LOCATION_API_ROOT_KEY = "root";
        const QString LOCATION_API_DOMAIN_KEY = "domain";
        const QString LOCATION_API_ONLINE_KEY = "online";

        if (!locationMap.contains(LOCATION_API_ONLINE_KEY)
            || locationMap[LOCATION_API_ONLINE_KEY].toBool()) {

            QVariantMap rootMap = locationMap[LOCATION_API_ROOT_KEY].toMap();
            if (rootMap.isEmpty()) {
                rootMap = locationMap;
            }

            QVariantMap domainObject = rootMap[LOCATION_API_DOMAIN_KEY].toMap();

            if (!domainObject.isEmpty()) {
                const QString DOMAIN_NETWORK_ADDRESS_KEY = "network_address";
                const QString DOMAIN_NETWORK_PORT_KEY = "network_port";
                const QString DOMAIN_ICE_SERVER_ADDRESS_KEY = "ice_server_address";

                DependencyManager::get<NodeList>()->flagTimeForConnectionStep(LimitedNodeList::ConnectionStep::HandleAddress);

                const QString DOMAIN_ID_KEY = "id";
                QString domainIDString = domainObject[DOMAIN_ID_KEY].toString();
                QUuid domainID(domainIDString);

                if (domainObject.contains(DOMAIN_NETWORK_ADDRESS_KEY)) {
                    QString domainHostname = domainObject[DOMAIN_NETWORK_ADDRESS_KEY].toString();

                    quint16 domainPort = domainObject.contains(DOMAIN_NETWORK_PORT_KEY)
                        ? domainObject[DOMAIN_NETWORK_PORT_KEY].toUInt()
                        : DEFAULT_DOMAIN_SERVER_PORT;

                    qCDebug(networking) << "Possible domain change required to connect to" << domainHostname
                        << "on" << domainPort;
                    emit possibleDomainChangeRequired(domainHostname, domainPort);
                } else {
                    QString iceServerAddress = domainObject[DOMAIN_ICE_SERVER_ADDRESS_KEY].toString();

                    qCDebug(networking) << "Possible domain change required to connect to domain with ID" << domainID
                        << "via ice-server at" << iceServerAddress;

                    emit possibleDomainChangeRequiredViaICEForID(iceServerAddress, domainID);
                }

                LookupTrigger trigger = (LookupTrigger) reply.property(LOOKUP_TRIGGER_KEY).toInt();

                // set our current root place id to the ID that came back
                const QString PLACE_ID_KEY = "id";
                _rootPlaceID = rootMap[PLACE_ID_KEY].toUuid();

                // set our current root place name to the name that came back
                const QString PLACE_NAME_KEY = "name";
                QString placeName = rootMap[PLACE_NAME_KEY].toString();
                if (!placeName.isEmpty()) {
                    setHost(placeName, trigger);
                } else {
                    setHost(domainIDString, trigger);
                }

                // check if we had a path to override the path returned
                QString overridePath = reply.property(OVERRIDE_PATH_KEY).toString();

                if (!overridePath.isEmpty()) {
                    handlePath(overridePath, trigger);
                } else {
                    // take the path that came back
                    const QString PLACE_PATH_KEY = "path";
                    QString returnedPath = locationMap[PLACE_PATH_KEY].toString();

                    bool shouldFaceViewpoint = locationMap.contains(LOCATION_API_ONLINE_KEY);

                    if (!returnedPath.isEmpty()) {
                        if (shouldFaceViewpoint) {
                            // try to parse this returned path as a viewpoint, that's the only thing it could be for now
                            if (!handleViewpoint(returnedPath, shouldFaceViewpoint)) {
                                qCDebug(networking) << "Received a location path that was could not be handled as a viewpoint -"
                                    << returnedPath;
                            }
                        } else {
                            handlePath(returnedPath, trigger);
                        }
                    } else {
                        // we're going to hit the index path, set that as the _newHostLookupPath
                        _newHostLookupPath = INDEX_PATH;

                        // we didn't override the path or get one back - ask the DS for the viewpoint of its index path
                        // which we will jump to if it exists
                        emit pathChangeRequired(INDEX_PATH);
                    }
                }

            } else {
                qCDebug(networking) << "Received an address manager API response with no domain key. Cannot parse.";
                qCDebug(networking) << locationMap;
            }
        } else {
            // we've been told that this result exists but is offline, emit our signal so the application can handle
            emit lookupResultIsOffline();
        }
    } else {
        qCDebug(networking) << "Received an address manager API response with no location key or place key. Cannot parse.";
        qCDebug(networking) << locationMap;
    }
}

void AddressManager::handleAPIError(QNetworkReply& errorReply) {
    qCDebug(networking) << "AddressManager API error -" << errorReply.error() << "-" << errorReply.errorString();

    if (errorReply.error() == QNetworkReply::ContentNotFoundError) {
        emit lookupResultIsNotFound();
    }
    emit lookupResultsFinished();
}

const QString GET_PLACE = "/api/v1/places/%1";

void AddressManager::attemptPlaceNameLookup(const QString& lookupString, const QString& overridePath, LookupTrigger trigger) {
    // assume this is a place name and see if we can get any info on it
    QString placeName = QUrl::toPercentEncoding(lookupString);

    QVariantMap requestParams;

    // if the user asked for a specific path with this lookup then keep it with the request so we can use it later
    if (!overridePath.isEmpty()) {
        requestParams.insert(OVERRIDE_PATH_KEY, overridePath);
    }

    // remember how this lookup was triggered for history storage handling later
    requestParams.insert(LOOKUP_TRIGGER_KEY, static_cast<int>(trigger));

    AccountManager::getInstance().sendRequest(GET_PLACE.arg(placeName),
                                              AccountManagerAuth::None,
                                              QNetworkAccessManager::GetOperation,
                                              apiCallbackParameters(),
                                              QByteArray(), NULL, requestParams);
}

const QString GET_DOMAIN_ID = "/api/v1/domains/%1";

void AddressManager::attemptDomainIDLookup(const QString& lookupString, const QString& overridePath, LookupTrigger trigger) {
    // assume this is a domain ID and see if we can get any info on it
    QString domainID = QUrl::toPercentEncoding(lookupString);

    QVariantMap requestParams;

    // if the user asked for a specific path with this lookup then keep it with the request so we can use it later
    if (!overridePath.isEmpty()) {
        requestParams.insert(OVERRIDE_PATH_KEY, overridePath);
    }

    // remember how this lookup was triggered for history storage handling later
    requestParams.insert(LOOKUP_TRIGGER_KEY, static_cast<int>(trigger));

    AccountManager::getInstance().sendRequest(GET_DOMAIN_ID.arg(domainID),
                                                AccountManagerAuth::None,
                                                QNetworkAccessManager::GetOperation,
                                                apiCallbackParameters(),
                                                QByteArray(), NULL, requestParams);
}

bool AddressManager::handleNetworkAddress(const QString& lookupString, LookupTrigger trigger) {
    const QString IP_ADDRESS_REGEX_STRING = "^((?:(?:[0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5])\\.){3}"
        "(?:[0-9]|[1-9][0-9]|1[0-9]{2}|2[0-4][0-9]|25[0-5]))(?::(\\d{1,5}))?$";

    const QString HOSTNAME_REGEX_STRING = "^((?:[A-Z0-9]|[A-Z0-9][A-Z0-9\\-]{0,61}[A-Z0-9])"
        "(?:\\.(?:[A-Z0-9]|[A-Z0-9][A-Z0-9\\-]{0,61}[A-Z0-9]))+|localhost)(?::(\\d{1,5}))?$";

    QRegExp ipAddressRegex(IP_ADDRESS_REGEX_STRING);

    if (ipAddressRegex.indexIn(lookupString) != -1) {
        QString domainIPString = ipAddressRegex.cap(1);

        qint16 domainPort = DEFAULT_DOMAIN_SERVER_PORT;
        if (!ipAddressRegex.cap(2).isEmpty()) {
            domainPort = (qint16) ipAddressRegex.cap(2).toInt();
        }

        emit lookupResultsFinished();
        setDomainInfo(domainIPString, domainPort, trigger);

        return true;
    }

    QRegExp hostnameRegex(HOSTNAME_REGEX_STRING, Qt::CaseInsensitive);

    if (hostnameRegex.indexIn(lookupString) != -1) {
        QString domainHostname = hostnameRegex.cap(1);

        quint16 domainPort = DEFAULT_DOMAIN_SERVER_PORT;

        if (!hostnameRegex.cap(2).isEmpty()) {
            domainPort = (qint16)hostnameRegex.cap(2).toInt();
        }

        emit lookupResultsFinished();
        setDomainInfo(domainHostname, domainPort, trigger);

        return true;
    }

    return false;
}

bool AddressManager::handleDomainID(const QString& host) {
    const QString UUID_REGEX_STRING = "[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}";

    QRegExp domainIDRegex(UUID_REGEX_STRING, Qt::CaseInsensitive);

    return (domainIDRegex.indexIn(host) != -1);
}

void AddressManager::handlePath(const QString& path, LookupTrigger trigger, bool wasPathOnly) {
    if (!handleViewpoint(path, false, wasPathOnly)) {
        qCDebug(networking) << "User entered path could not be handled as a viewpoint - " << path <<
                            "- wll attempt to ask domain-server to resolve.";

        if (!wasPathOnly) {
            // if we received a path with a host then we need to remember what it was here so we can not
            // double set add to the history stack once handle viewpoint is called with the result
            _newHostLookupPath = path;
        } else {
            // clear the _newHostLookupPath so it doesn't match when this return comes in
            _newHostLookupPath = QString();
        }

        emit pathChangeRequired(path);
    }
}

bool AddressManager::handleViewpoint(const QString& viewpointString, bool shouldFace,
                                     bool definitelyPathOnly, const QString& pathString) {
    const QString FLOAT_REGEX_STRING = "([-+]?[0-9]*\\.?[0-9]+(?:[eE][-+]?[0-9]+)?)";
    const QString SPACED_COMMA_REGEX_STRING = "\\s*,\\s*";
    const QString POSITION_REGEX_STRING = QString("\\/") + FLOAT_REGEX_STRING + SPACED_COMMA_REGEX_STRING +
        FLOAT_REGEX_STRING + SPACED_COMMA_REGEX_STRING + FLOAT_REGEX_STRING + "\\s*(?:$|\\/)";
    const QString QUAT_REGEX_STRING = QString("\\/") + FLOAT_REGEX_STRING + SPACED_COMMA_REGEX_STRING +
        FLOAT_REGEX_STRING + SPACED_COMMA_REGEX_STRING + FLOAT_REGEX_STRING + SPACED_COMMA_REGEX_STRING +
        FLOAT_REGEX_STRING + "\\s*$";

    QRegExp positionRegex(POSITION_REGEX_STRING);

    if (positionRegex.indexIn(viewpointString) != -1) {
        // we have at least a position, so emit our signal to say we need to change position
        glm::vec3 newPosition(positionRegex.cap(1).toFloat(),
                              positionRegex.cap(2).toFloat(),
                              positionRegex.cap(3).toFloat());

        // We need to use definitelyPathOnly, pathString and _newHostLookupPath to determine if the current address
        // should be stored in the history before we ask for a position/orientation change. A relative path that was
        // not associated with a host lookup should always trigger a history change (definitelyPathOnly) and a viewpointString
        // with a non empty pathString (suggesting this is the result of a lookup with the domain-server) that does not match
        // _newHostLookupPath should always trigger a history change.
        //
        // We use _newHostLookupPath to determine if the client has already stored its last address
        // before moving to a new host thanks to the information in the same lookup URL.


        if (definitelyPathOnly || (!pathString.isEmpty() && pathString != _newHostLookupPath)) {
            addCurrentAddressToHistory(LookupTrigger::UserInput);
        }

        if (!isNaN(newPosition.x) && !isNaN(newPosition.y) && !isNaN(newPosition.z)) {
            glm::quat newOrientation;

            QRegExp orientationRegex(QUAT_REGEX_STRING);

            // we may also have an orientation
            if (viewpointString[positionRegex.matchedLength() - 1] == QChar('/')
                && orientationRegex.indexIn(viewpointString, positionRegex.matchedLength() - 1) != -1) {

                glm::quat newOrientation = glm::normalize(glm::quat(orientationRegex.cap(4).toFloat(),
                                                                    orientationRegex.cap(1).toFloat(),
                                                                    orientationRegex.cap(2).toFloat(),
                                                                    orientationRegex.cap(3).toFloat()));

                if (!isNaN(newOrientation.x) && !isNaN(newOrientation.y) && !isNaN(newOrientation.z)
                    && !isNaN(newOrientation.w)) {
                    emit locationChangeRequired(newPosition, true, newOrientation, shouldFace);
                    return true;
                } else {
                    qCDebug(networking) << "Orientation parsed from lookup string is invalid. Will not use for location change.";
                }
            }

            emit locationChangeRequired(newPosition, false, newOrientation, shouldFace);

        } else {
            qCDebug(networking) << "Could not jump to position from lookup string because it has an invalid value.";
        }

        return true;
    } else {
        return false;
    }
}

const QString GET_USER_LOCATION = "/api/v1/users/%1/location";

bool AddressManager::handleUsername(const QString& lookupString) {
    const QString USERNAME_REGEX_STRING = "^@(\\S+)";

    QRegExp usernameRegex(USERNAME_REGEX_STRING);

    if (usernameRegex.indexIn(lookupString) != -1) {
        goToUser(usernameRegex.cap(1));
        return true;
    }

    return false;
}

void AddressManager::setHost(const QString& host, LookupTrigger trigger) {
    if (host != _host) {

        // if the host is being changed we should store current address in the history
        addCurrentAddressToHistory(trigger);

        _host = host;
        emit hostChanged(_host);
    }
}


void AddressManager::setDomainInfo(const QString& hostname, quint16 port, LookupTrigger trigger) {
    setHost(hostname, trigger);

    _rootPlaceID = QUuid();

    qCDebug(networking) << "Possible domain change required to connect to domain at" << hostname << "on" << port;

    DependencyManager::get<NodeList>()->flagTimeForConnectionStep(LimitedNodeList::ConnectionStep::HandleAddress);

    emit possibleDomainChangeRequired(hostname, port);
}

void AddressManager::goToUser(const QString& username) {
    QString formattedUsername = QUrl::toPercentEncoding(username);

    // for history storage handling we remember how this lookup was trigged - for a username it's always user input
    QVariantMap requestParams;
    requestParams.insert(LOOKUP_TRIGGER_KEY, static_cast<int>(LookupTrigger::UserInput));

    // this is a username - pull the captured name and lookup that user's location
    AccountManager::getInstance().sendRequest(GET_USER_LOCATION.arg(formattedUsername),
                                              AccountManagerAuth::Optional,
                                              QNetworkAccessManager::GetOperation,
                                              apiCallbackParameters(),
                                              QByteArray(), nullptr, requestParams);
}

void AddressManager::copyAddress() {
    QApplication::clipboard()->setText(currentAddress().toString());
}

void AddressManager::copyPath() {
    QApplication::clipboard()->setText(currentPath());
}

void AddressManager::addCurrentAddressToHistory(LookupTrigger trigger) {

    // if we're cold starting and this is called for the first address (from settings) we don't do anything
    if (trigger != LookupTrigger::StartupFromSettings) {
        if (trigger == LookupTrigger::UserInput) {
            // anyime the user has manually looked up an address we know we should clear the forward stack
            _forwardStack.clear();

            emit goForwardPossible(false);
        }

        if (trigger == LookupTrigger::Back) {
            // we're about to push to the forward stack
            // if it's currently empty emit our signal to say that going forward is now possible
            if (_forwardStack.size() == 0) {
                emit goForwardPossible(true);
            }

            // when the user is going back, we move the current address to the forward stack
            // and do not but it into the back stack
            _forwardStack.push(currentAddress());
        } else {
            // we're about to push to the back stack
            // if it's currently empty emit our signal to say that going forward is now possible
            if (_forwardStack.size() == 0) {
                emit goBackPossible(true);
            }

            // unless this was triggered from the result of a named path lookup, add the current address to the history
            _backStack.push(currentAddress());
        }
    }
}
