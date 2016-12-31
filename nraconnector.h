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
 * File contents: NRA networking and protocol implementation
 */

#ifndef INCLUDE_NRACONNECTOR_H
#define INCLUDE_NRACONNECTOR_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include "rtlserver.h"

class NRAConnector : public QObject {
	Q_OBJECT

public:
	enum ConnectorState {
		CSIdle,
		CSError,
		CSRunning
	};

	struct RBWEntry {
		QString displayString;
		float valueHz;

		RBWEntry(const QString& _displayString, float _valueHz) :
			displayString(_displayString),
			valueHz(_valueHz)
		{ }
	};
	typedef QList<RBWEntry> RBWList;

	struct ReferenceLevelEntry {
		QString displayString;
		float valueV;

		ReferenceLevelEntry(const QString& _displayString, float _valueV) :
			displayString(_displayString),
			valueV(_valueV)
		{ }
	};
	typedef QList<ReferenceLevelEntry> ReferenceLevelList;

	explicit NRAConnector(QObject* parent = nullptr);

	static void registerTypes();

public slots:
	void start(const QHostAddress& nraAddress, quint16 nraPort, quint16 nraStreamPort, const QHostAddress& rtlListenAddress, quint16 rtlListenPort);
	void stop();
	void setFCenter(quint32 fcent);
	void setReferenceLevel(float rl);
	void setAttenuation(float att);
	void setDigitalAttenuation(float att);

signals:
	void onStateReport(ConnectorState state, const QString& text);
	void onDeviceInfo(const QString& productName, const QString& serial);
	void onRBWList(const RBWList& rbwList);
	void onReferenceLevelList(const ReferenceLevelList& rlList);
	void onStreamRate(int rate);

protected:
	enum NRAState {
		NRAIdle,
		NRAConnecting,
		NRACmdRemoteNewline,
		NRACmdRemoteOn,
		NRACmdDevInfo,
		NRACmdModeIQStream,
		NRACmdStreamSetup,
		NRACmdIQStreamRBWList,
		NRACmdRLList,
		NRACmdIQStreamRBW,
		NRACmdStreamState,
		NRAStream,
		NRARunning,
		NRAExecutingCommand
	};

	enum StreamState {
		StrHeader,
		StrSkip,
		StrContext,
		StrSamples
	};

#pragma pack(push, 1)
	struct NRAStreamHeader {
		quint16 byteOrder;
		quint16 headerVersion;
		quint16 streamID;
		quint16 streamVersion;
		quint16 reserved1;
		quint16 reserved2;
		quint32 packetCounter;
		quint32 sizeOfContext;
		quint32 numberOfItems;
		quint32 sizeOfItem;
		quint32 reserved3;
	} __attribute__((packed));

	struct NRAStreamContext {
		quint32 integerSeconds;
		quint32 fractionalSeconds;
		quint32 eventFlags;
		quint32 changeFlags;
		quint16 dataItemFormat;
		quint16 unit;
		float scaleToUnit;
		float sampleRate;
		float rbw;
		double fCent;
		float rl;
		float attenuator;
		float temperature;
	} __attribute__((packed));
#pragma pack(pop)

	RTLServer m_rtlServer;

	QTcpSocket m_nraConnection;
	QTcpSocket m_nraStream;
	QHostAddress m_nraAddress;
	quint16 m_nraPort;
	quint16 m_nraStreamPort;
	NRAState m_nraState;
	RBWList m_rbwList;
	ReferenceLevelList m_rlList;
	int m_lineCount;
	int m_lineNo;
	QTimer m_streamRateTimer;
	int m_streamRate;
	StreamState m_streamState;
	uint m_streamExpect;
	NRAStreamHeader m_streamHeader;
	NRAStreamContext m_streamContext;
	uint m_sampleRate;
	uint m_sampleBlockSize;
	QByteArray m_sampleBuffer;
	uint m_sampleBufferFill;
	bool m_newFCentPending;
	quint32 m_newFCent;
	bool m_newReferenceLevelPending;
	float m_newReferenceLevel;
	bool m_newAttenuationPending;
	float m_newAttenuation;
	int m_digitalAttenuation;

	static const char* getErrorString(int errorCode);
	int readNRAReturnCode();
	int readNRADevInfo();
	int readNRARBWList(bool* more);
	int readNRARLList(bool* more);
	void handleNRAError(const QString& text);
	bool handleStreamHeader();
	bool handleStreamContext();
	bool handleStreamSamples();
	void relaySamples(uint sampleRate, const IQSampleS16* samples, size_t sampleCount);

	void sendNextCommand();

protected slots:
	void handleNRAConnectionState(QAbstractSocket::SocketState socketState);
	void handleNRAConnectionError(QAbstractSocket::SocketError);
	void handleNRAConnectionReadyRead();

	void handleNRAStreamState(QAbstractSocket::SocketState socketState);
	void handleNRAStreamError(QAbstractSocket::SocketError);
	void handleNRAStreamReadyRead();

	void handleStreamRateTimer();
};

#endif // INCLUDE_NRACONNECTOR_H
