#include "visualization.h"

#include "asset_catalog.h"
#include "event.h"

#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFont>
#include <QCoreApplication>
#include <QFileInfo>
#include <QGridLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMap>
#include <QTextCursor>
#include <QPlainTextEdit>
#include <QPointF>
#include <QPolygonF>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextOption>
#include <QMouseEvent>
#include <QPainter>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>
#include <QSvgRenderer>

#include "qcustomplot.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <sstream>

namespace {

QString formatSimTime(int totalSeconds)
{
	int h = totalSeconds / 3600;
	int m = (totalSeconds % 3600) / 60;
	int s = totalSeconds % 60;
	return QStringLiteral("%1:%2:%3")
		.arg(h, 2, 10, QLatin1Char('0'))
		.arg(m, 2, 10, QLatin1Char('0'))
		.arg(s, 2, 10, QLatin1Char('0'));
}

QString eventTypeName(EventType type)
{
	switch (type) {
	case EventType::PassengerArrived:    return QStringLiteral("到达");
	case EventType::PassengerExited:     return QStringLiteral("离开");
	case EventType::CongestionTriggered: return QStringLiteral("拥堵");
	case EventType::TimeoutReached:      return QStringLiteral("超时");
	case EventType::PeakHourStarted:     return QStringLiteral("高峰");
	case EventType::PeakHourEnded:       return QStringLiteral("平峰");
	case EventType::TrainArrived:        return QStringLiteral("列车");
	case EventType::PassengerSurge:      return QStringLiteral("爆发");
	default:                             return QStringLiteral("未知");
	}
}

double clamp01(double value)
{
	if (value < 0.0) {
		return 0.0;
	}
	if (value > 1.0) {
		return 1.0;
	}
	return value;
}

QColor lerpColor(const QColor &from, const QColor &to, double t)
{
	t = clamp01(t);
	const int red = static_cast<int>(from.red() + (to.red() - from.red()) * t);
	const int green = static_cast<int>(from.green() + (to.green() - from.green()) * t);
	const int blue = static_cast<int>(from.blue() + (to.blue() - from.blue()) * t);
	return QColor(red, green, blue);
}

QColor densityToColor(double density)
{
	const double clamped = clamp01(density / 0.85);
	if (clamped < 0.5) {
		return lerpColor(QColor("#4A9C8C"), QColor("#D4953A"), clamped * 2.0);
	}
	return lerpColor(QColor("#D4953A"), QColor("#C43D3D"), (clamped - 0.5) * 2.0);
}

double gaussian(double dx, double dy, double sigma)
{
	const double exponent = -(dx * dx + dy * dy) / (2.0 * sigma * sigma);
	return std::exp(exponent);
}

QString resourcePath(const QString &relativePath)
{
	const QString appDir = QCoreApplication::applicationDirPath();
	const QStringList candidates = {
		QDir(appDir).absoluteFilePath(QStringLiteral("resources/") + relativePath),
		QDir(appDir).absoluteFilePath(QStringLiteral("../resources/") + relativePath),
		QDir(appDir).absoluteFilePath(QStringLiteral("../../resources/") + relativePath),
		QDir::current().absoluteFilePath(QStringLiteral("resources/") + relativePath),
		QDir::current().absoluteFilePath(relativePath)
	};
	for (const QString &candidate : candidates) {
		if (QFileInfo::exists(candidate)) {
			return candidate;
		}
	}
	return candidates.front();
}

QString cssImageUrl(const QString &relativePath)
{
	const QString fullPath = resourcePath(relativePath);
	if (!QFileInfo::exists(fullPath)) {
		return QString();
	}
	const QString normalized = QDir::fromNativeSeparators(QFileInfo(fullPath).absoluteFilePath());
	return QStringLiteral("\"%1\"").arg(normalized);
}

QString resolveFirstExistingPath(const QStringList &relativePaths)
{
	for (const QString &relative : relativePaths) {
		const QString fullPath = resourcePath(relative);
		if (QFileInfo::exists(fullPath)) {
			return fullPath;
		}
	}
	return QString();
}

QPixmap loadPixmapWithSvgSupport(const QString &filePath, int maxW, int maxH)
{
	if (filePath.isEmpty()) {
		return QPixmap();
	}
	if (filePath.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
		QSvgRenderer renderer(filePath);
		if (!renderer.isValid()) {
			return QPixmap();
		}
		QSize size = renderer.defaultSize();
		if (!size.isValid()) {
			size = QSize(maxW, maxH);
		}
		size.scale(maxW, maxH, Qt::KeepAspectRatio);
		QPixmap pixmap(size);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		renderer.render(&painter);
		return pixmap;
	}
	QPixmap pixmap(filePath);
	if (!pixmap.isNull()) {
		return pixmap.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}
	return QPixmap();
}

QString titleWithIcon(const QString &iconFile, const QString &text)
{
	const QString iconUrl = QUrl::fromLocalFile(resourcePath(QStringLiteral("icons/") + iconFile)).toString();
	return QStringLiteral("<img src=\"%1\" width=\"16\" height=\"16\"/> <span style=\"color:#3D322C;\">%2</span>").arg(iconUrl, text);
}

QString titleWithIconBadge(const QString &iconFile, const QString &text, const QString &badgeColor)
{
	const QString iconUrl = QUrl::fromLocalFile(resourcePath(QStringLiteral("icons/") + iconFile)).toString();
	return QStringLiteral(
		"<span style=\"display:inline-block;width:22px;height:22px;line-height:22px;text-align:center;"
		"border-radius:11px;background:%1;vertical-align:middle;margin-right:6px;\">"
		"<img src=\"%2\" width=\"16\" height=\"16\"/></span>"
		"<span style=\"color:#3D322C;vertical-align:middle;\">%3</span>")
		.arg(badgeColor, iconUrl, text);
}

QColor lineColorForIndex(int index)
{
	static const QColor palette[] = {
		QColor("#4A9C8C"), QColor("#D4953A"), QColor("#C4566E"), QColor("#8B5E7A")
	};
	return palette[index % 4];
}

QString nodeTypeToChinese(const std::string &type)
{
	if (type == "entrance") return QStringLiteral("入口");
	if (type == "exit") return QStringLiteral("出口");
	if (type == "security") return QStringLiteral("安检");
	if (type == "ticket") return QStringLiteral("售票");
	if (type == "gate") return QStringLiteral("闸机");
	if (type == "hall") return QStringLiteral("大厅");
	if (type == "corridor") return QStringLiteral("走廊");
	if (type == "stairs") return QStringLiteral("楼梯");
	if (type == "escalator") return QStringLiteral("扶梯");
	if (type == "platform") return QStringLiteral("站台");
	if (type == "waiting") return QStringLiteral("候车区");
	return QString::fromStdString(type);
}

QString floorToString(int floor)
{
	if (floor < 0) return QStringLiteral("B%1").arg(-floor);
	return QStringLiteral("F%1").arg(floor);
}

} // namespace

VisualizationWidget::VisualizationWidget(QWidget *parent)
	: QWidget(parent)
{
	buildUi();
}

