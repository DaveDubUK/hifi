//
//  DatagramProcessor.h
//  interface/src
//
//  Created by Stephen Birarda on 1/23/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_DatagramProcessor_h
#define hifi_DatagramProcessor_h

#include <QtCore/QObject>

class DatagramProcessor : public QObject {
    Q_OBJECT
public:
    DatagramProcessor(QObject* parent = 0);
    
    int getInPacketCount() const { return _inPacketCount; }
    int getOutPacketCount() const { return _outPacketCount; }
    int getInByteCount() const { return _inByteCount; }
    int getOutByteCount() const { return _outByteCount; }
    
    void resetCounters() { _inPacketCount = 0; _outPacketCount = 0; _inByteCount = 0; _outByteCount = 0; }

    void shutdown() { _isShuttingDown = true; }
public slots:
    void processDatagrams();
    
private:
    int _inPacketCount = 0;
    int _outPacketCount = 0;
    int _inByteCount = 0;
    int _outByteCount = 0;
    bool _isShuttingDown = false;
};

#endif // hifi_DatagramProcessor_h
