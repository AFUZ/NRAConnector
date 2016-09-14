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

#include <QTcpSocket>
#include <QtEndian>
#include "rtlserver.h"

RTLServer::RTLServer(QObject* parent) :
	QObject(parent)
{
	connect(&m_rtlServer, &QTcpServer::newConnection, this, &RTLServer::handleRTLServerNewConnection);
	m_nraSampleRate = -1;
	m_rtlSampleRate = 2000000.0;
}

bool RTLServer::open(const QHostAddress& rtlListenAddress, quint16 rtlListenPort)
{
	m_rtlListenAddress = rtlListenAddress;
	m_rtlListenPort = rtlListenPort;

	if(!m_rtlServer.listen(m_rtlListenAddress, m_rtlListenPort)) {
		m_errorString = m_rtlServer.errorString();
		return false;
	}

	return true;
}

void RTLServer::close()
{
	if(m_rtlSocket != nullptr) {
		m_rtlSocket->abort();
		m_rtlSocket->deleteLater();
		m_rtlSocket = nullptr;
	}
	m_rtlServer.close();
}

void RTLServer::relaySamples(uint sampleRate, const IQSampleS16* samples, size_t sampleCount, int shift)
{
	if(m_rtlSocket == nullptr)
		return;

	if((Real)sampleRate != m_nraSampleRate) {
		m_nraSampleRate = (Real)sampleRate;
		/*
		Real cutoff = m_nraSampleRate * 0.9;
		if((m_rtlSampleRate * 0.9) < cutoff)
			cutoff = m_rtlSampleRate * 0.9;
		m_interpolator.create(16, m_nraSampleRate, cutoff);
		*/
		m_interpolator.create((double)m_nraSampleRate, (double)m_rtlSampleRate);
		if(m_rtlSampleRate > 0.0)
			m_interpolatorDistance = m_nraSampleRate / m_rtlSampleRate;
		else m_interpolatorDistance = 1.0;
		m_interpolatorDistanceRemain = m_interpolatorDistance;
		qDebug("interpoaltor distance %f", (double)m_interpolatorDistance);
	}

	Complex outputSample;
	while(sampleCount > 0) {
		Complex inputSample(samples->i >> shift, samples->q >> shift);
		for(bool consumed = false; !consumed; ) {
			if(m_interpolator.interpolate(&m_interpolatorDistanceRemain, inputSample, &consumed, &outputSample)) {
				m_buffer[m_bufferFill++] = (quint8)((qint8)(outputSample.real()) + 128);
				m_buffer[m_bufferFill++] = (quint8)((qint8)(outputSample.imag()) + 128);

				if(m_bufferFill >= ((int)sizeof(m_buffer) - 4)) {
					m_rtlSocket->write((const char*)m_buffer, m_bufferFill);
					m_bufferFill = 0;
				}
				m_interpolatorDistanceRemain += m_interpolatorDistance;
			}
		}
		++samples;
		--sampleCount;
	}
#if 0

	Real sampleDistance = (Real)sampleRate / m_rtlSampleRate;

	while(sampleCount > 0) {
		Real nextI = ((Real)samples->i);
		Real nextQ = ((Real)samples->q);
		bool consumed = false;

		while(!consumed) {
			Real resultI;
			Real resultQ;
			if(m_interpolator.next(&m_sampleDistance, nextI, nextQ, &consumed, &resultI, &resultQ)) {
				m_buffer[m_bufferFill++] = (quint8)(resultI / 1.0) + 128;
				m_buffer[m_bufferFill++] = (quint8)(resultQ / 1.0) + 128;

				if(m_bufferFill >= ((int)sizeof(m_buffer) - 2)) {
					m_rtlSocket->write((const char*)m_buffer, m_bufferFill);
					m_bufferFill = 0;
				}

				m_sampleDistance += sampleDistance;
			}
		}
		++samples;
		--sampleCount;
	}
#endif
}

