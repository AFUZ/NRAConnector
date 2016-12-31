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

#include "nraconnector.h"

NRAConnector::NRAConnector(QObject* parent) :
	QObject(parent),
	m_nraConnection(),
	m_nraStream(),
	m_nraAddress(),
	m_nraPort(),
	m_nraState(NRAIdle),
	m_rbwList(),
	m_rlList(),
	m_digitalAttenuation(0)
{
	connect(&m_nraConnection, &QTcpSocket::stateChanged, this, &NRAConnector::handleNRAConnectionState);
	connect(&m_nraConnection, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(handleNRAConnectionError(QAbstractSocket::SocketError)));
	connect(&m_nraConnection, &QTcpSocket::readyRead, this, &NRAConnector::handleNRAConnectionReadyRead);

	connect(&m_nraStream, &QTcpSocket::stateChanged, this, &NRAConnector::handleNRAStreamState);
	connect(&m_nraStream, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(handleNRAStreamError(QAbstractSocket::SocketError)));
	connect(&m_nraStream, &QTcpSocket::readyRead, this, &NRAConnector::handleNRAStreamReadyRead);

	connect(&m_streamRateTimer, &QTimer::timeout, this, &NRAConnector::handleStreamRateTimer);

	connect(&m_rtlServer, &RTLServer::onSetFCenter, this, &NRAConnector::setFCenter);
}

void NRAConnector::registerTypes()
{
	qRegisterMetaType<ConnectorState>("ConnectorState");
	qRegisterMetaType<RBWList>("RBWList");
	qRegisterMetaType<ReferenceLevelList>("ReferenceLevelList");
}

void NRAConnector::start(const QHostAddress& nraAddress, quint16 nraPort, quint16 nraStreamPort, const QHostAddress& rtlListenAddress, quint16 rtlListenPort)
{
	m_rtlServer.close();
	m_nraConnection.abort();
	m_nraStream.abort();
	m_streamRateTimer.stop();

	m_nraAddress = nraAddress;
	m_nraPort = nraPort;
	m_nraStreamPort = nraStreamPort;

	if(!m_rtlServer.open(rtlListenAddress, rtlListenPort)) {
		emit onStateReport(CSError, tr("Could not listen on TCP %1:%2: %3").
						   arg(rtlListenAddress.toString()).
						   arg(rtlListenPort).
						   arg(m_rtlServer.errorString()));
		return;
	}

	m_nraConnection.connectToHost(m_nraAddress, m_nraPort, QTcpSocket::ReadWrite);
	m_nraState = NRAConnecting;
	emit onStateReport(CSRunning, tr("Connecting to NRA at %1:%2").arg(m_nraAddress.toString()).arg(m_nraPort));
}

void NRAConnector::stop()
{
	m_rtlServer.close();
	m_nraConnection.abort();
	m_nraStream.abort();
	m_streamRateTimer.stop();
	m_nraState = NRAIdle;
	emit onStateReport(CSIdle, QString::null);
}

void NRAConnector::setFCenter(quint32 fcent)
{
	m_newFCent = fcent;
	m_newFCentPending = true;
	sendNextCommand();
}

void NRAConnector::setReferenceLevel(float rl)
{
	m_newReferenceLevel = rl;
	m_newReferenceLevelPending = true;
	sendNextCommand();
}

void NRAConnector::setAttenuation(float att)
{
	m_newAttenuation = att;
	m_newAttenuationPending = true;
	sendNextCommand();
}

void NRAConnector::setDigitalAttenuation(float att)
{
	m_digitalAttenuation = (int)(att / 6.0);
}