void VisualizationWidget::buildUi()
{
	auto *rootLayout = new QVBoxLayout(this);
	rootLayout->setContentsMargins(0, 0, 0, 0);
	rootLayout->setSpacing(12);


	auto *mainSplitter = new QSplitter(Qt::Horizontal, this);
	mainSplitter_ = mainSplitter;
	mainSplitter->setChildrenCollapsible(false);

	auto *leftWidget = new QWidget(mainSplitter);
	leftPanel_ = leftWidget;
	auto *leftLayout = new QVBoxLayout(leftWidget);
	leftLayout->setContentsMargins(0, 0, 0, 0);
	leftLayout->setSpacing(12);

	topologyCard_ = createCardFrame();
	auto *topologyCard = topologyCard_;
	auto *topologyLayout = new QVBoxLayout(topologyCard);
	topologyLayout->setContentsMargins(12, 12, 12, 12);

	// ====================== 修复：拓扑视图标题（原生方式） ======================
	auto *topologyTitleLayout = new QHBoxLayout();
	topologyTitleLayout->setContentsMargins(0, 0, 0, 0);
	topologyTitleLayout->setSpacing(8);

	auto *topologyIcon = new QLabel(topologyCard);
	QPixmap topologyPixmap = loadPixmapWithSvgSupport(resourcePath(QStringLiteral("icons/node_topology.svg")), 16, 16);
	topologyIcon->setPixmap(topologyPixmap);

	auto *topologyTitle = new QLabel(QStringLiteral("拓扑视图"), topologyCard);
	topologyTitle->setObjectName(QStringLiteral("Headline"));
	topologyTitle->setStyleSheet(QStringLiteral("font-size: 16px; color:#3D322C;"));

	topologyTitleLayout->addWidget(topologyIcon);
	topologyTitleLayout->addWidget(topologyTitle);
	topologyTitleLayout->addStretch(1);

	floorFilterCombo_ = new QComboBox(topologyCard);
	floorFilterCombo_->addItem(QStringLiteral("全部楼层"), QVariant(-999));
	floorFilterCombo_->setFixedWidth(100);
	floorFilterCombo_->setStyleSheet(QStringLiteral(
		"QComboBox{background:#FDF8F0;color:#3D322C;border:1px solid #C4A882;"
		"border-radius:6px;padding:2px 8px;font-size:12px;}"
		"QComboBox:hover{border-color:#8B5E7A;}"));
	topologyTitleLayout->addWidget(floorFilterCombo_);
	connect(floorFilterCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged),
		this, [this](int) {
			rebuildTopologyPlot(nullptr);
			rebuildHeatmapPlot(nullptr);
		});

	heatmapOverlayBtn_ = new QPushButton(QStringLiteral("热力图"), topologyCard);
	heatmapOverlayBtn_->setFixedSize(52, 22);
	heatmapOverlayBtn_->setCheckable(true);
	heatmapOverlayBtn_->setStyleSheet(QStringLiteral(
		"QPushButton{background:#E8DDD0;color:#3D322C;border:1px solid #C4A882;"
		"border-radius:4px;font-size:11px;}"
		"QPushButton:hover{background:#D4C5B2;}"
		"QPushButton:checked{background:#C43D3D;color:white;border-color:#A83232;}"));
	topologyTitleLayout->addWidget(heatmapOverlayBtn_);
	connect(heatmapOverlayBtn_, &QPushButton::toggled, this, [this](bool) {
		rebuildTopologyPlot(nullptr);
	});

	auto *topoExpandBtn = new QPushButton(QStringLiteral("展开"), topologyCard);
	topoExpandBtn->setFixedSize(40, 22);
	topoExpandBtn->setStyleSheet(QStringLiteral(
		"QPushButton{background:#E8DDD0;color:#3D322C;border:1px solid #C4A882;"
		"border-radius:4px;font-size:11px;}"
		"QPushButton:hover{background:#D4C5B2;}"));
	topologyTitleLayout->addWidget(topoExpandBtn);
	connect(topoExpandBtn, &QPushButton::clicked, this, [this, topoExpandBtn]() {
		toggleMaximizeCard(topologyCard_, topoExpandBtn);
	});

	topologyLayout->addLayout(topologyTitleLayout);
	// ======================================================================

	topologyPlot_ = new QCustomPlot(topologyCard);
	topologyPlot_->setMinimumHeight(320);
	topologyPlot_->setBackground(QBrush(QColor("#FDF8F0")));
	topologyPlot_->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
	topologyPlot_->setMouseTracking(true);
	connect(topologyPlot_, &QCustomPlot::mouseMove, this, [this](QMouseEvent *event) {
		handleTopologyMouseMove(event);
	});
	topologyPlot_->legend->setVisible(false);
	topologyPlot_->xAxis->setLabel(QStringLiteral("X坐标"));
	topologyPlot_->yAxis->setLabel(QStringLiteral("Y坐标"));
	topologyPlot_->xAxis->setBasePen(QPen(QColor("#B8A0A0")));
	topologyPlot_->yAxis->setBasePen(QPen(QColor("#B8A0A0")));
	topologyPlot_->xAxis->setTickPen(QPen(QColor("#B8A0A0")));
	topologyPlot_->yAxis->setTickPen(QPen(QColor("#B8A0A0")));
	topologyPlot_->xAxis->setSubTickPen(QPen(QColor("#D4C5C0")));
	topologyPlot_->yAxis->setSubTickPen(QPen(QColor("#D4C5C0")));
	topologyPlot_->xAxis->setTickLabelColor(QColor("#8B7D6B"));
	topologyPlot_->yAxis->setTickLabelColor(QColor("#8B7D6B"));
	topologyPlot_->xAxis->setLabelColor(QColor("#8B7D6B"));
	topologyPlot_->yAxis->setLabelColor(QColor("#8B7D6B"));
	topologyPlot_->xAxis->grid()->setPen(QPen(QColor(200, 190, 180, 70)));
	topologyPlot_->yAxis->grid()->setPen(QPen(QColor(200, 190, 180, 70)));

	auto *topoRow = new QHBoxLayout();
	topoRow->setContentsMargins(0, 0, 0, 0);
	topoRow->setSpacing(6);
	topoRow->addWidget(topologyPlot_, 1);

	nodeListPanel_ = new QWidget(topologyCard);
	nodeListPanel_->setFixedWidth(200);
	nodeListPanel_->setStyleSheet(QStringLiteral("background:#F5EDE0;border:1px solid #C4A882;border-radius:6px;"));
	auto *nodeListOuterLayout = new QVBoxLayout(nodeListPanel_);
	nodeListOuterLayout->setContentsMargins(6, 6, 6, 6);
	nodeListOuterLayout->setSpacing(0);

	auto *nodeListHeader = new QLabel(QStringLiteral("节点列表"), nodeListPanel_);
	nodeListHeader->setStyleSheet(QStringLiteral("font-size:12px;font-weight:bold;color:#3D322C;padding:2px 4px;border:none;"));
	nodeListOuterLayout->addWidget(nodeListHeader);

	auto *nodeListScroll = new QScrollArea(nodeListPanel_);
	nodeListScroll->setWidgetResizable(true);
	nodeListScroll->setStyleSheet(QStringLiteral("QScrollArea{border:none;background:transparent;}"));
	auto *nodeListContent = new QWidget();
	nodeListContent->setStyleSheet(QStringLiteral("background:transparent;"));
	nodeListLayout_ = new QVBoxLayout(nodeListContent);
	nodeListLayout_->setContentsMargins(2, 2, 2, 2);
	nodeListLayout_->setSpacing(1);
	nodeListLayout_->addStretch();
	nodeListScroll->setWidget(nodeListContent);
	nodeListOuterLayout->addWidget(nodeListScroll, 1);

	topoRow->addWidget(nodeListPanel_);
	topologyLayout->addLayout(topoRow, 1);

	heatmapCard_ = createCardFrame();
	auto *heatmapCard = heatmapCard_;
	auto *heatmapLayout = new QVBoxLayout(heatmapCard);
	heatmapLayout->setContentsMargins(12, 12, 12, 12);

	// ====================== 修复：热力图标题（原生方式） ======================
	auto *heatmapTitleLayout = new QHBoxLayout();
	heatmapTitleLayout->setContentsMargins(0, 0, 0, 0);
	heatmapTitleLayout->setSpacing(8);

	auto *heatmapIcon = new QLabel(heatmapCard);
	QPixmap heatmapPixmap = loadPixmapWithSvgSupport(resourcePath(QStringLiteral("icons/alert_heatmap.svg")), 16, 16);
	heatmapIcon->setPixmap(heatmapPixmap);

	auto *heatmapTitle = new QLabel(QStringLiteral("热力图"), heatmapCard);
	heatmapTitle->setObjectName(QStringLiteral("Headline"));
	heatmapTitle->setStyleSheet(QStringLiteral("font-size: 16px; color:#3D322C;"));

	heatmapTitleLayout->addWidget(heatmapIcon);
	heatmapTitleLayout->addWidget(heatmapTitle);
	heatmapTitleLayout->addStretch(1);

	auto *heatExpandBtn = new QPushButton(QStringLiteral("展开"), heatmapCard);
	heatExpandBtn->setFixedSize(40, 22);
	heatExpandBtn->setStyleSheet(QStringLiteral(
		"QPushButton{background:#E8DDD0;color:#3D322C;border:1px solid #C4A882;"
		"border-radius:4px;font-size:11px;}"
		"QPushButton:hover{background:#D4C5B2;}"));
	heatmapTitleLayout->addWidget(heatExpandBtn);
	connect(heatExpandBtn, &QPushButton::clicked, this, [this, heatExpandBtn]() {
		toggleMaximizeCard(heatmapCard_, heatExpandBtn);
	});

	heatmapLayout->addLayout(heatmapTitleLayout);
	// ======================================================================

	heatmapPlot_ = new QCustomPlot(heatmapCard);
	heatmapPlot_->setMinimumHeight(280);
	heatmapPlot_->setBackground(QBrush(QColor("#FDF8F0")));
	heatmapPlot_->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
	heatmapPlot_->xAxis->setLabel(QStringLiteral("X坐标"));
	heatmapPlot_->yAxis->setLabel(QStringLiteral("Y坐标"));
	heatmapPlot_->xAxis->setBasePen(QPen(QColor("#B8A0A0")));
	heatmapPlot_->yAxis->setBasePen(QPen(QColor("#B8A0A0")));
	heatmapPlot_->xAxis->setTickPen(QPen(QColor("#B8A0A0")));
	heatmapPlot_->yAxis->setTickPen(QPen(QColor("#B8A0A0")));
	heatmapPlot_->xAxis->setTickLabelColor(QColor("#8B7D6B"));
	heatmapPlot_->yAxis->setTickLabelColor(QColor("#8B7D6B"));
	heatmapPlot_->xAxis->setLabelColor(QColor("#8B7D6B"));
	heatmapPlot_->yAxis->setLabelColor(QColor("#8B7D6B"));
	heatmapColorMap_ = new QCPColorMap(heatmapPlot_->xAxis, heatmapPlot_->yAxis);
	heatmapColorScale_ = new QCPColorScale(heatmapPlot_);
	heatmapPlot_->plotLayout()->addElement(0, 1, heatmapColorScale_);
	heatmapColorMap_->setColorScale(heatmapColorScale_);
	heatmapColorScale_->axis()->setLabel(QStringLiteral("密度"));
	heatmapColorScale_->axis()->setLabelColor(QColor("#8B7D6B"));
	heatmapColorScale_->axis()->setTickLabelColor(QColor("#8B7D6B"));
	heatmapColorScale_->axis()->setBasePen(QPen(QColor("#B8A0A0")));
	QCPColorGradient gradient;
	gradient.setColorStopAt(0.0, QColor("#E8F0EE"));
	gradient.setColorStopAt(0.35, QColor("#4A9C8C"));
	gradient.setColorStopAt(0.7, QColor("#D4953A"));
	gradient.setColorStopAt(1.0, QColor("#C43D3D"));
	heatmapColorMap_->setGradient(gradient);
	heatmapColorMap_->setDataRange(QCPRange(0.0, 1.0));
	heatmapLayout->addWidget(heatmapPlot_, 1);

	leftLayout->addWidget(topologyCard, 3);
	leftLayout->addWidget(heatmapCard, 2);

	auto *rightWidget = new QWidget(mainSplitter);
	auto *rightLayout = new QVBoxLayout(rightWidget);
	rightLayout->setContentsMargins(0, 0, 0, 0);
	rightLayout->setSpacing(12);

	statsCard_ = createCardFrame();
	auto *statsCard = statsCard_;
	auto *statsLayout = new QVBoxLayout(statsCard);
	statsLayout->setContentsMargins(12, 12, 12, 12);

	// ====================== 修复：统计图表标题（原生方式） ======================
	auto *statsTitleLayout = new QHBoxLayout();
	statsTitleLayout->setContentsMargins(0, 0, 0, 0);
	statsTitleLayout->setSpacing(8);

	auto *statsIcon = new QLabel(statsCard);
	QPixmap statsPixmap = loadPixmapWithSvgSupport(resourcePath(QStringLiteral("icons/passenger_flow.svg")), 16, 16);
	statsIcon->setPixmap(statsPixmap);

	auto *statsTitle = new QLabel(QStringLiteral("统计图表"), statsCard);
	statsTitle->setObjectName(QStringLiteral("Headline"));
	statsTitle->setStyleSheet(QStringLiteral("font-size: 16px; color:#3D322C;"));

	statsTitleLayout->addWidget(statsIcon);
	statsTitleLayout->addWidget(statsTitle);
	statsTitleLayout->addStretch(1);

	auto *statsExpandBtn = new QPushButton(QStringLiteral("展开"), statsCard);
	statsExpandBtn->setFixedSize(40, 22);
	statsExpandBtn->setStyleSheet(QStringLiteral(
		"QPushButton{background:#E8DDD0;color:#3D322C;border:1px solid #C4A882;"
		"border-radius:4px;font-size:11px;}"
		"QPushButton:hover{background:#D4C5B2;}"));
	statsTitleLayout->addWidget(statsExpandBtn);
	connect(statsExpandBtn, &QPushButton::clicked, this, [this, statsExpandBtn]() {
		toggleMaximizeCard(statsCard_, statsExpandBtn);
	});

	statsLayout->addLayout(statsTitleLayout);
	// ======================================================================

	statisticsPlot_ = new QCustomPlot(statsCard);
	statisticsPlot_->setMinimumHeight(280);
	statisticsPlot_->setBackground(QBrush(QColor("#FDF8F0")));
	statisticsPlot_->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
	statisticsPlot_->legend->setVisible(true);
	statisticsPlot_->legend->setBrush(QBrush(QColor(254, 250, 243, 235)));
	statisticsPlot_->legend->setBorderPen(QPen(QColor("#D4C5B2"), 1));
	statisticsPlot_->legend->setTextColor(QColor("#3D322C"));
	statisticsPlot_->legend->setFont(QFont(QStringLiteral("Segoe UI"), 10, QFont::DemiBold));
	statisticsPlot_->xAxis->setLabel(QStringLiteral("时间(秒)"));
	statisticsPlot_->yAxis->setLabel(QStringLiteral("乘客数量"));
	statisticsPlot_->xAxis->setBasePen(QPen(QColor("#B8A0A0")));
	statisticsPlot_->yAxis->setBasePen(QPen(QColor("#B8A0A0")));
	statisticsPlot_->xAxis->setTickPen(QPen(QColor("#B8A0A0")));
	statisticsPlot_->yAxis->setTickPen(QPen(QColor("#B8A0A0")));
	statisticsPlot_->xAxis->setTickLabelColor(QColor("#8B7D6B"));
	statisticsPlot_->yAxis->setTickLabelColor(QColor("#8B7D6B"));
	statisticsPlot_->xAxis->setLabelColor(QColor("#8B7D6B"));
	statisticsPlot_->yAxis->setLabelColor(QColor("#8B7D6B"));
	statisticsPlot_->xAxis->grid()->setPen(QPen(QColor(200, 190, 180, 70)));
	statisticsPlot_->yAxis->grid()->setPen(QPen(QColor(200, 190, 180, 70)));
	activeGraph_ = statisticsPlot_->addGraph();
	completedGraph_ = statisticsPlot_->addGraph();
	timeoutGraph_ = statisticsPlot_->addGraph();
	activeGraph_->setName(QStringLiteral("活跃"));
	completedGraph_->setName(QStringLiteral("已完成"));
	timeoutGraph_->setName(QStringLiteral("超时"));
	activeGraph_->setPen(QPen(lineColorForIndex(0), 3));
	completedGraph_->setPen(QPen(lineColorForIndex(1), 3));
	timeoutGraph_->setPen(QPen(lineColorForIndex(2), 3));
	statsLayout->addWidget(statisticsPlot_, 1);

	// ====================== 数据统计面板 ======================
	analyticsCard_ = createCardFrame();
	auto *analyticsLayout = new QVBoxLayout(analyticsCard_);
	analyticsLayout->setContentsMargins(12, 12, 12, 12);

	auto *analyticsTitleLayout = new QHBoxLayout();
	analyticsTitleLayout->setContentsMargins(0, 0, 0, 0);
	analyticsTitleLayout->setSpacing(8);

	auto *analyticsIcon = new QLabel(analyticsCard_);
	QPixmap analyticsPixmap = loadPixmapWithSvgSupport(resourcePath(QStringLiteral("icons/passenger_flow.svg")), 16, 16);
	analyticsIcon->setPixmap(analyticsPixmap);

	auto *analyticsTitle = new QLabel(QStringLiteral("数据分析"), analyticsCard_);
	analyticsTitle->setObjectName(QStringLiteral("Headline"));
	analyticsTitle->setStyleSheet(QStringLiteral("font-size: 16px; color:#3D322C;"));

	analyticsTitleLayout->addWidget(analyticsIcon);
	analyticsTitleLayout->addWidget(analyticsTitle);
	analyticsTitleLayout->addStretch(1);

	auto *analyticsExpandBtn = new QPushButton(QStringLiteral("展开"), analyticsCard_);
	analyticsExpandBtn->setFixedSize(40, 22);
	analyticsExpandBtn->setStyleSheet(QStringLiteral(
		"QPushButton{background:#E8DDD0;color:#3D322C;border:1px solid #C4A882;"
		"border-radius:4px;font-size:11px;}"
		"QPushButton:hover{background:#D4C5B2;}"));
	analyticsTitleLayout->addWidget(analyticsExpandBtn);
	connect(analyticsExpandBtn, &QPushButton::clicked, this, [this, analyticsExpandBtn]() {
		toggleMaximizeCard(analyticsCard_, analyticsExpandBtn);
	});

	analyticsLayout->addLayout(analyticsTitleLayout);

	auto *summaryRow = new QHBoxLayout();
	summaryRow->setSpacing(12);

	auto makeSummaryBox = [&](const QString &title, QLabel *&valueLabel) {
		auto *box = new QFrame(analyticsCard_);
		box->setStyleSheet(QStringLiteral(
			"QFrame{background:#F5EDE0;border:1px solid #D4C5B2;border-radius:8px;padding:6px;}"));
		auto *boxLayout = new QVBoxLayout(box);
		boxLayout->setContentsMargins(8, 4, 8, 4);
		boxLayout->setSpacing(2);
		auto *titleLabel = new QLabel(title, box);
		titleLabel->setStyleSheet(QStringLiteral("font-size:10px;color:#8B7D6B;border:none;"));
		titleLabel->setAlignment(Qt::AlignCenter);
		valueLabel = new QLabel(QStringLiteral("--"), box);
		valueLabel->setStyleSheet(QStringLiteral("font-size:14px;font-weight:bold;color:#2C2418;border:none;"));
		valueLabel->setAlignment(Qt::AlignCenter);
		boxLayout->addWidget(titleLabel);
		boxLayout->addWidget(valueLabel);
		return box;
	};

	summaryRow->addWidget(makeSummaryBox(QStringLiteral("吞吐量/分"), analyticsThroughput_));
	summaryRow->addWidget(makeSummaryBox(QStringLiteral("平均通行"), analyticsAvgTravel_));
	summaryRow->addWidget(makeSummaryBox(QStringLiteral("最大排队"), analyticsMaxQueue_));
	summaryRow->addWidget(makeSummaryBox(QStringLiteral("拥堵热点"), analyticsCongestionNode_));
	analyticsLayout->addLayout(summaryRow);

	analyticsTable_ = new QTableWidget(analyticsCard_);
	analyticsTable_->setColumnCount(5);
	analyticsTable_->setHorizontalHeaderLabels({
		QStringLiteral("区域"),
		QStringLiteral("类型"),
		QStringLiteral("当前人数"),
		QStringLiteral("容量"),
		QStringLiteral("密度")
	});
	analyticsTable_->horizontalHeader()->setStretchLastSection(true);
	analyticsTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	analyticsTable_->verticalHeader()->setVisible(false);
	analyticsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
	analyticsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
	analyticsTable_->setAlternatingRowColors(true);
	analyticsTable_->setStyleSheet(QStringLiteral(
		"QTableWidget{background:#FDF8F0;border:1px solid #D4C5B2;border-radius:6px;"
		"font-size:11px;color:#3D322C;gridline-color:#E8DDD0;}"
		"QHeaderView::section{background:#EDE4D8;color:#3D322C;border:none;"
		"padding:4px;font-size:11px;font-weight:bold;}"
		"QTableWidget::item:alternate{background:#F5EDE0;}"
		"QTableWidget::item:selected{background:#D4953A;color:white;}"));
	analyticsLayout->addWidget(analyticsTable_, 1);

	// ====================== 乘客详情面板 ======================
	passengerCard_ = createCardFrame();
	auto *passengerLayout = new QVBoxLayout(passengerCard_);
	passengerLayout->setContentsMargins(12, 12, 12, 12);

	auto *passengerTitleLayout = new QHBoxLayout();
	passengerTitleLayout->setContentsMargins(0, 0, 0, 0);
	passengerTitleLayout->setSpacing(8);

	auto *passengerIcon = new QLabel(passengerCard_);
	QPixmap passengerPixmap = loadPixmapWithSvgSupport(resourcePath(QStringLiteral("icons/node_entrance.svg")), 16, 16);
	passengerIcon->setPixmap(passengerPixmap);

	auto *passengerTitle = new QLabel(QStringLiteral("乘客详情"), passengerCard_);
	passengerTitle->setObjectName(QStringLiteral("Headline"));
	passengerTitle->setStyleSheet(QStringLiteral("font-size: 16px; color:#3D322C;"));

	passengerTitleLayout->addWidget(passengerIcon);
	passengerTitleLayout->addWidget(passengerTitle);
	passengerTitleLayout->addStretch(1);

	auto *passengerExpandBtn = new QPushButton(QStringLiteral("展开"), passengerCard_);
	passengerExpandBtn->setFixedSize(40, 22);
	passengerExpandBtn->setStyleSheet(QStringLiteral(
		"QPushButton{background:#E8DDD0;color:#3D322C;border:1px solid #C4A882;"
		"border-radius:4px;font-size:11px;}"
		"QPushButton:hover{background:#D4C5B2;}"));
	passengerTitleLayout->addWidget(passengerExpandBtn);
	connect(passengerExpandBtn, &QPushButton::clicked, this, [this, passengerExpandBtn]() {
		toggleMaximizeCard(passengerCard_, passengerExpandBtn);
	});

	passengerLayout->addLayout(passengerTitleLayout);

	passengerTable_ = new QTableWidget(passengerCard_);
	passengerTable_->setColumnCount(7);
	passengerTable_->setHorizontalHeaderLabels({
		QStringLiteral("ID"),
		QStringLiteral("状态"),
		QStringLiteral("速度"),
		QStringLiteral("耐心度"),
		QStringLiteral("熟悉度"),
		QStringLiteral("当前位置"),
		QStringLiteral("等待时间")
	});
	passengerTable_->horizontalHeader()->setStretchLastSection(true);
	passengerTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
	passengerTable_->verticalHeader()->setVisible(false);
	passengerTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
	passengerTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
	passengerTable_->setAlternatingRowColors(true);
	passengerTable_->setStyleSheet(QStringLiteral(
		"QTableWidget{background:#FDF8F0;border:1px solid #D4C5B2;border-radius:6px;"
		"font-size:11px;color:#3D322C;gridline-color:#E8DDD0;}"
		"QHeaderView::section{background:#EDE4D8;color:#3D322C;border:none;"
		"padding:4px;font-size:11px;font-weight:bold;}"
		"QTableWidget::item:alternate{background:#F5EDE0;}"
		"QTableWidget::item:selected{background:#D4953A;color:white;}"));
	passengerLayout->addWidget(passengerTable_, 1);

	logCard_ = createCardFrame();
	auto *logCard = logCard_;
	auto *logLayout = new QVBoxLayout(logCard);
	logLayout->setContentsMargins(12, 12, 12, 12);

	// ====================== 修复：事件日志标题（原生方式） ======================
	auto *logTitleLayout = new QHBoxLayout();
	logTitleLayout->setContentsMargins(0, 0, 0, 0);
	logTitleLayout->setSpacing(8);

	auto *logIcon = new QLabel(logCard);
	QPixmap logPixmap = loadPixmapWithSvgSupport(resourcePath(QStringLiteral("icons/trajectory_log.svg")), 16, 16);
	logIcon->setPixmap(logPixmap);

	auto *logTitle = new QLabel(QStringLiteral("事件日志"), logCard);
	logTitle->setObjectName(QStringLiteral("Headline"));
	logTitle->setStyleSheet(QStringLiteral("font-size: 16px; color:#3D322C;"));

	logTitleLayout->addWidget(logIcon);
	logTitleLayout->addWidget(logTitle);
	logTitleLayout->addStretch(1);

	auto *logExpandBtn = new QPushButton(QStringLiteral("展开"), logCard);
	logExpandBtn->setFixedSize(40, 22);
	logExpandBtn->setStyleSheet(QStringLiteral(
		"QPushButton{background:#E8DDD0;color:#3D322C;border:1px solid #C4A882;"
		"border-radius:4px;font-size:11px;}"
		"QPushButton:hover{background:#D4C5B2;}"));
	logTitleLayout->addWidget(logExpandBtn);
	connect(logExpandBtn, &QPushButton::clicked, this, [this, logExpandBtn]() {
		toggleMaximizeCard(logCard_, logExpandBtn);
	});

	logLayout->addLayout(logTitleLayout);
	// ======================================================================

	eventStack_ = new QStackedWidget(logCard);
	eventEmptyPage_ = new QWidget(eventStack_);
	auto *emptyLayout = new QVBoxLayout(eventEmptyPage_);
	emptyLayout->setContentsMargins(8, 8, 8, 8);
	emptyLayout->setSpacing(8);
	auto *emptyImage = new QLabel(eventEmptyPage_);
	emptyImage->setAlignment(Qt::AlignCenter);
	const QString emptyStatePath = resolveFirstExistingPath({
		QStringLiteral("ui/empty_state.svg"),
	});
	QPixmap emptyPixmap = loadPixmapWithSvgSupport(emptyStatePath, 280, 280);
	if (!emptyPixmap.isNull()) {
		emptyImage->setPixmap(emptyPixmap.scaled(220, 220, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	} else {
		emptyImage->setText(QStringLiteral("加载空状态图片失败"));
		emptyImage->setStyleSheet(QStringLiteral("color:#8B7D6B;"));
	}
	auto *emptyHint = new QLabel(QStringLiteral("暂无事件。点击开始按钮查看实时日志。"), eventEmptyPage_);
	emptyHint->setAlignment(Qt::AlignCenter);
	emptyHint->setStyleSheet(QStringLiteral("color:#8B7D6B; font-size:13px;"));
	emptyLayout->addStretch(1);
	emptyLayout->addWidget(emptyImage);
	emptyLayout->addWidget(emptyHint);
	emptyLayout->addStretch(1);
	eventEmptyPage_->setStyleSheet(QStringLiteral("background-color:#FDF8F0; border:1px solid #E0D5C5; border-radius:12px;"));

	eventLog_ = new QPlainTextEdit(logCard);
	eventLog_->setReadOnly(true);
	eventLog_->setPlaceholderText(QStringLiteral("等待事件中..."));
	eventLog_->setStyleSheet(QStringLiteral("QPlainTextEdit{background-color:#FDF8F0;color:#3D322C;border:1px solid #E0D5C5;border-radius:12px;}"));
	eventStack_->addWidget(eventEmptyPage_);
	eventStack_->addWidget(eventLog_);
	eventStack_->setCurrentWidget(eventEmptyPage_);
	logLayout->addWidget(eventStack_, 1);

	rightLayout->addWidget(statsCard, 2);
	rightLayout->addWidget(analyticsCard_, 2);
	rightLayout->addWidget(passengerCard_, 2);
	rightLayout->addWidget(logCard, 2);

	auto *rightScrollArea = new QScrollArea(mainSplitter);
	rightScrollArea->setWidget(rightWidget);
	rightScrollArea->setWidgetResizable(true);
	rightScrollArea->setFrameShape(QFrame::NoFrame);
	rightScrollArea->setStyleSheet(QStringLiteral("QScrollArea{background:transparent;border:none;}"));
	rightPanel_ = rightScrollArea;

	mainSplitter->addWidget(leftWidget);
	mainSplitter->addWidget(rightScrollArea);
	mainSplitter->setStretchFactor(0, 3);
	mainSplitter->setStretchFactor(1, 2);
	rootLayout->addWidget(mainSplitter, 1);
}

QFrame *VisualizationWidget::createCardFrame()
{
    auto *frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("CardFrame"));
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setFrameShadow(QFrame::Raised);
    
    frame->setStyleSheet(
        QStringLiteral("QFrame#CardFrame{"
                       "background-color: rgba(254, 250, 243, 0.95);"
                       "border: 1px solid rgba(180, 160, 140, 0.25);"
                       "border-radius: 16px;"
                       "}"));
    

    auto *shadowEffect = new QGraphicsDropShadowEffect(frame);
    shadowEffect->setBlurRadius(28);
    shadowEffect->setXOffset(0);
    shadowEffect->setYOffset(6);
    shadowEffect->setColor(QColor(139, 94, 122, 45));
    frame->setGraphicsEffect(shadowEffect);
    
    return frame;
}

