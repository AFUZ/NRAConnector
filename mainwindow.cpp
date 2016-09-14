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
 * File contents: Application main window
 */

#include <QMessageBox>
#include <QSettings>
#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget* parent) :
	QWidget(parent),
	ui(new Ui::MainWindow),
	m_nraConnector(new NRAConnector(this))
{
	ui->setupUi(this);

	loadSettings();

	connect(m_nraConnector, &NRAConnector::onStateReport, this, &MainWindow::handleNRAStateReport);
	connect(m_nraConnector, &NRAConnector::onDeviceInfo, this, &MainWindow::handleNRADeviceInfo);
	connect(m_nraConnector, &NRAConnector::onStreamRate, this, &MainWindow::handleNRAStreamRate);
	connect(m_nraConnector, &NRAConnector::onReferenceLevelList, this, &MainWindow::handleNRAReferenceLevelList);
	ui->status->setText(tr("Idle"));

	bool blocked = ui->digiAtt->blockSignals(true);
	ui->digiAtt->addItem(tr("0 dB"));
	ui->digiAtt->addItem(tr("6 dB"));
	ui->digiAtt->addItem(tr("12 dB"));
	ui->digiAtt->addItem(tr("18 dB"));
	ui->digiAtt->addItem(tr("24 dB"));
	ui->digiAtt->addItem(tr("30 dB"));
	ui->digiAtt->addItem(tr("36 dB"));
	ui->digiAtt->addItem(tr("42 dB"));
	ui->digiAtt->addItem(tr("48 dB"));
	ui->digiAtt->blockSignals(blocked);

	resetGUI();
}

MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::on_startButton_toggled(bool checked)
{
	if(checked) {
		// start
		QHostAddress nraAddress;
		int nraPort;
		QHostAddress rtlTcpAddress;
		int rtlTcpPort;

		bool ok;

		if(!nraAddress.setAddress(ui->nraIP->text())) {
			QMessageBox::critical(this, tr("Configuration Error"), tr("NRA IP address \"%1\" is invalid.").arg(ui->nraIP->text()));
			ui->startButton->setChecked(false);
			return;
		}
		ui->nraIP->setText(nraAddress.toString());

		nraPort = ui->nraPort->text().toInt(&ok);
		if(!ok) {
			QMessageBox::critical(this, tr("Configuration Error"), tr("NRA port number \"%1\" is invalid.").arg(ui->nraIP->text()));
			ui->startButton->setChecked(false);
			return;
		}
		if((nraPort < 1) || (nraPort > 65535)) {
			QMessageBox::critical(this, tr("Configuration Error"), tr("NRA port number \"%1\" is out of range.").arg(nraPort));
			ui->startButton->setChecked(false);
			return;
		}
		ui->nraPort->setText(tr("%1").arg(nraPort));

		if(!rtlTcpAddress.setAddress(ui->rtlListenIP->text())) {
			QMessageBox::critical(this, tr("Configuration Error"), tr("RTL-TCP listen IP \"%1\" is invalid.").arg(ui->rtlListenIP->text()));
			ui->startButton->setChecked(false);
			return;
		}
		ui->rtlListenIP->setText(rtlTcpAddress.toString());

		rtlTcpPort = ui->rtlListenPort->text().toInt(&ok);
		if(!ok) {
			QMessageBox::critical(this, tr("Configuration Error"), tr("RTL-TCP listen port \"%1\" is invalid.").arg(ui->rtlListenPort->text()));
			ui->startButton->setChecked(false);
			return;
		}
		if((rtlTcpPort < 1) || (rtlTcpPort > 65535)) {
			QMessageBox::critical(this, tr("Configuration Error"), tr("RTL-TCP listen port number \"%1\" is out of range.").arg(rtlTcpPort));
			ui->startButton->setChecked(false);
			return;
		}
		ui->rtlListenPort->setText(tr("%1").arg(rtlTcpPort));

		saveSettings();
		m_nraConnector->start(nraAddress, nraPort, rtlTcpAddress, rtlTcpPort);

		ui->nraIP->setEnabled(false);
		ui->nraPort->setEnabled(false);
		ui->rtlListenIP->setEnabled(false);
		ui->rtlListenPort->setEnabled(false);
		ui->startButton->setText(tr("Stop"));
	} else {
		// stop
		m_nraConnector->stop();
	}
}