const char* NRAConnector::getErrorString(int errorCode)
{
	switch(errorCode) {
		case 0:
			return "command successful";
		case 201:
			return "command parameter has been corrected";
		case 401:
			return "remote command is not implemented in the remote module";
		case 402:
			return "invalid parameter";
		case 403:
			return "invalid count of parameters";
		case 404:
			return "invalid parameter range";
		case 405:
			return "last command is not completed";
		case 406:
			return "answer time between remote module and application module is too high";
		case 407:
			return "invalid or corrupt data";
		case 408:
			return "error while accessing the hardware";
		case 409:
			return "command is not supported in this version of the application module";
		case 410:
			return "remote is not activated, please send REMOTE ON first";
		case 411:
			return "command is not supported in the selected mode";
		case 412:
			return "memory of data logger is full";
		case 413:
			return "option code is invalid";
		case 414:
			return "incompatible version";
		case 415:
			return "subindex full";
		case 416:
			return "file counter full";
		case 417:
			return "data lost";
		case 418:
			return "checksum error";
		case 419:
			return "FPGA programming error";
		case 420:
			return "path not found";
		case 421:
			return "break detected";
		case 422:
			return "low battery";
		case 423:
			return "file open error";
		case 424:
			return "data verify error";
		case 425:
			return "error while writing voice comment";
		case 426:
			return "no data available";
		case 427:
			return "program parameter error";
		case 428:
			return "frequencies of equipment do not match";
		case 429:
			return "connected equipment and device frequencies do not match";
		case 430:
			return "self test error was detected";
		case 431:
			return "datalogger reorganization required";
		case 432:
			return "mode not available";

		default:
			return "unknown error code";
	}
}

int NRAConnector::readNRAReturnCode()
{
	if(!m_nraConnection.canReadLine())
		return -1;
	QByteArray line(m_nraConnection.readLine(32).trimmed());
	if(!line.endsWith(';'))
		return -1;
	line.truncate(line.count() - 1);
	bool ok;
	int returnCode = line.toInt(&ok);
	if(!ok)
		return -1;
	return returnCode;
}

int NRAConnector::readNRADevInfo()
{
	if(!m_nraConnection.canReadLine())
		return -1;
	QByteArray line(m_nraConnection.readLine(1024).trimmed());
	if(!line.endsWith(';'))
		return -1;
	line.truncate(line.count() - 1);
	QList<QByteArray> list(line.split(','));
	if(list.count() != 9)
		return -1;

	if(!list[0].startsWith('\"'))
		return -1;
	if(!list[0].endsWith('\"'))
		return -1;
	if(!list[2].startsWith('\"'))
		return -1;
	if(!list[2].endsWith('\"'))
		return -1;

	QByteArray product = list[0].mid(1, list[0].length() - 2);
	QByteArray serial = list[2].mid(1, list[2].length() - 2);

	emit onDeviceInfo(product, serial);

	bool ok;
	int returnCode = list[8].toInt(&ok);
	if(!ok)
		return -1;
	return returnCode;
}

int NRAConnector::readNRARBWList(bool* more)
{
	if(!m_nraConnection.canReadLine())
		return -1;
	if((m_lineCount == 0) && (m_lineNo == 0)) {
		// read number of entries
		QByteArray line(m_nraConnection.readLine(64).trimmed());
		if(!line.endsWith(','))
			return -1;
		line.truncate(line.count() - 1);
		bool ok;
		int i = line.toInt(&ok);
		if(!ok)
			return -1;
		m_lineCount = i + 1;
		m_lineNo = 0;
		*more = true;
		return 0;
	} else {
		// entries
		if((m_lineNo + 1) < m_lineCount) {
			QByteArray line(m_nraConnection.readLine(64).trimmed());
			if(!line.endsWith(','))
				return -1;
			line.truncate(line.count() - 1);
			QList<QByteArray> list(line.split(','));
			if(list.count() != 2)
				return -1;
			if(!list[0].startsWith('\"'))
				return -1;
			if(!list[0].endsWith('\"'))
				return -1;
			bool ok;
			float value = list[1].toFloat(&ok);
			if(!ok)
				return -1;
			m_rbwList.append(RBWEntry(list[0].mid(1, list[0].length() - 2), value));
			++m_lineNo;
			*more = true;
			return 0;
		} else {
			// return code
			int returnCode = readNRAReturnCode();
			if(returnCode == 0)
				emit onRBWList(m_rbwList);
			*more = false;
			return returnCode;
		}
	}
}