void VisualizationWidget::clearHistory()
{
	timeHistory_.clear();
	activeHistory_.clear();
	completedHistory_.clear();
	timeoutHistory_.clear();
	lastEventCount_ = 0;
	lastTime_ = -1.0;
	lastCompletedForThroughput_ = 0;
	lastTimeForThroughput_ = 0;
	currentThroughput_ = 0.0;
	if (eventLog_) {
		eventLog_->clear();
	}
	if (eventStack_ && eventEmptyPage_) {
		eventStack_->setCurrentWidget(eventEmptyPage_);
	}
	if (statisticsPlot_) {
		statisticsPlot_->replot();
	}
}

void VisualizationWidget::toggleMaximizeCard(QFrame *card, QPushButton *btn)
{
	if (!mainSplitter_) return;

	if (maximizedCard_ == card) {
		topologyCard_->show();
		heatmapCard_->show();
		statsCard_->show();
		logCard_->show();
		if (analyticsCard_) analyticsCard_->show();
		if (passengerCard_) passengerCard_->show();

		if (leftPanel_) leftPanel_->show();
		if (rightPanel_) rightPanel_->show();

		if (!savedSplitterSizes_.isEmpty()) {
			mainSplitter_->setSizes(savedSplitterSizes_);
			savedSplitterSizes_.clear();
		}

		maximizedCard_ = nullptr;
		btn->setText(QStringLiteral("展开"));
	} else {
		if (!maximizedCard_) {
			savedSplitterSizes_ = mainSplitter_->sizes();
		}

		QFrame *allCards[] = { topologyCard_, heatmapCard_, statsCard_, logCard_, analyticsCard_, passengerCard_ };
		for (auto *c : allCards) {
			if (c && c != card) c->hide();
		}
		card->show();

		bool inLeftPanel = (card == topologyCard_ || card == heatmapCard_);
		if (inLeftPanel) {
			if (rightPanel_) rightPanel_->hide();
		} else {
			if (leftPanel_) leftPanel_->hide();
		}

		maximizedCard_ = card;
		btn->setText(QStringLiteral("还原"));
	}
}