void MainWindow::on_nraRefLvl_currentIndexChanged(int index)
{
	m_nraConnector->setReferenceLevel(ui->nraRefLvl->itemData(index).toFloat());
}

void MainWindow::on_digiAtt_currentIndexChanged(int index)
{
	m_nraConnector->setDigitalAttenuation(index * 6);
}

void MainWindow::handleNRAStateReport(NRAConnector::ConnectorState state, const QString& text)
{
	bool blocked;

	switch(state) {
		case NRAConnector::CSIdle:
			ui->status->setText(tr("Idle"));
			blocked = ui->startButton->blockSignals(true);
			ui->startButton->setChecked(false);
			ui->startButton->blockSignals(blocked);
			resetGUI();
			break;

		case NRAConnector::CSError:
			ui->status->setText(tr("Error: %1").arg(text));
			QMessageBox::critical(this, tr("NRA Connector Error"), tr("Error: %1").arg(text));
			blocked = ui->startButton->blockSignals(true);
			ui->startButton->setChecked(false);
			ui->startButton->blockSignals(blocked);
			resetGUI();
			break;

		case NRAConnector::CSRunning:
			if(text.isEmpty())
				ui->status->setText(tr("Running"));
			ui->status->setText(tr("Running: %1").arg(text));
			ui->digiAtt->setEnabled(true);
			break;
	}
}

void MainWindow::handleNRADeviceInfo(const QString& productName, const QString& serial)
{
	ui->nraDevInfo->setText(tr("Model %1, S/N %2").arg(productName).arg(serial));
}

void MainWindow::handleNRAStreamRate(int rate)
{
	ui->nraStreamBitrate->setText(tr("%1 kBit/s").arg(rate * 8 / 1024));
}

void MainWindow::handleNRAReferenceLevelList(const NRAConnector::ReferenceLevelList& rlList)
{
	bool blocked = ui->nraRefLvl->blockSignals(true);
	for(int i = 0; i < rlList.count(); ++i)
		ui->nraRefLvl->addItem(rlList[i].displayString, rlList[i].valueV);
	ui->nraRefLvl->setEnabled(true);
	ui->nraRefLvl->blockSignals(blocked);
	ui->nraRefLvl->setCurrentIndex(0);
	if(rlList.count() > 0)
		m_nraConnector->setReferenceLevel(ui->nraRefLvl->itemData(0).toFloat());
}

void MainWindow::resetGUI()
{
	ui->nraIP->setEnabled(true);
	ui->nraPort->setEnabled(true);
	ui->rtlListenIP->setEnabled(true);
	ui->rtlListenPort->setEnabled(true);
	ui->nraDevInfo->clear();
	ui->nraStreamBitrate->clear();
	ui->nraRefLvl->clear();
	ui->nraRefLvl->setEnabled(false);
	ui->digiAtt->setEnabled(false);
	ui->startButton->setText(tr("Start!"));
}

void MainWindow::loadSettings()
{
	QSettings settings(QSettings::UserScope, "afuz.org", "NRAConnector");

	ui->nraIP->setText(settings.value("nraip", "192.168.128.128").toString());
	ui->nraPort->setText(settings.value("nraport", "55555").toString());
	ui->rtlListenIP->setText(settings.value("rtllistenip", "0.0.0.0").toString());
	ui->rtlListenPort->setText(settings.value("rtllistenport", "1234").toString());
}

void MainWindow::saveSettings()
{
	QSettings settings(QSettings::UserScope, "afuz.org", "NRAConnector");

	settings.setValue("nraip", ui->nraIP->text());
	settings.setValue("nraport", ui->nraPort->text());
	settings.setValue("rtllistenip", ui->rtlListenIP->text());
	settings.setValue("rtllistenport", ui->rtlListenPort->text());
}