int NRAConnector::readNRARLList(bool* more)
{
	if(!m_nraConnection.canReadLine())
		return -1;
	if((m_lineCount == 0) && (m_lineNo == 0)) {
		// read number of entries
		QByteArray line(m_nraConnection.readLine(64).trimmed());
		if(!line.endsWith(','))
			return -1;
		line.truncate(line.count() - 1);
		bool ok;
		int i = line.toInt(&ok);
		if(!ok)
			return -1;
		m_lineCount = i + 1;
		m_lineNo = 0;
		*more = true;
		return 0;
	} else {
		// entries
		if((m_lineNo + 1) < m_lineCount) {
			QByteArray line(m_nraConnection.readLine(64).trimmed());
			if(!line.endsWith(','))
				return -1;
			line.truncate(line.count() - 1);
			QList<QByteArray> list(line.split(','));
			if(list.count() != 2)
				return -1;
			if(!list[0].startsWith('\"'))
				return -1;
			if(!list[0].endsWith('\"'))
				return -1;
			bool ok;
			float value = list[1].toFloat(&ok);
			if(!ok)
				return -1;
			m_rlList.append(ReferenceLevelEntry(list[0].mid(1, list[0].length() - 2), value));
			++m_lineNo;
			*more = true;
			return 0;
		} else {
			// return code
			int returnCode = readNRAReturnCode();
			if(returnCode == 0) {
				if(m_rlList.count() > 0) {
					m_newReferenceLevel = m_rlList[0].valueV;
					m_newReferenceLevelPending = true;
				}
				emit onReferenceLevelList(m_rlList);
			}
			*more = false;
			return returnCode;
		}
	}
}

void NRAConnector::handleNRAConnectionState(QAbstractSocket::SocketState socketState)
{
	if(socketState == QAbstractSocket::ConnectedState) {
		if(m_nraState == NRAConnecting) {
			emit onStateReport(CSRunning, tr("Connection to NRA established. Sending initial commands..."));
			m_newFCentPending = false;
			m_newReferenceLevelPending = false;
			m_newAttenuationPending = false;

			m_nraConnection.write("REMOTE_NEWLINE LF;\n");
			m_nraState = NRACmdRemoteNewline;
		}
	} else if(socketState == QAbstractSocket::UnconnectedState) {
	} else {
	}
}

void NRAConnector::handleNRAConnectionError(QAbstractSocket::SocketError)
{
	if(m_nraState != NRAIdle) {
		emit onStateReport(CSError, tr("NRA connection error: %1"). arg(m_nraConnection.errorString()));
		m_rtlServer.close();
		m_nraConnection.abort();
		m_nraStream.abort();
		m_streamRateTimer.stop();
		m_nraState = NRAIdle;
	}
}

void NRAConnector::handleNRAError(const QString& text)
{
	emit onStateReport(CSError, text);
	m_rtlServer.close();
	m_nraConnection.abort();
	m_nraStream.abort();
	m_streamRateTimer.stop();
	m_nraState = NRAIdle;
}

bool NRAConnector::handleStreamHeader()
{
	if(m_streamHeader.byteOrder != 0x55aa) {
		handleNRAError(tr("Unsupported stream byte order %1 detected").arg(m_streamHeader.byteOrder, 4, 16, QChar('0')));
		return false;
	}
	if(m_streamHeader.headerVersion != 1) {
		handleNRAError(tr("Unknown stream header version %d detected").arg(m_streamHeader.headerVersion));
		return false;
	}
	if(m_streamHeader.streamID != 1) {
		// skip non-IQ-data packet
		m_streamExpect = m_streamHeader.sizeOfContext + m_streamHeader.numberOfItems * m_streamHeader.sizeOfItem;
		m_streamState = StrSkip;
		return true;
	}

	// next is context
	if(m_streamHeader.sizeOfContext != sizeof(NRAStreamContext)) {
		handleNRAError(tr("Incompatible stream context size %d detected").arg(m_streamHeader.sizeOfContext));
		return false;
	}

	m_streamExpect = m_streamHeader.sizeOfContext;
	m_streamState = StrContext;

	return true;
}