void VisualizationWidget::setHighlightedPath(const std::vector<std::string> &path)
{
	highlightedPath_ = path;
	rebuildTopologyPlot();
}

void VisualizationWidget::setComparedPaths(const std::vector<std::pair<std::vector<std::string>, QColor>> &paths)
{
	comparedPaths_ = paths;
	rebuildTopologyPlot();
}

void VisualizationWidget::clearComparedPaths()
{
	comparedPaths_.clear();
	rebuildTopologyPlot();
}

void VisualizationWidget::setGraph(const MetroGraph &graph)
{
	graph_ = graph;
	hasGraph_ = true;
	renderedNodes_.clear();
	clearHistory();

	if (floorFilterCombo_) {
		floorFilterCombo_->blockSignals(true);
		floorFilterCombo_->clear();
		floorFilterCombo_->addItem(QStringLiteral("全部楼层"), QVariant(-999));

		std::vector<int> floors;
		const auto &declaredFloors = graph_.floors();
		if (!declaredFloors.empty()) {
			floors = declaredFloors;
		} else {
			std::set<int> uniqueFloors;
			for (const auto &entry : graph_.nodes()) {
				uniqueFloors.insert(entry.second.floor);
			}
			floors.assign(uniqueFloors.begin(), uniqueFloors.end());
			std::sort(floors.begin(), floors.end());
		}
		for (int floor : floors) {
			floorFilterCombo_->addItem(floorToString(floor), QVariant(floor));
		}
		floorFilterCombo_->blockSignals(false);
	}

	rebuildTopologyPlot();
	rebuildHeatmapPlot();
	refreshStatisticsPlot();
}

