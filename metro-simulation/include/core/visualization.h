#pragma once

#include "simulation.h"
#include "metro_graph.h"

#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

class QCPColorMap;
class QCPColorScale;
class QCPGraph;
class QCustomPlot;
class QMouseEvent;

class VisualizationWidget : public QWidget {
	Q_OBJECT
public:
	explicit VisualizationWidget(QWidget *parent = nullptr);

	void setGraph(const MetroGraph &graph);
	void setSimulation(const Simulation &simulation);
	void setHighlightedPath(const std::vector<std::string> &path);
	void setComparedPaths(const std::vector<std::pair<std::vector<std::string>, QColor>> &paths);
	void clearComparedPaths();
	void clearHistory();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

signals:
	void legendRequested();

private:
	void buildUi();
	void rebuildTopologyPlot(const Simulation *simulation = nullptr);
	void rebuildHeatmapPlot(const Simulation *simulation = nullptr);
	void rebuildNodeList();
	void refreshNodeList();
	void refreshStatisticsPlot();
	void refreshSummary(const Simulation *simulation = nullptr);
	void appendEvents(const Simulation &simulation);
	void updatePlotsFromSimulation(const Simulation &simulation);
	void handleTopologyMouseMove(QMouseEvent *event);
	void toggleMaximizeCard(QFrame *card, QPushButton *btn);
	void refreshDataAnalytics(const Simulation &simulation);
	void refreshPassengerPanel(const Simulation &simulation);

	QFrame *createCardFrame();
	QColor densityColor(double density) const;
	double nodeDensity(const std::string &nodeId, const Simulation &simulation) const;
	int nodePassengerCount(const std::string &nodeId, const Simulation &simulation) const;

	struct RenderedNodeInfo {
		std::string id;
		std::string name;
		std::string type;
		double x = 0.0;
		double y = 0.0;
		double capacity = 0.0;
		int currentCount = 0;
		double density = 0.0;
		int floor = 0;
	};

	QLabel *stationLabel_ = nullptr;
	QLabel *timeLabel_ = nullptr;
	QLabel *activeLabel_ = nullptr;
	QLabel *completedLabel_ = nullptr;
	QLabel *timeoutLabel_ = nullptr;
	QLabel *congestionLabel_ = nullptr;
	QLabel *queueLabel_ = nullptr;
	QLabel *avgTravelLabel_ = nullptr;

	QCustomPlot *topologyPlot_ = nullptr;
	QCustomPlot *heatmapPlot_ = nullptr;
	QCustomPlot *statisticsPlot_ = nullptr;
	QCPColorMap *heatmapColorMap_ = nullptr;
	QCPColorScale *heatmapColorScale_ = nullptr;
	QCPGraph *activeGraph_ = nullptr;
	QCPGraph *completedGraph_ = nullptr;
	QCPGraph *timeoutGraph_ = nullptr;
	QStackedWidget *eventStack_ = nullptr;
	QWidget *eventEmptyPage_ = nullptr;
	QPlainTextEdit *eventLog_ = nullptr;
	QPlainTextEdit *headerEventFeed_ = nullptr;

	QWidget *nodeListPanel_ = nullptr;
	QVBoxLayout *nodeListLayout_ = nullptr;
	QVector<QLabel *> nodeListInfoLabels_;
	QVector<QWidget *> nodeDetailPanels_;
	QVector<QLabel *> nodeDetailLabels_;

	QComboBox *floorFilterCombo_ = nullptr;
	QPushButton *heatmapOverlayBtn_ = nullptr;

	// Data analytics panel
	QFrame *analyticsCard_ = nullptr;
	QTableWidget *analyticsTable_ = nullptr;
	QLabel *analyticsThroughput_ = nullptr;
	QLabel *analyticsAvgTravel_ = nullptr;
	QLabel *analyticsMaxQueue_ = nullptr;
	QLabel *analyticsCongestionNode_ = nullptr;

	// Passenger detail panel
	QFrame *passengerCard_ = nullptr;
	QTableWidget *passengerTable_ = nullptr;

	QFrame *topologyCard_ = nullptr;
	QFrame *heatmapCard_ = nullptr;
	QFrame *statsCard_ = nullptr;
	QFrame *logCard_ = nullptr;
	QSplitter *mainSplitter_ = nullptr;
	QFrame *maximizedCard_ = nullptr;
	QWidget *leftPanel_ = nullptr;
	QWidget *rightPanel_ = nullptr;
	QList<int> savedSplitterSizes_;

	QVector<double> timeHistory_;
	QVector<double> activeHistory_;
	QVector<double> completedHistory_;
	QVector<double> timeoutHistory_;
	QVector<RenderedNodeInfo> renderedNodes_;
	double topologyNodeRadius_ = 0.6;

	MetroGraph graph_;
	std::vector<std::string> highlightedPath_;
	std::vector<std::pair<std::vector<std::string>, QColor>> comparedPaths_;
	std::size_t lastEventCount_ = 0;
	double lastTime_ = -1.0;
	bool hasGraph_ = false;
	int lastCompletedForThroughput_ = 0;
	int lastTimeForThroughput_ = 0;
	double currentThroughput_ = 0.0;
};