bool NRAConnector::handleStreamContext()
{
	if(m_streamContext.dataItemFormat != 2) {
		// not int16 - just skip it
		m_streamExpect = m_streamHeader.numberOfItems * m_streamHeader.sizeOfItem;
		m_streamState = StrSkip;
		return true;
	}
	if(m_streamHeader.sizeOfItem != 4) {
		handleNRAError(tr("Incompatible sample size %d detected").arg(m_streamHeader.sizeOfItem));
		return false;
	}

	m_streamExpect = m_streamHeader.numberOfItems * m_streamHeader.sizeOfItem;
	m_streamState = StrSamples;

	if((uint)m_streamContext.sampleRate != m_sampleRate) {
		m_sampleRate = (uint)m_streamContext.sampleRate;
		m_sampleBlockSize = (m_sampleRate / 20) * m_streamHeader.sizeOfItem;
		m_sampleBufferFill = 0;
		if((uint)m_sampleBuffer.size() < m_sampleBlockSize) {
			m_sampleBuffer.resize((int)m_sampleBlockSize);
			if((uint)m_sampleBuffer.size() != m_sampleBlockSize) {
				handleNRAError(tr("Out of memory while allocating sample buffer"));
				return false;
			}
		}
	}

	//qDebug("freq %f, rate %f, samples %d bytes", m_streamContext.fCent, m_streamContext.sampleRate, m_streamExpect);
	return true;
}

bool NRAConnector::handleStreamSamples()
{
	while(m_nraStream.isReadable()) {
		uint block = m_sampleBlockSize - m_sampleBufferFill;
		if(block > m_streamExpect)
			block = m_streamExpect;

		qint64 res = m_nraStream.read(m_sampleBuffer.data() + m_sampleBufferFill, block);
		if(res < 0) {
			handleNRAError(tr("NRA stream read error"));
			return false;
		}
		m_streamRate += res;
		m_sampleBufferFill += res;
		m_streamExpect -= res;

		if(m_sampleBufferFill >= m_sampleBlockSize) {
			relaySamples(m_sampleRate, (const IQSampleS16*)m_sampleBuffer.constData(), m_sampleBlockSize / m_streamHeader.sizeOfItem);
			m_sampleBufferFill = 0;
		}

		if(m_streamExpect == 0) {
			m_streamState = StrHeader;
			m_streamExpect = sizeof(NRAStreamHeader);
			return true;
		}

		if(res != block)
			break;
	}
	return false;
}

void NRAConnector::relaySamples(uint sampleRate, const IQSampleS16* samples, size_t sampleCount)
{
	m_rtlServer.relaySamples(sampleRate, samples, sampleCount, m_digitalAttenuation);
}

void NRAConnector::sendNextCommand()
{
	if(m_nraState != NRARunning)
		return;

	if(m_newFCentPending) {
		char buf[64];
		sprintf(buf, "IQSTREAM_FCENT %u;\n", m_newFCent);
		qDebug("[%s]", buf);
		m_nraConnection.write(buf);
		m_nraState = NRAExecutingCommand;
		m_newFCentPending = false;
	} else if(m_newReferenceLevelPending)  {
		QString cmd(QString::asprintf("IQSTREAM_RL %f;\n", (double)m_newReferenceLevel));
		qDebug("[%s]", qPrintable(cmd));
		m_nraConnection.write(cmd.toLatin1());
		m_nraState = NRAExecutingCommand;
		m_newReferenceLevelPending = false;
	} else if(m_newAttenuationPending) {
		QString cmd(QString::asprintf("IQSTREAM_ATT %f;\n", (double)m_newAttenuation));
		qDebug("[%s]", qPrintable(cmd));
		m_nraConnection.write(cmd.toLatin1());
		m_nraState = NRAExecutingCommand;
		m_newAttenuationPending = false;
	} else {
	}
}