int VisualizationWidget::nodePassengerCount(const std::string &nodeId, const Simulation &simulation) const
{
	const auto &occ = simulation.nodeOccupancy();
	auto it = occ.find(nodeId);
	return (it != occ.end()) ? it->second : 0;
}

double VisualizationWidget::nodeDensity(const std::string &nodeId, const Simulation &simulation) const
{
	const auto &occ = simulation.nodeOccupancy();
	auto occIt = occ.find(nodeId);
	int count = (occIt != occ.end()) ? occIt->second : 0;

	auto it = graph_.nodes().find(nodeId);
	if (it == graph_.nodes().end() || it->second.capacity <= 0.0) {
		return 0.0;
	}
	return static_cast<double>(count) / it->second.capacity;
}

QColor VisualizationWidget::densityColor(double density) const
{
	return densityToColor(density);
}

void VisualizationWidget::rebuildTopologyPlot(const Simulation *simulation)
{
	if (!topologyPlot_) {
		return;
	}

	topologyPlot_->clearItems();
	topologyPlot_->clearGraphs();
	topologyPlot_->graphCount();

	if (!hasGraph_ || graph_.nodes().empty()) {
		topologyPlot_->xAxis->setRange(0, 1);
		topologyPlot_->yAxis->setRange(0, 1);
		topologyPlot_->replot();
		return;
	}

	int selectedFloor = -999;
	if (floorFilterCombo_) selectedFloor = floorFilterCombo_->currentData().toInt();
	bool filterByFloor = (selectedFloor != -999);

	double minX = std::numeric_limits<double>::max();
	double maxX = std::numeric_limits<double>::lowest();
	double minY = std::numeric_limits<double>::max();
	double maxY = std::numeric_limits<double>::lowest();
	for (const auto &entry : graph_.nodes()) {
		if (filterByFloor && entry.second.floor != selectedFloor) continue;
		minX = std::min(minX, entry.second.x);
		maxX = std::max(maxX, entry.second.x);
		minY = std::min(minY, entry.second.y);
		maxY = std::max(maxY, entry.second.y);
	}
	if (minX > maxX) {
		topologyPlot_->xAxis->setRange(0, 1);
		topologyPlot_->yAxis->setRange(0, 1);
		topologyPlot_->replot();
		return;
	}
	const double spanX = std::max(1.0, maxX - minX);
	const double spanY = std::max(1.0, maxY - minY);
	const double radius = std::max(0.35, std::min(spanX, spanY) * 0.03);

	topologyNodeRadius_ = radius;
	renderedNodes_.clear();

	for (const auto &edge : graph_.edges()) {
		auto fromIt = graph_.nodes().find(edge.from);
		auto toIt = graph_.nodes().find(edge.to);
		if (fromIt == graph_.nodes().end() || toIt == graph_.nodes().end()) {
			continue;
		}
		if (filterByFloor && fromIt->second.floor != selectedFloor && toIt->second.floor != selectedFloor) {
			continue;
		}
		auto *line = new QCPItemLine(topologyPlot_);
		const QColor edgeColor = lineColorForIndex(std::max(0, edge.lineIndex - 1));
		line->setPen(QPen(edgeColor, 2));
		line->start->setCoords(fromIt->second.x, fromIt->second.y);
		line->end->setCoords(toIt->second.x, toIt->second.y);

		const QString lineIconPath = resourcePath(QStringLiteral("icons/") + QString::fromStdString(assets::lineIconForIndex(edge.lineIndex)));
		QPixmap lineIcon(lineIconPath);
		if (!lineIcon.isNull()) {
			auto *lineIconItem = new QCPItemPixmap(topologyPlot_);
			lineIconItem->setPixmap(lineIcon.scaled(24, 24, Qt::KeepAspectRatio, Qt::SmoothTransformation));
			const double midX = (fromIt->second.x + toIt->second.x) * 0.5;
			const double midY = (fromIt->second.y + toIt->second.y) * 0.5;
			lineIconItem->topLeft->setCoords(midX - radius * 0.45, midY + radius * 0.45);
			lineIconItem->bottomRight->setCoords(midX + radius * 0.45, midY - radius * 0.45);
		}
	}

	for (const auto &entry : graph_.nodes()) {
		const auto &node = entry.second;
		if (filterByFloor && node.floor != selectedFloor) continue;
		const int currentCount = simulation ? nodePassengerCount(node.id, *simulation) : 0;
		double density = simulation ? nodeDensity(node.id, *simulation) : 0.0;
		renderedNodes_.append(RenderedNodeInfo{
			node.id,
			node.name,
			node.type,
			node.x,
			node.y,
			node.capacity,
			currentCount,
			density,
			node.floor,
		});
		QColor fill = densityColor(density);
		auto *ellipse = new QCPItemEllipse(topologyPlot_);
		ellipse->topLeft->setCoords(node.x - radius * 0.85, node.y + radius * 0.85);
		ellipse->bottomRight->setCoords(node.x + radius * 0.85, node.y - radius * 0.85);
		ellipse->setPen(QPen(fill.darker(180), 2));
		ellipse->setBrush(QBrush(fill.lighter(120)));

		const QString iconPath = resourcePath(QStringLiteral("icons/") + QString::fromStdString(assets::nodeIconForType(node.type)));
		QPixmap icon(iconPath);
		if (!icon.isNull()) {
			auto *pix = new QCPItemPixmap(topologyPlot_);
			bool smallIcon = (node.type == "stairs" || node.type == "entrance" ||
				node.type == "exit" || node.type == "security" ||
				node.type == "ticket" || node.type == "gate");
			const double iconHalf = smallIcon ? radius * 0.3 : radius * 0.5;
			int iconPx = smallIcon ? 28 : 48;
			pix->setPixmap(icon.scaled(iconPx, iconPx, Qt::KeepAspectRatio, Qt::SmoothTransformation));
			pix->topLeft->setCoords(node.x - iconHalf, node.y + iconHalf);
			pix->bottomRight->setCoords(node.x + iconHalf, node.y - iconHalf);
		}
	}

	// Heatmap overlay on topology background (Item 7)
	if (heatmapOverlayBtn_ && heatmapOverlayBtn_->isChecked() && simulation) {
		const double sigma = std::max(spanX, spanY) / 6.0 + 0.1;
		const int gridW = 40, gridH = 40;
		const double xPad = spanX * 0.15, yPad = spanY * 0.15;
		for (int gy = 0; gy < gridH; ++gy) {
			for (int gx = 0; gx < gridW; ++gx) {
				double xPos = minX - xPad + (double(gx) / (gridW - 1)) * (spanX + 2 * xPad);
				double yPos = minY - yPad + (double(gy) / (gridH - 1)) * (spanY + 2 * yPad);
				double value = 0.0;
				for (const auto &rn : renderedNodes_) {
					double d = rn.density;
					if (d > 0.0) {
						double dx = xPos - rn.x, dy = yPos - rn.y;
						value += d * std::exp(-(dx*dx + dy*dy) / (2.0 * sigma * sigma));
					}
				}
				if (value > 0.01) {
					QColor c = densityColor(value);
					c.setAlpha(80);
					double cellW = (spanX + 2*xPad) / gridW / 2.0;
					double cellH = (spanY + 2*yPad) / gridH / 2.0;
					auto *rect = new QCPItemRect(topologyPlot_);
					rect->topLeft->setCoords(xPos - cellW, yPos + cellH);
					rect->bottomRight->setCoords(xPos + cellW, yPos - cellH);
					rect->setPen(Qt::NoPen);
					rect->setBrush(QBrush(c));
				}
			}
		}
	}

	if (!simulation) {
		topologyPlot_->xAxis->setRange(minX - spanX * 0.2, maxX + spanX * 0.2);
		topologyPlot_->yAxis->setRange(minY - spanY * 0.2, maxY + spanY * 0.25);
	}

	// Highlighted path overlay
	if (highlightedPath_.size() >= 2) {
		const auto &nodes = graph_.nodes();
		for (std::size_t i = 0; i + 1 < highlightedPath_.size(); ++i) {
			auto fromIt = nodes.find(highlightedPath_[i]);
			auto toIt = nodes.find(highlightedPath_[i + 1]);
			if (fromIt == nodes.end() || toIt == nodes.end()) continue;
			if (filterByFloor && fromIt->second.floor != selectedFloor && toIt->second.floor != selectedFloor) continue;
			auto *line = new QCPItemLine(topologyPlot_);
			line->setPen(QPen(QColor(196, 61, 61, 200), 5));
			line->start->setCoords(fromIt->second.x, fromIt->second.y);
			line->end->setCoords(toIt->second.x, toIt->second.y);
		}
		auto startIt = nodes.find(highlightedPath_.front());
		auto endIt = nodes.find(highlightedPath_.back());
		if (startIt != nodes.end()) {
			auto *marker = new QCPItemEllipse(topologyPlot_);
			double r = radius * 1.2;
			marker->topLeft->setCoords(startIt->second.x - r, startIt->second.y + r);
			marker->bottomRight->setCoords(startIt->second.x + r, startIt->second.y - r);
			marker->setPen(QPen(QColor("#2E8B57"), 3));
			marker->setBrush(QBrush(QColor(46, 139, 87, 60)));
		}
		if (endIt != nodes.end()) {
			auto *marker = new QCPItemEllipse(topologyPlot_);
			double r = radius * 1.2;
			marker->topLeft->setCoords(endIt->second.x - r, endIt->second.y + r);
			marker->bottomRight->setCoords(endIt->second.x + r, endIt->second.y - r);
			marker->setPen(QPen(QColor("#C43D3D"), 3));
			marker->setBrush(QBrush(QColor(196, 61, 61, 60)));
		}
	}

	if (!comparedPaths_.empty()) {
		const auto &nodes = graph_.nodes();
		for (const auto &cp : comparedPaths_) {
			const auto &path = cp.first;
			const QColor &color = cp.second;
			if (path.size() < 2) continue;
			for (std::size_t i = 0; i + 1 < path.size(); ++i) {
				auto fromIt = nodes.find(path[i]);
				auto toIt = nodes.find(path[i + 1]);
				if (fromIt == nodes.end() || toIt == nodes.end()) continue;
				if (filterByFloor && fromIt->second.floor != selectedFloor && toIt->second.floor != selectedFloor) continue;
				auto *line = new QCPItemLine(topologyPlot_);
				QPen pen(color, 3);
				pen.setStyle(Qt::DashLine);
				line->setPen(pen);
				line->start->setCoords(fromIt->second.x, fromIt->second.y);
				line->end->setCoords(toIt->second.x, toIt->second.y);
			}
		}
	}

	topologyPlot_->replot();
	if (!simulation) {
		rebuildNodeList();
	}
}