void RTLServer::handleRTLServerNewConnection()
{
	if(m_rtlSocket != nullptr) {
		m_rtlSocket->abort();
		m_rtlSocket->deleteLater();
		m_rtlSocket = nullptr;
	}

	m_rtlSocket = m_rtlServer.nextPendingConnection();
	if(m_rtlSocket == nullptr)
		return;

	m_rtlSocket->setParent(this);

	connect(m_rtlSocket, &QTcpSocket::stateChanged, this, &RTLServer::handleRTLConnectionState);
	connect(m_rtlSocket, SIGNAL(error(QAbstractSocket::SocketError)), this, SLOT(handleRTLConnectionError(QAbstractSocket::SocketError)));
	connect(m_rtlSocket, &QTcpSocket::readyRead, this, &RTLServer::handleRTLConnectionReadyRead);

	RTLDongleInfo dongleInfo;
	::memcpy(dongleInfo.magic, "RTL0", 4);
	dongleInfo.tunerType = 0;
	dongleInfo.tunerGainCount = 0;
	m_rtlSocket->write((const char*)&dongleInfo, sizeof(RTLDongleInfo));

	m_nraSampleRate = -1;
	m_bufferFill = 0;
	m_interpolatorDistance = 1.0;
	m_interpolatorDistanceRemain = 0.0;
}

void RTLServer::handleRTLConnectionState(QAbstractSocket::SocketState socketState)
{
	if(socketState == QAbstractSocket::ConnectedState) {
	} else if(socketState == QAbstractSocket::UnconnectedState) {
		if(m_rtlSocket != nullptr) {
			m_rtlSocket->deleteLater();
			m_rtlSocket = nullptr;
		}
	} else {
	}
}

void RTLServer::handleRTLConnectionError(QAbstractSocket::SocketError)
{
	if(m_rtlSocket != nullptr) {
		m_rtlSocket->deleteLater();
		m_rtlSocket = nullptr;
	}
}

void RTLServer::handleRTLConnectionReadyRead()
{
	while(m_rtlSocket->isReadable()) {
		if(m_rtlSocket->bytesAvailable() < (int)sizeof(RTLCommand))
			return;

		RTLCommand cmd;

		if(m_rtlSocket->read((char*)&cmd, sizeof(RTLCommand)) != sizeof(RTLCommand)) {
			m_rtlSocket->abort();
			m_rtlSocket->deleteLater();
			m_rtlSocket = nullptr;
			return;
		}

		QString hex;
		for(uint i = 0; i < sizeof(RTLCommand); ++i)
			hex.append(QString(" %1").arg(((const quint8*)&cmd)[i], 2, 16, QChar('0')));
		//qDebug("RTL RX%s", qPrintable(hex));

		quint32 param = qFromBigEndian(cmd.param);

		switch(cmd.cmd) {
			case 0x01:
				qDebug("RTL: set freq %u", param);
				emit onSetFCenter(param);
				//				setFCenter(param);
				//				rtlsdr_set_center_freq(dev,ntohl(param));
				break;
			case 0x02:
				qDebug("RTL: set sample rate %u", param);
				m_rtlSampleRate = param;
				m_nraSampleRate = -1; // force interpolator re-create
				//rtlsdr_set_sample_rate(dev, ntohl(param));
				break;
			case 0x03:
				qDebug("RTL: set gain mode %u", param);
				//rtlsdr_set_tuner_gain_mode(dev, ntohl(param));
				break;
			case 0x04:
				qDebug("RTL: set gain %u", param);
				//rtlsdr_set_tuner_gain(dev, ntohl(param));
				break;
			case 0x05:
				qDebug("RTL: set freq correction %u", param);
				//rtlsdr_set_freq_correction(dev, ntohl(param));
				break;
			case 0x06:
				qDebug("RTL: set if stage %d gain %d", param >> 16, (short)(param & 0xffff));
				//rtlsdr_set_tuner_if_gain(dev, tmp >> 16, (short)(tmp & 0xffff));
				break;
			case 0x07:
				qDebug("RTL: set test mode %u", param);
				//rtlsdr_set_testmode(dev, ntohl(param));
				break;
			case 0x08:
				qDebug("RTL: set agc mode %u", param);
				//rtlsdr_set_agc_mode(dev, ntohl(param));
				break;
			case 0x09:
				qDebug("RTL: set direct sampling %u", param);
				//rtlsdr_set_direct_sampling(dev, ntohl(param));
				break;
			case 0x0a:
				qDebug("RTL: set offset tuning %u", param);
				//rtlsdr_set_offset_tuning(dev, ntohl(param));
				break;
			case 0x0b:
				qDebug("RTL: set rtl xtal %u", param);
				//rtlsdr_set_xtal_freq(dev, ntohl(param), 0);
				break;
			case 0x0c:
				qDebug("RTL: set tuner xtal %u", param);
				//rtlsdr_set_xtal_freq(dev, 0, ntohl(param));
				break;
			case 0x0d:
				qDebug("RTL: set tuner gain by index %u", param);
				//set_gain_by_index(dev, ntohl(param));
				break;
			default:
				qDebug("RTL: received unknown command %02x", cmd.cmd);
				break;
		}
	}
}
