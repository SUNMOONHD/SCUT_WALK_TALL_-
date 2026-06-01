#pragma once

#include "simulation.h"

#include <QMainWindow>

class QAction;
class QLabel;
class QTimer;
class QCheckBox;
class QDialog;
class QToolBar;
class QToolButton;
class WaitingSpinnerWidget;
class VisualizationWidget;
class Station3DView;
class QComboBox;

class MainWindow : public QMainWindow {
public:
	explicit MainWindow(QWidget *parent = nullptr);

private:
	void buildUi();
	void setupToolbar();
	void setupConnections();
	void loadInitialScenario();
	void stepSimulation();
	void refreshDashboard();
	void toggleSimulation();
	void resetSimulation();
	void exportResults();
	void showDiagramImage();
	void showImageDialog(const QString &title, const QStringList &relativeCandidates);
	void openTopologyEditor();
    void open3DView();
    void showLegend();
	void openPathCompare();
	void updateToolbarState();
	void applyPathPreference(int index);
	void applyAlgorithmSettings();
	QString resourcePath(const QString &relativePath) const;
	QString dataPath(const QString &relativePath) const;
	QIcon iconForResource(const QString &fileName) const;

	Simulation simulation_;
	VisualizationWidget *dashboard_ = nullptr;
	QTimer *timer_ = nullptr;
	WaitingSpinnerWidget *spinner_ = nullptr;
	QAction *startAction_ = nullptr;
	QAction *pauseAction_ = nullptr;
	QAction *resetAction_ = nullptr;
	QAction *exportAction_ = nullptr;
	QAction *diagramAction_ = nullptr;
	QAction *editAction_ = nullptr;
    QAction *view3dAction_ = nullptr;
    QAction *legendAction_ = nullptr;
	QAction *pathCompareAction_ = nullptr;
	QComboBox *speedCombo_ = nullptr;
	QComboBox *pathPreferenceCombo_ = nullptr;
	QCheckBox *useAStarCheck_ = nullptr;
	QCheckBox *useCacheCheck_ = nullptr;
	QLabel *statusLabel_ = nullptr;
	bool running_ = false;
	QDialog *view3dDialog_ = nullptr;
	Station3DView *view3dWidget_ = nullptr;
	QToolBar *mainToolbar_ = nullptr;
};