void NRAConnector::handleNRAConnectionReadyRead()
{
	int returnCode;
	bool more;

	while(m_nraConnection.canReadLine()) {
		switch(m_nraState) {
			case NRAIdle:
			case NRAConnecting:
				break;

			case NRACmdRemoteNewline:
				if(m_nraConnection.canReadLine()) {
					returnCode = readNRAReturnCode();
					if(returnCode == 0) {
						m_nraConnection.write("REMOTE ON;\n");
						m_nraState = NRACmdRemoteOn;
					} else {
						handleNRAError(tr("NRA command failed: %1 (%2)").arg(getErrorString(returnCode)).arg(returnCode));
						return;
					}
				}
				break;

			case NRACmdRemoteOn:
				if(m_nraConnection.canReadLine()) {
					returnCode = readNRAReturnCode();
					if(returnCode == 0) {
						m_nraConnection.write("DEV_INFO?;\n");
						m_nraState = NRACmdDevInfo;
					} else {
						handleNRAError(tr("NRA command failed: %1 (%2)").arg(getErrorString(returnCode)).arg(returnCode));
						return;
					}
				}
				break;

			case NRACmdDevInfo:
				if(m_nraConnection.canReadLine()) {
					returnCode = readNRADevInfo();
					if(returnCode == 0) {
						m_nraConnection.write("MODE IQSTREAM;\n");
						m_nraState = NRACmdModeIQStream;
					} else {
						handleNRAError(tr("NRA command failed: %1 (%2)").arg(getErrorString(returnCode)).arg(returnCode));
						return;
					}
				}
				break;

			case NRACmdModeIQStream:
				if(m_nraConnection.canReadLine()) {
					returnCode = readNRAReturnCode();
					if(returnCode == 0) {
						char buf[128];
						sprintf(buf, "STREAM_SETUP TCP,\"\",%d;\n", m_nraStreamPort);
						m_nraConnection.write(buf);
						m_nraState = NRACmdStreamSetup;
					} else {
						handleNRAError(tr("NRA command failed: %1 (%2)").arg(getErrorString(returnCode)).arg(returnCode));
						return;
					}
				}
				break;

			case NRACmdStreamSetup:
				if(m_nraConnection.canReadLine()) {
					returnCode = readNRAReturnCode();
					if(returnCode == 0) {
						m_nraConnection.write("IQSTREAM_RBW_LIST?;\n");
						m_nraState = NRACmdIQStreamRBWList;
						m_rbwList.clear();
						m_lineCount = 0;
						m_lineNo = 0;
					} else {
						handleNRAError(tr("NRA command failed: %1 (%2)").arg(getErrorString(returnCode)).arg(returnCode));
						return;
					}
				}
				break;

			case NRACmdIQStreamRBWList:
				if(m_nraConnection.canReadLine()) {
					returnCode = readNRARBWList(&more);
					if(returnCode != 0) {
						handleNRAError(tr("NRA command failed: %1 (%2)").arg(getErrorString(returnCode)).arg(returnCode));
						return;
					}
					if(!more) {
						m_nraConnection.write("RL_LIST?;\n");
						m_nraState = NRACmdRLList;
						m_rlList.clear();
						m_lineCount = 0;
						m_lineNo = 0;
					}
				}
				break;

			case NRACmdRLList:
				if(m_nraConnection.canReadLine()) {
					returnCode = readNRARLList(&more);
					if(returnCode != 0) {
						handleNRAError(tr("NRA command failed: %1 (%2)").arg(getErrorString(returnCode)).arg(returnCode));
						return;
					}
					if(!more) {
						m_nraConnection.write("IQSTREAM_RBW 400000;\n");
						m_nraState = NRACmdIQStreamRBW;
					}
				}
				break;

			case NRACmdIQStreamRBW:
				if(m_nraConnection.canReadLine()) {
					returnCode = readNRAReturnCode();
					if(returnCode == 0) {
						m_nraConnection.write("STREAM_STATE ON;\n");
						m_nraState = NRACmdStreamState;
					} else {
						handleNRAError(tr("NRA command failed: %1 (%2)").arg(getErrorString(returnCode)).arg(returnCode));
						return;
					}
				}
				break;

			case NRACmdStreamState:
				if(m_nraConnection.canReadLine()) {
					returnCode = readNRAReturnCode();
					if(returnCode == 0) {
						emit onStateReport(CSRunning, tr("Starting NRA stream..."));
						m_nraStream.connectToHost(m_nraAddress, m_nraStreamPort, QTcpSocket::ReadWrite);
						m_nraState = NRAStream;
					} else {
						handleNRAError(tr("NRA command failed: %1 (%2)").arg(getErrorString(returnCode)).arg(returnCode));
						return;
					}
				}
				break;

			case NRAStream:
				break;

			case NRARunning:
				break;

			case NRAExecutingCommand:
				if(m_nraConnection.canReadLine()) {
					returnCode = readNRAReturnCode();
					if(returnCode == 0) {
						m_nraState = NRARunning;
						sendNextCommand();
					} else {
						handleNRAError(tr("NRA command failed: %1 (%2)").arg(getErrorString(returnCode)).arg(returnCode));
						return;
					}
				}
				break;
		}
	}
}