void VisualizationWidget::rebuildNodeList()
{
	if (!nodeListLayout_) return;

	while (nodeListLayout_->count() > 0) {
		QLayoutItem *item = nodeListLayout_->takeAt(0);
		if (item->widget()) {
			delete item->widget();
		}
		delete item;
	}
	nodeListInfoLabels_.clear();
	nodeDetailPanels_.clear();
	nodeDetailLabels_.clear();

	for (const auto &node : renderedNodes_) {
		auto *container = new QWidget();
		container->setStyleSheet(QStringLiteral("background:transparent;"));
		auto *containerLayout = new QVBoxLayout(container);
		containerLayout->setContentsMargins(0, 0, 0, 0);
		containerLayout->setSpacing(0);

		auto *row = new QWidget();
		row->setFixedHeight(22);
		row->setCursor(Qt::PointingHandCursor);
		row->setStyleSheet(QStringLiteral("background:transparent;"));
		auto *rowLayout = new QHBoxLayout(row);
		rowLayout->setContentsMargins(2, 0, 2, 0);
		rowLayout->setSpacing(4);

		auto *iconLabel = new QLabel();
		const QString iconPath = resourcePath(QStringLiteral("icons/") + QString::fromStdString(assets::nodeIconForType(node.type)));
		QPixmap pix(iconPath);
		if (!pix.isNull()) {
			iconLabel->setPixmap(pix.scaled(14, 14, Qt::KeepAspectRatio, Qt::SmoothTransformation));
		}
		iconLabel->setFixedSize(16, 16);
		rowLayout->addWidget(iconLabel);

		auto *infoLabel = new QLabel();
		infoLabel->setStyleSheet(QStringLiteral("font-size:10px;color:#2C2418;border:none;background:transparent;"));
		infoLabel->setText(QStringLiteral("%1 | %2 | %3")
			.arg(QString::fromStdString(node.name))
			.arg(nodeTypeToChinese(node.type))
			.arg(floorToString(node.floor)));
		rowLayout->addWidget(infoLabel, 1);

		containerLayout->addWidget(row);

		auto *detailPanel = new QWidget();
		detailPanel->setVisible(false);
		detailPanel->setStyleSheet(QStringLiteral(
			"background-color:#F5EDE0;border-radius:4px;margin:2px 4px 4px 20px;"));
		auto *detailLayout = new QVBoxLayout(detailPanel);
		detailLayout->setContentsMargins(8, 4, 8, 4);
		detailLayout->setSpacing(2);

		auto *detailLabel = new QLabel();
		detailLabel->setStyleSheet(QStringLiteral("font-size:10px;color:#2C2418;border:none;background:transparent;"));
		detailLabel->setText(QStringLiteral(
			"类型: %1\n楼层: %2\n当前人数: %3 / 容量: %4\n密度: %5%")
			.arg(nodeTypeToChinese(node.type))
			.arg(floorToString(node.floor))
			.arg(node.currentCount)
			.arg(node.capacity, 0, 'f', 0)
			.arg(node.density * 100.0, 0, 'f', 1));
		detailLayout->addWidget(detailLabel);

		containerLayout->addWidget(detailPanel);

		row->installEventFilter(this);
		row->setProperty("detailPanel", QVariant::fromValue(static_cast<QObject *>(detailPanel)));

		nodeListLayout_->addWidget(container);
		nodeListInfoLabels_.append(infoLabel);
		nodeDetailPanels_.append(detailPanel);
		nodeDetailLabels_.append(detailLabel);
	}

	nodeListLayout_->addStretch();
}

void VisualizationWidget::refreshNodeList()
{
	const int count = qMin(nodeListInfoLabels_.size(), renderedNodes_.size());
	for (int i = 0; i < count; ++i) {
		const auto &node = renderedNodes_[i];
		auto *label = nodeListInfoLabels_[i];
		label->setToolTip(QStringLiteral(
			"<div style='background-color:#F5EDE0;color:#2C2418;padding:4px;'>"
			"<b>%1</b><br>"
			"类型: %2 | 楼层: %3<br>"
			"当前人数: %4 / 容量: %5<br>"
			"密度: %6%</div>")
			.arg(QString::fromStdString(node.name))
			.arg(nodeTypeToChinese(node.type))
			.arg(floorToString(node.floor))
			.arg(node.currentCount)
			.arg(node.capacity, 0, 'f', 0)
			.arg(node.density * 100.0, 0, 'f', 1));

		if (i < nodeDetailLabels_.size() && nodeDetailLabels_[i]) {
			nodeDetailLabels_[i]->setText(QStringLiteral(
				"类型: %1\n楼层: %2\n当前人数: %3 / 容量: %4\n密度: %5%")
				.arg(nodeTypeToChinese(node.type))
				.arg(floorToString(node.floor))
				.arg(node.currentCount)
				.arg(node.capacity, 0, 'f', 0)
				.arg(node.density * 100.0, 0, 'f', 1));
		}
	}
}

bool VisualizationWidget::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() == QEvent::MouseButtonPress) {
		QVariant v = watched->property("detailPanel");
		if (v.isValid()) {
			auto *panel = qobject_cast<QWidget *>(v.value<QObject *>());
			if (panel) {
				panel->setVisible(!panel->isVisible());
				return true;
			}
		}
	}
	return QWidget::eventFilter(watched, event);
}

