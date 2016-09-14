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

#ifndef INCLUDE_MAINWINDOW_H
#define INCLUDE_MAINWINDOW_H

#include <QWidget>
#include "nraconnector.h"

namespace Ui {
	class MainWindow;
}

class MainWindow : public QWidget {
	Q_OBJECT

public:
	explicit MainWindow(QWidget* parent = nullptr);
	~MainWindow();

protected slots:
	void on_startButton_toggled(bool checked);
	void on_nraRefLvl_currentIndexChanged(int index);
	void on_digiAtt_currentIndexChanged(int index);

	void handleNRAStateReport(NRAConnector::ConnectorState state, const QString& text);
	void handleNRADeviceInfo(const QString& productName, const QString& serial);
	void handleNRAStreamRate(int rate);
	void handleNRAReferenceLevelList(const NRAConnector::ReferenceLevelList& rlList);

protected:
	Ui::MainWindow* ui;
	NRAConnector* m_nraConnector;

	void resetGUI();
	void loadSettings();
	void saveSettings();
};

#endif // INCLUDE_MAINWINDOW_H