void NRAConnector::handleNRAStreamState(QAbstractSocket::SocketState socketState)
{
	if(socketState == QAbstractSocket::ConnectedState) {
		if(m_nraState == NRAStream) {
			emit onStateReport(CSRunning, tr("NRA streaming active."));
			m_nraState = NRARunning;
			m_streamRate = 0;
			m_streamRateTimer.start(1000);
			m_streamState = StrHeader;
			m_streamExpect = sizeof(NRAStreamHeader);
			m_sampleRate = (uint)-1;
			m_sampleBlockSize = 0;
			m_sampleBufferFill = 0;
			m_newFCent = 100000000.0;
			m_newFCentPending = true;
			sendNextCommand();
		}
	} else if(socketState == QAbstractSocket::UnconnectedState) {
	} else {
	}
}

void NRAConnector::handleNRAStreamError(QAbstractSocket::SocketError socketError)
{
	if(m_nraState != NRAIdle) {
		emit onStateReport(CSError, tr("NRA stream error: (%1) %2").arg(socketError).arg(m_nraConnection.errorString()));
		m_rtlServer.close();
		m_nraConnection.abort();
		m_nraStream.abort();
		m_streamRateTimer.stop();
		m_nraState = NRAIdle;
	}
}

void NRAConnector::handleNRAStreamReadyRead()
{
	while(m_nraStream.isReadable()) {
		switch(m_streamState) {
			case StrHeader:
				if(m_nraStream.bytesAvailable() < m_streamExpect)
					return;
				if(m_nraStream.read((char*)&m_streamHeader, sizeof(NRAStreamHeader)) == sizeof(NRAStreamHeader)) {
					m_streamRate += sizeof(NRAStreamHeader);
					if(!handleStreamHeader())
						return;
				} else {
					handleNRAError(tr("NRA stream read error"));
					return;
				}
				break;

			case StrSkip: {
				char buffer[1024];
				uint block = m_streamExpect;
				if(block > sizeof(buffer))
					block = sizeof(buffer);
				qint64 res = m_nraStream.read(buffer, block);
				if(res < 0) {
					handleNRAError(tr("NRA stream read error"));
					return;
				}
				m_streamRate += res;
				m_streamExpect -= res;
				if(m_streamExpect == 0) {
					m_streamState = StrHeader;
					m_streamExpect = sizeof(NRAStreamHeader);
				}
				if(res == block)
					continue;
				else return;
			}

			case StrContext:
				if(m_nraStream.bytesAvailable() < m_streamExpect)
					return;
				if(m_nraStream.read((char*)&m_streamContext, sizeof(NRAStreamContext)) == sizeof(NRAStreamContext)) {
					m_streamRate += sizeof(NRAStreamContext);
					if(!handleStreamContext())
						return;
				} else {
					handleNRAError(tr("NRA stream read error"));
					return;
				}
				break;

			case StrSamples:
				if(!handleStreamSamples())
					return;
				break;
		}
	}
}

void NRAConnector::handleStreamRateTimer()
{
	emit onStreamRate(m_streamRate);
	m_streamRate = 0;
}