void VisualizationWidget::handleTopologyMouseMove(QMouseEvent *event)
{
	if (!topologyPlot_ || renderedNodes_.isEmpty() || !event) {
		QToolTip::hideText();
		return;
	}

	const double x = topologyPlot_->xAxis->pixelToCoord(event->position().x());
	const double y = topologyPlot_->yAxis->pixelToCoord(event->position().y());

	const RenderedNodeInfo *nearest = nullptr;
	double bestDistanceSquared = std::numeric_limits<double>::max();
	const double threshold = topologyNodeRadius_ * 2.5;
	const double thresholdSquared = threshold * threshold;

	for (const auto &node : renderedNodes_) {
		const double dx = x - node.x;
		const double dy = y - node.y;
		const double distanceSquared = dx * dx + dy * dy;
		if (distanceSquared <= thresholdSquared && distanceSquared < bestDistanceSquared) {
			bestDistanceSquared = distanceSquared;
			nearest = &node;
		}
	}

	if (!nearest) {
		QToolTip::hideText();
		return;
	}

	QStringList connectedLines;
	for (const auto &edge : graph_.edges()) {
		if (edge.from == nearest->id || edge.to == nearest->id) {
			const std::string &otherId = (edge.from == nearest->id) ? edge.to : edge.from;
			auto otherIt = graph_.nodes().find(otherId);
			QString otherName = (otherIt != graph_.nodes().end())
				? QString::fromStdString(otherIt->second.name) : QStringLiteral("?");
			connectedLines.append(QStringLiteral("  → %1 (线路%2)")
				.arg(otherName)
				.arg(edge.lineIndex));
		}
	}

	QString tooltip = QStringLiteral(
		"<div style='background-color:#F5EDE0;color:#2C2418;padding:4px;'>"
		"<b>%1</b><br>"
		"类型: %2 | 楼层: %3<br>"
		"当前人数: %4 / 容量: %5<br>"
		"密度: %6%")
		.arg(QString::fromStdString(nearest->name))
		.arg(nodeTypeToChinese(nearest->type))
		.arg(floorToString(nearest->floor))
		.arg(nearest->currentCount)
		.arg(nearest->capacity, 0, 'f', 0)
		.arg(nearest->density * 100.0, 0, 'f', 1);

	if (!connectedLines.isEmpty()) {
		tooltip += QStringLiteral("<br><br><b>连接:</b><br>") + connectedLines.join(QStringLiteral("<br>"));
	}
	tooltip += QStringLiteral("</div>");

	QToolTip::showText(event->globalPosition().toPoint(), tooltip, topologyPlot_);
}

void VisualizationWidget::rebuildHeatmapPlot(const Simulation *simulation)
{
	if (!heatmapPlot_ || !heatmapColorMap_) {
		return;
	}

	heatmapPlot_->clearItems();

	if (!hasGraph_ || graph_.nodes().empty()) {
		heatmapColorMap_->data()->setSize(60, 60);
		heatmapColorMap_->data()->fill(0.0);
		heatmapPlot_->xAxis->setRange(0, 1);
		heatmapPlot_->yAxis->setRange(0, 1);
		heatmapPlot_->replot();
		return;
	}

	int selectedFloor = -999;
	if (floorFilterCombo_) selectedFloor = floorFilterCombo_->currentData().toInt();
	bool filterByFloor = (selectedFloor != -999);

	double minX = std::numeric_limits<double>::max();
	double maxX = std::numeric_limits<double>::lowest();
	double minY = std::numeric_limits<double>::max();
	double maxY = std::numeric_limits<double>::lowest();
	for (const auto &entry : graph_.nodes()) {
		if (filterByFloor && entry.second.floor != selectedFloor) continue;
		minX = std::min(minX, entry.second.x);
		maxX = std::max(maxX, entry.second.x);
		minY = std::min(minY, entry.second.y);
		maxY = std::max(maxY, entry.second.y);
	}
	if (minX > maxX) {
		heatmapColorMap_->data()->setSize(40, 40);
		heatmapColorMap_->data()->fill(0.0);
		heatmapPlot_->xAxis->setRange(0, 1);
		heatmapPlot_->yAxis->setRange(0, 1);
		heatmapPlot_->replot();
		return;
	}
	const double spanX = std::max(1.0, maxX - minX);
	const double spanY = std::max(1.0, maxY - minY);
	const double xPadding = spanX * 0.15;
	const double yPadding = spanY * 0.18;
	const int width = std::clamp(static_cast<int>(spanX / 0.5), 40, 200);
	const int height = std::clamp(static_cast<int>(spanY / 0.5), 40, 200);

	heatmapColorMap_->data()->setSize(width, height);
	heatmapColorMap_->data()->setRange(QCPRange(minX - xPadding, maxX + xPadding), QCPRange(minY - yPadding, maxY + yPadding));
	const double sigma = std::max(spanX, spanY) / 6.0 + 0.1;

	QVector<std::pair<QPointF, double>> nodes;
	nodes.reserve(static_cast<int>(graph_.nodes().size()));
	for (const auto &entry : graph_.nodes()) {
		if (filterByFloor && entry.second.floor != selectedFloor) continue;
		const double density = simulation ? nodeDensity(entry.first, *simulation) : 0.0;
		nodes.append({QPointF(entry.second.x, entry.second.y), density});
	}

	double maxValue = 1.0;
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			const double xPos = minX - xPadding + (static_cast<double>(x) / (width - 1)) * (spanX + 2.0 * xPadding);
			const double yPos = minY - yPadding + (static_cast<double>(y) / (height - 1)) * (spanY + 2.0 * yPadding);
			double value = 0.0;
			for (const auto &node : nodes) {
				value += node.second * gaussian(xPos - node.first.x(), yPos - node.first.y(), sigma);
			}
			maxValue = std::max(maxValue, value);
			heatmapColorMap_->data()->setCell(x, y, value);
		}
	}

	heatmapColorMap_->setDataRange(QCPRange(0.0, maxValue * 1.05));
	heatmapPlot_->rescaleAxes();
	heatmapPlot_->replot();
}

void VisualizationWidget::refreshStatisticsPlot()
{
	if (!statisticsPlot_) {
		return;
	}

	activeGraph_->setData(timeHistory_, activeHistory_);
	completedGraph_->setData(timeHistory_, completedHistory_);
	timeoutGraph_->setData(timeHistory_, timeoutHistory_);

	const double maxTime = timeHistory_.isEmpty() ? 60.0 : std::max(60.0, timeHistory_.back());
	const double viewWidth = std::min(180.0, maxTime);
	statisticsPlot_->xAxis->setRange(maxTime - viewWidth, maxTime);

	double maxValue = 5.0;
	for (double value : activeHistory_) {
		maxValue = std::max(maxValue, value);
	}
	for (double value : completedHistory_) {
		maxValue = std::max(maxValue, value);
	}
	for (double value : timeoutHistory_) {
		maxValue = std::max(maxValue, value);
	}
	statisticsPlot_->yAxis->setRange(0, maxValue * 1.2 + 1.0);
	statisticsPlot_->replot();
}

void VisualizationWidget::appendEvents(const Simulation &simulation)
{
	if (!eventLog_) {
		return;
	}

	if (simulation.events().size() < lastEventCount_) {
		lastEventCount_ = 0;
		eventLog_->clear();
		if (headerEventFeed_) headerEventFeed_->clear();
	}

	for (std::size_t index = lastEventCount_; index < simulation.events().size(); ++index) {
		const auto &event = simulation.events()[index];
		if (eventStack_ && eventLog_) {
			eventStack_->setCurrentWidget(eventLog_);
		}
		const QString line = QStringLiteral("[%1] %2 | 乘客=%3 | 节点=%4 | %5")
								.arg(formatSimTime(event.time))
								.arg(eventTypeName(event.type))
								.arg(event.passengerId > 0 ? QString::number(event.passengerId) : QStringLiteral("-"))
								.arg(QString::fromStdString(event.nodeId.empty() ? std::string("-") : event.nodeId))
								.arg(QString::fromStdString(event.message));
		eventLog_->appendPlainText(line);

		if (headerEventFeed_ &&
			(event.type == EventType::PeakHourStarted ||
			 event.type == EventType::PeakHourEnded ||
			 event.type == EventType::TrainArrived ||
			 event.type == EventType::PassengerSurge ||
			 event.type == EventType::CongestionTriggered)) {
			const QString headerLine = QStringLiteral("[%1] %2")
										.arg(formatSimTime(event.time))
										.arg(QString::fromStdString(event.message));
			headerEventFeed_->appendPlainText(headerLine);
		}
	}
	lastEventCount_ = simulation.events().size();
	const QTextCursor cursor = eventLog_->textCursor();
	eventLog_->setTextCursor(cursor);
}

void VisualizationWidget::refreshSummary(const Simulation *simulation)
{
	if (!simulation) {
		return;
	}

	if (stationLabel_)
		stationLabel_->setText(QString::fromStdString(graph_.stationName()));
	if (timeLabel_)
		timeLabel_->setText(QStringLiteral("时间: %1 秒 | 节点: %2 | 边: %3")
								.arg(simulation->currentTime())
								.arg(graph_.nodeCount())
								.arg(graph_.edgeCount()));

	int activePassengers = 0;
	for (const auto &passenger : simulation->passengers()) {
		if (passenger.state != PassengerState::Finished) {
			++activePassengers;
		}
	}

	if (activeLabel_) activeLabel_->setText(QString::number(activePassengers));
	if (completedLabel_) completedLabel_->setText(QString::number(simulation->statistics().completedPassengers()));
	if (timeoutLabel_) timeoutLabel_->setText(QString::number(simulation->statistics().timedOutPassengers()));
	if (congestionLabel_) congestionLabel_->setText(QString::number(simulation->statistics().congestionEvents()));
	if (queueLabel_) queueLabel_->setText(QString::number(simulation->statistics().maxQueueLength()));
	double avgTravel = simulation->statistics().averageTravelTime();
	if (avgTravelLabel_) {
		if (avgTravel > 0.0) {
			int minutes = static_cast<int>(avgTravel) / 60;
			int seconds = static_cast<int>(avgTravel) % 60;
			avgTravelLabel_->setText(QStringLiteral("%1分%2秒").arg(minutes).arg(seconds, 2, 10, QChar('0')));
		} else {
			avgTravelLabel_->setText(QStringLiteral("--"));
		}
	}
}

