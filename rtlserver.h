/*
 * This file is part of NRAConnector
 * written by Christian Daniel 2016 -- <dg2ndk@afuz.org>
 *
 * The MIT License (MIT)
 * Copyright (c) 2016 Amateurfunk Unterfranken e.V.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * File contents: RTL-TCP sample conversion and protocol implementation
 */

#ifndef INCLUDE_RTLSERVER_H
#define INCLUDE_RTLSERVER_H

#include <QObject>
#include <QTcpServer>
#include "dsptypes.h"
#include "sseinterpolator.h"

class RTLServer : public QObject {
	Q_OBJECT

public:
	explicit RTLServer(QObject* parent = nullptr);

	bool open(const QHostAddress& rtlListenAddress, quint16 rtlListenPort);
	const QString& errorString() const { return m_errorString; }
	void close();

	void relaySamples(uint sampleRate, const IQSampleS16* samples, size_t sampleCount, int shift);

signals:
	void onSetFCenter(quint32 fCenter);

protected:
#pragma pack(push, 1)
	struct RTLDongleInfo {
		quint8 magic[4];
		quint32 tunerType;
		quint32 tunerGainCount;
	} __attribute__((packed));

	struct RTLCommand {
		quint8 cmd;
		quint32 param;
	} __attribute__((packed));
#pragma pack(pop)

	QTcpServer m_rtlServer;
	QHostAddress m_rtlListenAddress;
	quint16 m_rtlListenPort;
	QTcpSocket* m_rtlSocket;
	QString m_errorString;
	Real m_nraSampleRate;
	Real m_rtlSampleRate;
	SSEInterpolator m_interpolator;
	Real m_interpolatorDistance;
	Real m_interpolatorDistanceRemain;
	quint8 m_buffer[4096];
	int m_bufferFill;

protected slots:
	void handleRTLServerNewConnection();
	void handleRTLConnectionState(QAbstractSocket::SocketState socketState);
	void handleRTLConnectionError(QAbstractSocket::SocketError);
	void handleRTLConnectionReadyRead();
};

#endif // INCLUDE_RTLSERVER_H
