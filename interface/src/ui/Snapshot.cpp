//
//  Snapshot.cpp
//  interface/src/ui
//
//  Created by Stojce Slavkovski on 1/26/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryFile>
#include <QUrl>

#include <AccountManager.h>
#include <AddressManager.h>
#include <avatar/AvatarManager.h>
#include <avatar/MyAvatar.h>
#include <FileUtils.h>
#include <GLCanvas.h>
#include <NodeList.h>

#include "Application.h"
#include "Snapshot.h"

// filename format: hifi-snap-by-%username%-on-%date%_%time%_@-%location%.jpg
// %1 <= username, %2 <= date and time, %3 <= current location
const QString FILENAME_PATH_FORMAT = "hifi-snap-by-%1-on-%2.jpg";

const QString DATETIME_FORMAT = "yyyy-MM-dd_hh-mm-ss";
const QString SNAPSHOTS_DIRECTORY = "Snapshots";

const QString URL = "highfidelity_url";

Setting::Handle<QString> Snapshot::snapshotsLocation("snapshotsLocation",
    QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));

SnapshotMetaData* Snapshot::parseSnapshotData(QString snapshotPath) {

    if (!QFile(snapshotPath).exists()) {
        return NULL;
    }

    QImage shot(snapshotPath);

    // no location data stored
    if (shot.text(URL).isEmpty()) {
        return NULL;
    }

    // parsing URL
    QUrl url = QUrl(shot.text(URL), QUrl::ParsingMode::StrictMode);

    SnapshotMetaData* data = new SnapshotMetaData();
    data->setURL(url);

    return data;
}

QString Snapshot::saveSnapshot(QImage image) {

    QFile* snapshotFile = savedFileForSnapshot(image, false);

    // we don't need the snapshot file, so close it, grab its filename and delete it
    snapshotFile->close();

    QString snapshotPath = QFileInfo(*snapshotFile).absoluteFilePath();

    delete snapshotFile;

    return snapshotPath;
}

QTemporaryFile* Snapshot::saveTempSnapshot(QImage image) {
    // return whatever we get back from saved file for snapshot
    return static_cast<QTemporaryFile*>(savedFileForSnapshot(image, true));
}

QFile* Snapshot::savedFileForSnapshot(QImage & shot, bool isTemporary) {

    // adding URL to snapshot
    QUrl currentURL = DependencyManager::get<AddressManager>()->currentAddress();
    shot.setText(URL, currentURL.toString());

    QString username = AccountManager::getInstance().getAccountInfo().getUsername();
    // normalize username, replace all non alphanumeric with '-'
    username.replace(QRegExp("[^A-Za-z0-9_]"), "-");

    QDateTime now = QDateTime::currentDateTime();

    QString filename = FILENAME_PATH_FORMAT.arg(username, now.toString(DATETIME_FORMAT));

    const int IMAGE_QUALITY = 100;

    if (!isTemporary) {
        QString snapshotFullPath = snapshotsLocation.get();

        if (!snapshotFullPath.endsWith(QDir::separator())) {
            snapshotFullPath.append(QDir::separator());
        }

        snapshotFullPath.append(filename);

        QFile* imageFile = new QFile(snapshotFullPath);
        imageFile->open(QIODevice::WriteOnly);

        shot.save(imageFile, 0, IMAGE_QUALITY);
        imageFile->close();

        return imageFile;

    } else {
        QTemporaryFile* imageTempFile = new QTemporaryFile(QDir::tempPath() + "/XXXXXX-" + filename);

        if (!imageTempFile->open()) {
            qDebug() << "Unable to open QTemporaryFile for temp snapshot. Will not save.";
            return NULL;
        }

        shot.save(imageTempFile, 0, IMAGE_QUALITY);
        imageTempFile->close();

        return imageTempFile;
    }
}