void VisualizationWidget::updatePlotsFromSimulation(const Simulation &simulation)
{
	if (simulation.currentTime() < lastTime_) {
		clearHistory();
	}
	lastTime_ = simulation.currentTime();

	const int activePassengers = static_cast<int>(simulation.passengers().size())
		- simulation.statistics().completedPassengers()
		- simulation.statistics().timedOutPassengers();

	timeHistory_.append(static_cast<double>(simulation.currentTime()));
	activeHistory_.append(static_cast<double>(activePassengers));
	completedHistory_.append(static_cast<double>(simulation.statistics().completedPassengers()));
	timeoutHistory_.append(static_cast<double>(simulation.statistics().timedOutPassengers()));

	const int maximumSamples = 240;
	while (timeHistory_.size() > maximumSamples) {
		timeHistory_.removeFirst();
		activeHistory_.removeFirst();
		completedHistory_.removeFirst();
		timeoutHistory_.removeFirst();
	}

	refreshStatisticsPlot();

	for (auto &node : renderedNodes_) {
		node.currentCount = nodePassengerCount(node.id, simulation);
		node.density = nodeDensity(node.id, simulation);
	}
	refreshNodeList();
}

void VisualizationWidget::setSimulation(const Simulation &simulation)
{
	refreshSummary(&simulation);
	appendEvents(simulation);
	updatePlotsFromSimulation(simulation);
	rebuildTopologyPlot(&simulation);
	rebuildHeatmapPlot(&simulation);
	refreshDataAnalytics(simulation);
	refreshPassengerPanel(simulation);
}

void VisualizationWidget::refreshDataAnalytics(const Simulation &simulation)
{
	if (!analyticsTable_) return;

	int currentTime = simulation.currentTime();
	int completed = simulation.statistics().completedPassengers();
	if (currentTime - lastTimeForThroughput_ >= 60) {
		int delta = completed - lastCompletedForThroughput_;
		double elapsed = static_cast<double>(currentTime - lastTimeForThroughput_) / 60.0;
		currentThroughput_ = (elapsed > 0.0) ? delta / elapsed : 0.0;
		lastCompletedForThroughput_ = completed;
		lastTimeForThroughput_ = currentTime;
	}

	if (analyticsThroughput_)
		analyticsThroughput_->setText(QStringLiteral("%1 人").arg(static_cast<int>(currentThroughput_)));

	double avgTravel = simulation.statistics().averageTravelTime();
	if (analyticsAvgTravel_) {
		if (avgTravel > 0.0) {
			int minutes = static_cast<int>(avgTravel) / 60;
			int seconds = static_cast<int>(avgTravel) % 60;
			analyticsAvgTravel_->setText(QStringLiteral("%1分%2秒").arg(minutes).arg(seconds, 2, 10, QChar('0')));
		} else {
			analyticsAvgTravel_->setText(QStringLiteral("--"));
		}
	}

	if (analyticsMaxQueue_)
		analyticsMaxQueue_->setText(QString::number(simulation.statistics().maxQueueLength()));

	const auto &congestionMap = simulation.statistics().congestionCountByNode();
	if (analyticsCongestionNode_) {
		double maxDensity = 0.0;
		std::string maxDensityNode;
		for (const auto &node : renderedNodes_) {
			if (node.density > maxDensity) {
				maxDensity = node.density;
				maxDensityNode = node.name;
			}
		}
		if (maxDensity > 0.01) {
			QString label = QStringLiteral("%1 (%2%)")
				.arg(QString::fromStdString(maxDensityNode))
				.arg(maxDensity * 100.0, 0, 'f', 0);
			analyticsCongestionNode_->setText(label);
			if (maxDensity > 0.7) {
				analyticsCongestionNode_->setStyleSheet(QStringLiteral("font-size:12px;font-weight:bold;color:#C43D3D;border:none;"));
			} else if (maxDensity > 0.4) {
				analyticsCongestionNode_->setStyleSheet(QStringLiteral("font-size:12px;font-weight:bold;color:#D4953A;border:none;"));
			} else {
				analyticsCongestionNode_->setStyleSheet(QStringLiteral("font-size:12px;font-weight:bold;color:#4A9C8C;border:none;"));
			}
		} else {
			analyticsCongestionNode_->setText(QStringLiteral("无"));
			analyticsCongestionNode_->setStyleSheet(QStringLiteral("font-size:14px;font-weight:bold;color:#4A9C8C;border:none;"));
		}
	}

	struct NodeStat {
		std::string name;
		std::string type;
		int count;
		double capacity;
		double density;
	};
	std::vector<NodeStat> stats;
	for (const auto &node : renderedNodes_) {
		stats.push_back({node.name, node.type, node.currentCount, node.capacity, node.density});
	}
	std::sort(stats.begin(), stats.end(), [](const NodeStat &a, const NodeStat &b) {
		return a.density > b.density;
	});

	int rowCount = std::min(static_cast<int>(stats.size()), 20);
	analyticsTable_->setRowCount(rowCount);
	for (int i = 0; i < rowCount; ++i) {
		const auto &s = stats[i];
		analyticsTable_->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(s.name)));
		analyticsTable_->setItem(i, 1, new QTableWidgetItem(nodeTypeToChinese(s.type)));
		analyticsTable_->setItem(i, 2, new QTableWidgetItem(QString::number(s.count)));
		analyticsTable_->setItem(i, 3, new QTableWidgetItem(QString::number(static_cast<int>(s.capacity))));

		QString densityStr = QStringLiteral("%1%").arg(s.density * 100.0, 0, 'f', 1);
		auto *densityItem = new QTableWidgetItem(densityStr);
		if (s.density > 0.7) {
			densityItem->setForeground(QColor("#C43D3D"));
			densityItem->setFont(QFont(QString(), -1, QFont::Bold));
		} else if (s.density > 0.4) {
			densityItem->setForeground(QColor("#D4953A"));
		}
		analyticsTable_->setItem(i, 4, densityItem);
	}
}

void VisualizationWidget::refreshPassengerPanel(const Simulation &simulation)
{
	if (!passengerTable_) return;

	const auto &passengers = simulation.passengers();

	std::vector<const Passenger *> activePassengers;
	activePassengers.reserve(passengers.size());
	for (const auto &p : passengers) {
		if (p.state != PassengerState::Finished) {
			activePassengers.push_back(&p);
		}
	}

	int rowCount = std::min(static_cast<int>(activePassengers.size()), 50);
	passengerTable_->setRowCount(rowCount);

	auto stateToString = [this](const Passenger &p) -> QString {
		if (p.onEdge) {
			return QStringLiteral("通行");
		}
		switch (p.state) {
		case PassengerState::Enter:    return QStringLiteral("进站");
		case PassengerState::Security: return QStringLiteral("安检");
		case PassengerState::Ticket:   return QStringLiteral("购票");
		case PassengerState::Wait: {
			auto it = graph_.nodes().find(p.currentNode);
			if (it != graph_.nodes().end()) {
				const auto &n = it->second;
				if (n.type == "platform")       return QStringLiteral("候车");
				if (n.type == "gate")           return QStringLiteral("闸机等待");
				if (n.type == "corridor")       return QStringLiteral("通道等待");
				if (n.type == "stairs")         return QStringLiteral("楼梯等待");
				if (n.type == "escalator")      return QStringLiteral("扶梯等待");
				if (n.type == "hall")           return QStringLiteral("大厅等待");
				if (n.type == "waiting")        return QStringLiteral("候车区");
			}
			return QStringLiteral("等待");
		}
		case PassengerState::Board:    return QStringLiteral("乘车");
		case PassengerState::Exit:     return QStringLiteral("出站");
		case PassengerState::Finished: return QStringLiteral("完成");
		}
		return QStringLiteral("未知");
	};

	auto stateToColor = [](const Passenger &p) -> QColor {
		if (p.onEdge) return QColor("#7B8D6E");
		switch (p.state) {
		case PassengerState::Enter:    return QColor("#C43D3D");
		case PassengerState::Security: return QColor("#D4953A");
		case PassengerState::Ticket:   return QColor("#8B5E7A");
		case PassengerState::Wait:     return QColor("#4A9C8C");
		case PassengerState::Board:    return QColor("#22558B");
		case PassengerState::Exit:     return QColor("#2E8B57");
		default: return QColor("#3D322C");
		}
	};

	for (int i = 0; i < rowCount; ++i) {
		const auto &p = *activePassengers[i];
		passengerTable_->setItem(i, 0, new QTableWidgetItem(QString::number(p.id)));

		auto *stateItem = new QTableWidgetItem(stateToString(p));
		stateItem->setForeground(stateToColor(p));
		passengerTable_->setItem(i, 1, stateItem);

		passengerTable_->setItem(i, 2, new QTableWidgetItem(QStringLiteral("%1 m/s").arg(p.speed, 0, 'f', 2)));
		passengerTable_->setItem(i, 3, new QTableWidgetItem(QStringLiteral("%1 s").arg(static_cast<int>(p.patience))));
		passengerTable_->setItem(i, 4, new QTableWidgetItem(QStringLiteral("%1%").arg(p.familiarity * 100.0, 0, 'f', 0)));

		auto nodeDisplayName = [this](const std::string &nodeId) -> QString {
			auto it = graph_.nodes().find(nodeId);
			if (it != graph_.nodes().end() && !it->second.name.empty())
				return QString::fromStdString(it->second.name);
			return QString::fromStdString(nodeId);
		};
		QString location = p.onEdge
			? QStringLiteral("%1→%2").arg(nodeDisplayName(p.edgeFrom), nodeDisplayName(p.edgeTo))
			: nodeDisplayName(p.currentNode);
		passengerTable_->setItem(i, 5, new QTableWidgetItem(location));

		auto *waitItem = new QTableWidgetItem(QStringLiteral("%1 s").arg(static_cast<int>(p.waitedSeconds)));
		if (p.waitedSeconds > p.patience * 0.7) {
			waitItem->setForeground(QColor("#C43D3D"));
			waitItem->setFont(QFont(QString(), -1, QFont::Bold));
		}
		passengerTable_->setItem(i, 6, waitItem);
	}
}